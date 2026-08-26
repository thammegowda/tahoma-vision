#include <tahoma/vision/codec.h>

#include <algorithm>
#include <array>
#include <csetjmp>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>

#include <jerror.h>
#include <jpeglib.h>

namespace tahoma::vision {
namespace {

struct JpegError {
    jpeg_error_mgr base;
    std::jmp_buf jump;
    std::array<char, JMSG_LENGTH_MAX> message{};
};

struct JpegSource {
    jpeg_source_mgr base;
};

void jpeg_failure(j_common_ptr context) {
    auto* error = reinterpret_cast<JpegError*>(context->err);
    context->err->format_message(context, error->message.data());
    std::longjmp(error->jump, 1);
}

void init_source(j_decompress_ptr) {}

boolean fill_input_buffer(j_decompress_ptr decoder) {
    ERREXIT(decoder, JERR_INPUT_EOF);
    return FALSE;
}

void skip_input_data(j_decompress_ptr decoder, long count) {
    if (count <= 0) return;
    if (static_cast<size_t>(count) > decoder->src->bytes_in_buffer) {
        decoder->src->next_input_byte += decoder->src->bytes_in_buffer;
        decoder->src->bytes_in_buffer = 0;
        fill_input_buffer(decoder);
        return;
    }
    decoder->src->next_input_byte += count;
    decoder->src->bytes_in_buffer -= static_cast<size_t>(count);
}

void term_source(j_decompress_ptr) {}

void set_source(j_decompress_ptr decoder, std::span<const uint8_t> encoded) {
    if (decoder->src == nullptr) {
        decoder->src = reinterpret_cast<jpeg_source_mgr*>(
            (*decoder->mem->alloc_small)(
                reinterpret_cast<j_common_ptr>(decoder), JPOOL_PERMANENT,
                sizeof(JpegSource)));
    }
    decoder->src->init_source = init_source;
    decoder->src->fill_input_buffer = fill_input_buffer;
    decoder->src->skip_input_data = skip_input_data;
    decoder->src->resync_to_restart = jpeg_resync_to_restart;
    decoder->src->term_source = term_source;
    decoder->src->bytes_in_buffer = encoded.size();
    decoder->src->next_input_byte = encoded.data();
}

uint16_t exif_u16(
        std::span<const uint8_t> data, size_t offset, bool little_endian) {
    if (offset > data.size() || data.size() - offset < 2) return 0;
    return little_endian
        ? static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8))
        : static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
}

uint32_t exif_u32(
        std::span<const uint8_t> data, size_t offset, bool little_endian) {
    if (offset > data.size() || data.size() - offset < 4) return 0;
    if (little_endian) {
        return static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8) |
            (static_cast<uint32_t>(data[offset + 2]) << 16) |
            (static_cast<uint32_t>(data[offset + 3]) << 24);
    }
    return (static_cast<uint32_t>(data[offset]) << 24) |
        (static_cast<uint32_t>(data[offset + 1]) << 16) |
        (static_cast<uint32_t>(data[offset + 2]) << 8) |
        static_cast<uint32_t>(data[offset + 3]);
}

int exif_orientation(j_decompress_ptr decoder) {
    constexpr std::array<uint8_t, 6> signature{'E', 'x', 'i', 'f', 0, 0};
    for (auto* marker = decoder->marker_list;
         marker != nullptr; marker = marker->next) {
        if (marker->marker != JPEG_APP0 + 1 ||
            marker->data_length < signature.size() ||
            !std::equal(signature.begin(), signature.end(), marker->data)) {
            continue;
        }
        const std::span<const uint8_t> tiff{
            marker->data + signature.size(),
            marker->data_length - signature.size()};
        if (tiff.size() < 8) return 1;
        const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
        if (!little_endian && !(tiff[0] == 'M' && tiff[1] == 'M')) return 1;
        if (exif_u16(tiff, 2, little_endian) != 42) return 1;
        const auto directory = static_cast<size_t>(
            exif_u32(tiff, 4, little_endian));
        if (directory > tiff.size() || tiff.size() - directory < 2) return 1;
        const auto count = exif_u16(tiff, directory, little_endian);
        size_t entry = directory + 2;
        for (uint16_t index = 0; index < count; ++index, entry += 12) {
            if (entry > tiff.size() || tiff.size() - entry < 12) return 1;
            if (exif_u16(tiff, entry, little_endian) != 0x0112) continue;
            if (exif_u16(tiff, entry + 2, little_endian) != 3 ||
                exif_u32(tiff, entry + 4, little_endian) != 1) {
                return 1;
            }
            const auto value = exif_u16(tiff, entry + 8, little_endian);
            return value >= 1 && value <= 8 ? value : 1;
        }
    }
    return 1;
}

Image orient_image(Image source, int orientation) {
    if (orientation <= 1 || orientation > 8) return source;
    const bool swaps_axes = orientation >= 5;
    auto result = make_image(
        swaps_axes ? source.height : source.width,
        swaps_axes ? source.width : source.height,
        source.format);
    const auto channels = channel_count(source.format);
    for (int64_t y = 0; y < source.height; ++y) {
        for (int64_t x = 0; x < source.width; ++x) {
            int64_t target_x = x;
            int64_t target_y = y;
            switch (orientation) {
                case 2: target_x = source.width - 1 - x; break;
                case 3:
                    target_x = source.width - 1 - x;
                    target_y = source.height - 1 - y;
                    break;
                case 4: target_y = source.height - 1 - y; break;
                case 5: target_x = y; target_y = x; break;
                case 6: target_x = source.height - 1 - y; target_y = x; break;
                case 7:
                    target_x = source.height - 1 - y;
                    target_y = source.width - 1 - x;
                    break;
                case 8: target_x = y; target_y = source.width - 1 - x; break;
                default: break;
            }
            const auto source_offset = static_cast<size_t>(
                y * source.row_stride + x * channels);
            const auto target_offset = static_cast<size_t>(
                target_y * result.row_stride + target_x * channels);
            std::copy_n(
                source.pixels.begin() + source_offset, channels,
                result.pixels.begin() + target_offset);
        }
    }
    return result;
}

uint8_t cmyk_component(uint8_t black, uint8_t component) {
    const auto product = static_cast<int>(black) * component + 128;
    const auto normalized = ((product >> 8) + product) >> 8;
    return static_cast<uint8_t>(
        std::clamp(static_cast<int>(black) - normalized, 0, 255));
}

uint8_t rgb_gray(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint8_t>(
        (red * 19595U + green * 38470U + blue * 7471U + 0x8000U) >> 16);
}

void convert_cmyk_row(
        const uint8_t* input, uint8_t* output, size_t width,
    PixelFormat format, bool inverted) {
    for (size_t column = 0; column < width; ++column) {
        const auto cyan = input[column * 4];
        const auto magenta = input[column * 4 + 1];
        const auto yellow = input[column * 4 + 2];
        const auto black = input[column * 4 + 3];
        const auto direct_component = [black](uint8_t component) {
            return static_cast<uint8_t>(
                ((255U - component) * (255U - black) + 127U) / 255U);
        };
        const auto red = inverted
            ? cmyk_component(black, 255 - cyan) : direct_component(cyan);
        const auto green = inverted
            ? cmyk_component(black, 255 - magenta) : direct_component(magenta);
        const auto blue = inverted
            ? cmyk_component(black, 255 - yellow) : direct_component(yellow);
        if (format == PixelFormat::Gray8) {
            output[column] = rgb_gray(red, green, blue);
        } else {
            output[column * 3] = red;
            output[column * 3 + 1] = green;
            output[column * 3 + 2] = blue;
        }
    }
}

void require_jpeg_format(PixelFormat format) {
    if (format != PixelFormat::Gray8 && format != PixelFormat::RGB8) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "JPEG supports only Gray8 and RGB8 images"};
    }
}

}  // namespace

Image decode_jpeg(
        std::span<const uint8_t> encoded, const DecodeOptions& options) {
    require_jpeg_format(options.output_format);
    jpeg_decompress_struct decoder{};
    JpegError error{};
    decoder.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpeg_failure;
    uint8_t* pixels = nullptr;
    uint8_t* cmyk_row = nullptr;
    size_t byte_count = 0;
    int64_t width = 0;
    int64_t height = 0;
    size_t row_stride = 0;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&decoder);
        std::free(cmyk_row);
        std::free(pixels);
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "JPEG decode failed: " + std::string{error.message.data()}};
    }
    jpeg_create_decompress(&decoder);
    set_source(&decoder, encoded);
    jpeg_save_markers(&decoder, JPEG_APP0 + 1, 0xffff);
    jpeg_read_header(&decoder, TRUE);
    const auto orientation = options.apply_exif_orientation
        ? exif_orientation(&decoder) : 1;
    const bool convert_cmyk = decoder.jpeg_color_space == JCS_CMYK ||
        decoder.jpeg_color_space == JCS_YCCK;
    decoder.out_color_space = convert_cmyk ? JCS_CMYK :
        options.output_format == PixelFormat::Gray8 ? JCS_GRAYSCALE : JCS_RGB;
    jpeg_calc_output_dimensions(&decoder);
    width = static_cast<int64_t>(decoder.output_width);
    height = static_cast<int64_t>(decoder.output_height);
    try {
        row_stride = packed_row_bytes(width, options.output_format);
        byte_count = required_buffer_bytes(
            width, height, options.output_format, row_stride);
    } catch (const std::exception&) {
        jpeg_destroy_decompress(&decoder);
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "decoded JPEG geometry overflows host storage"};
    }
    const auto pixel_count = static_cast<uint64_t>(width) *
        static_cast<uint64_t>(height);
    if (pixel_count > options.max_pixels ||
        byte_count > options.max_decoded_bytes) {
        jpeg_destroy_decompress(&decoder);
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "decoded JPEG exceeds configured limits"};
    }
    pixels = static_cast<uint8_t*>(std::malloc(byte_count));
    if (pixels == nullptr) {
        jpeg_destroy_decompress(&decoder);
        throw std::bad_alloc();
    }
    if (convert_cmyk) {
        cmyk_row = static_cast<uint8_t*>(std::malloc(
            static_cast<size_t>(decoder.output_width) * 4));
        if (cmyk_row == nullptr) {
            jpeg_destroy_decompress(&decoder);
            std::free(pixels);
            throw std::bad_alloc();
        }
    }
    jpeg_start_decompress(&decoder);
    while (decoder.output_scanline < decoder.output_height) {
        auto* output_row = pixels +
            static_cast<size_t>(decoder.output_scanline) * row_stride;
        auto* input_row = convert_cmyk ? cmyk_row : output_row;
        JSAMPROW rows[]{input_row};
        jpeg_read_scanlines(&decoder, rows, 1);
        if (convert_cmyk) {
            convert_cmyk_row(
                cmyk_row, output_row, decoder.output_width,
                options.output_format, decoder.saw_Adobe_marker);
        }
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    std::vector<uint8_t> result_pixels;
    try {
        result_pixels.assign(pixels, pixels + byte_count);
    } catch (...) {
        std::free(cmyk_row);
        std::free(pixels);
        throw;
    }
    Image result{
        .width = width,
        .height = height,
        .format = options.output_format,
        .row_stride = row_stride,
        .pixels = std::move(result_pixels),
    };
    std::free(cmyk_row);
    std::free(pixels);
    return orient_image(std::move(result), orientation);
}

std::vector<uint8_t> encode_jpeg(
        ImageView image, const JpegEncodeOptions& options) {
    validate(image);
    require_jpeg_format(image.format);
    if (options.quality < 1 || options.quality > 100) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "JPEG quality must be between 1 and 100"};
    }
    if (image.width > std::numeric_limits<JDIMENSION>::max() ||
        image.height > std::numeric_limits<JDIMENSION>::max()) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "JPEG dimensions exceed backend limits"};
    }
    jpeg_compress_struct encoder{};
    JpegError error{};
    encoder.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpeg_failure;
    unsigned char* output = nullptr;
    unsigned long output_size = 0;
    if (setjmp(error.jump)) {
        jpeg_destroy_compress(&encoder);
        std::free(output);
        throw CodecError{
            CodecErrorCode::Backend,
            "JPEG encode failed: " + std::string{error.message.data()}};
    }
    jpeg_create_compress(&encoder);
    jpeg_mem_dest(&encoder, &output, &output_size);
    encoder.image_width = static_cast<JDIMENSION>(image.width);
    encoder.image_height = static_cast<JDIMENSION>(image.height);
    encoder.input_components = static_cast<int>(channel_count(image.format));
    encoder.in_color_space = image.format == PixelFormat::Gray8
        ? JCS_GRAYSCALE : JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, options.quality, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    while (encoder.next_scanline < encoder.image_height) {
        auto* row = const_cast<uint8_t*>(image.pixels.data()) +
            static_cast<size_t>(encoder.next_scanline) * image.row_stride;
        JSAMPROW rows[]{row};
        jpeg_write_scanlines(&encoder, rows, 1);
    }
    jpeg_finish_compress(&encoder);
    std::vector<uint8_t> result(output, output + output_size);
    jpeg_destroy_compress(&encoder);
    std::free(output);
    return result;
}

}  // namespace tahoma::vision
