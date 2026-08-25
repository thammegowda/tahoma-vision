#include <tahoma/vision/codec.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>

#include <pigzpp/png.h>

namespace tahoma::vision {
namespace {

struct PngHeader {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
};

uint32_t read_be32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

PixelFormat pixel_format(uint8_t channels) {
    switch (channels) {
        case 1: return PixelFormat::Gray8;
        case 2: return PixelFormat::GrayAlpha8;
        case 3: return PixelFormat::RGB8;
        case 4: return PixelFormat::RGBA8;
        default:
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "PNG channel count is not supported"};
    }
}

PngHeader preflight_png(
        std::span<const uint8_t> encoded, const DecodeOptions& options) {
    constexpr std::array<uint8_t, 4> ihdr{'I', 'H', 'D', 'R'};
    if (encoded.size() < 33 || read_be32(encoded.data() + 8) != 13 ||
        !std::equal(ihdr.begin(), ihdr.end(), encoded.begin() + 12)) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PNG has no valid IHDR"};
    }
    const auto width = read_be32(encoded.data() + 16);
    const auto height = read_be32(encoded.data() + 20);
    const auto bit_depth = encoded[24];
    const auto color_type = encoded[25];
    const auto compression = encoded[26];
    const auto filter = encoded[27];
    const auto interlace = encoded[28];
    uint8_t channels = 0;
    switch (color_type) {
        case 0: channels = 1; break;
        case 4: channels = 2; break;
        case 2: channels = 3; break;
        case 6: channels = 4; break;
        default:
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "PNG color type is not supported"};
    }
    if (bit_depth != 8 || compression != 0 || filter != 0 || interlace != 0) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "PNG must be non-interlaced 8-bit input"};
    }
    const auto pixel_count = static_cast<uint64_t>(width) * height;
    if (width == 0 || height == 0 || pixel_count > options.max_pixels) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "decoded PNG exceeds configured pixel limits"};
    }
    try {
        const auto source_bytes = required_buffer_bytes(
            width, height, pixel_format(channels),
            static_cast<size_t>(width) * channels);
        const auto output_bytes = required_buffer_bytes(
            width, height, options.output_format,
            packed_row_bytes(width, options.output_format));
        if (source_bytes > options.max_decoded_bytes ||
            output_bytes > options.max_decoded_bytes) {
            throw CodecError{
                CodecErrorCode::ResourceLimit,
                "decoded PNG exceeds configured byte limits"};
        }
    } catch (const CodecError&) {
        throw;
    } catch (const std::exception&) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "decoded PNG geometry overflows host storage"};
    }
    return {width, height, channels};
}

uint8_t gray(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint8_t>(
        (299U * red + 587U * green + 114U * blue + 500U) / 1000U);
}

Image convert_image(
        const pigzpp::png::Image& source, PixelFormat output_format) {
    auto result = make_image(source.width, source.height, output_format);
    const auto target_channels = channel_count(output_format);
    const auto pixels = static_cast<size_t>(source.width) * source.height;
    for (size_t index = 0; index < pixels; ++index) {
        const auto* input = source.pixels.data() + index * source.channels;
        const auto source_gray = source.channels <= 2;
        const auto red = input[0];
        const auto green = source_gray ? red : input[1];
        const auto blue = source_gray ? red : input[2];
        const auto alpha = source.channels == 2 ? input[1] :
            source.channels == 4 ? input[3] : 255;
        auto* output = result.pixels.data() + index * target_channels;
        switch (output_format) {
            case PixelFormat::Gray8:
                output[0] = source_gray ? red : gray(red, green, blue);
                break;
            case PixelFormat::GrayAlpha8:
                output[0] = source_gray ? red : gray(red, green, blue);
                output[1] = alpha;
                break;
            case PixelFormat::RGB8:
                output[0] = red;
                output[1] = green;
                output[2] = blue;
                break;
            case PixelFormat::RGBA8:
                output[0] = red;
                output[1] = green;
                output[2] = blue;
                output[3] = alpha;
                break;
        }
    }
    return result;
}

pigzpp::png::Preset pigzpp_preset(PngPreset preset) {
    switch (preset) {
        case PngPreset::Fast: return pigzpp::png::Preset::Fast;
        case PngPreset::Balanced: return pigzpp::png::Preset::Balanced;
        case PngPreset::Small: return pigzpp::png::Preset::Small;
    }
    throw CodecError{
        CodecErrorCode::UnsupportedFeature, "unsupported PNG preset"};
}

}  // namespace

Image decode_png(
        std::span<const uint8_t> encoded, const DecodeOptions& options) {
    const auto header = preflight_png(encoded, options);
    try {
        const auto decoded = pigzpp::png::decode(
            encoded.data(), encoded.size());
        if (decoded.width != header.width || decoded.height != header.height ||
            decoded.channels != header.channels) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "PNG dimensions changed while decoding"};
        }
        return convert_image(decoded, options.output_format);
    } catch (const CodecError&) {
        throw;
    } catch (const std::exception& error) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PNG decode failed: " + std::string{error.what()}};
    }
}

std::vector<uint8_t> encode_png(
        ImageView image, const PngEncodeOptions& options) {
    validate(image);
    if (options.threads > 1) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "parallel PNG encoding is not available yet"};
    }
    if (image.width > std::numeric_limits<uint32_t>::max() ||
        image.height > std::numeric_limits<uint32_t>::max()) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "PNG dimensions exceed backend limits"};
    }
    const auto packed_stride = packed_row_bytes(image.width, image.format);
    const auto pixel_bytes = packed_stride * static_cast<size_t>(image.height);
    std::vector<uint8_t> packed;
    const uint8_t* pixels = image.pixels.data();
    if (image.row_stride != packed_stride) {
        packed.resize(pixel_bytes);
        for (int64_t row = 0; row < image.height; ++row) {
            std::memcpy(
                packed.data() + static_cast<size_t>(row) * packed_stride,
                image.pixels.data() + static_cast<size_t>(row) * image.row_stride,
                packed_stride);
        }
        pixels = packed.data();
    }
    try {
        return pigzpp::png::encode_buffer(
            pixels, pixel_bytes, static_cast<uint32_t>(image.width),
            static_cast<uint32_t>(image.height),
            static_cast<uint8_t>(channel_count(image.format)),
            pigzpp::png::preset_options(pigzpp_preset(options.preset)));
    } catch (const std::invalid_argument& error) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "PNG encode failed: " + std::string{error.what()}};
    } catch (const std::exception& error) {
        throw CodecError{
            CodecErrorCode::Backend,
            "PNG encode failed: " + std::string{error.what()}};
    }
}

}  // namespace tahoma::vision
