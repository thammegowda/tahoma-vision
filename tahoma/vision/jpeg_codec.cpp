#include <tahoma/vision/codec.h>

#include <array>
#include <csetjmp>
#include <cstdlib>
#include <limits>
#include <string>

#include <jpeglib.h>

namespace tahoma::vision {
namespace {

struct JpegError {
    jpeg_error_mgr base;
    std::jmp_buf jump;
    std::array<char, JMSG_LENGTH_MAX> message{};
};

void jpeg_failure(j_common_ptr context) {
    auto* error = reinterpret_cast<JpegError*>(context->err);
    context->err->format_message(context, error->message.data());
    std::longjmp(error->jump, 1);
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
    size_t byte_count = 0;
    int64_t width = 0;
    int64_t height = 0;
    size_t row_stride = 0;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&decoder);
        std::free(pixels);
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "JPEG decode failed: " + std::string{error.message.data()}};
    }
    jpeg_create_decompress(&decoder);
    jpeg_mem_src(
        &decoder, encoded.data(), static_cast<unsigned long>(encoded.size()));
    jpeg_read_header(&decoder, TRUE);
    decoder.out_color_space = options.output_format == PixelFormat::Gray8
        ? JCS_GRAYSCALE : JCS_RGB;
    jpeg_start_decompress(&decoder);
    width = static_cast<int64_t>(decoder.output_width);
    height = static_cast<int64_t>(decoder.output_height);
    row_stride = packed_row_bytes(width, options.output_format);
    byte_count = required_buffer_bytes(
        width, height, options.output_format, row_stride);
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
    while (decoder.output_scanline < decoder.output_height) {
        auto* row = pixels +
            static_cast<size_t>(decoder.output_scanline) * row_stride;
        JSAMPROW rows[]{row};
        jpeg_read_scanlines(&decoder, rows, 1);
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    Image result{
        .width = width,
        .height = height,
        .format = options.output_format,
        .row_stride = row_stride,
        .pixels = std::vector<uint8_t>(pixels, pixels + byte_count),
    };
    std::free(pixels);
    return result;
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
