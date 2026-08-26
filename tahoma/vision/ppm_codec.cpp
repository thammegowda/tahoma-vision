#include <tahoma/vision/codec.h>

#include <algorithm>
#include <string>

namespace tahoma::vision {
namespace {

size_t skip_space(std::span<const uint8_t> bytes, size_t position) {
    while (position < bytes.size()) {
        if (bytes[position] == '#') {
            while (position < bytes.size() && bytes[position] != '\n') ++position;
        } else if (std::isspace(bytes[position])) {
            ++position;
        } else {
            break;
        }
    }
    return position;
}

int64_t integer(std::span<const uint8_t> bytes, size_t& position) {
    position = skip_space(bytes, position);
    if (position >= bytes.size() || !std::isdigit(bytes[position])) {
        throw CodecError{CodecErrorCode::MalformedInput, "malformed PPM header"};
    }
    int64_t value = 0;
    while (position < bytes.size() && std::isdigit(bytes[position])) {
        value = value * 10 + bytes[position++] - '0';
    }
    return value;
}

}  // namespace

Image decode_ppm(std::span<const uint8_t> bytes, const DecodeOptions& options) {
    const auto source_channels = bytes[1] == '6' ? int64_t{3} : int64_t{1};
    size_t position = 2;
    const auto width = integer(bytes, position);
    const auto height = integer(bytes, position);
    const auto maximum = integer(bytes, position);
    if (width <= 0 || height <= 0 || maximum != 255) {
        throw CodecError{CodecErrorCode::MalformedInput, "PPM requires positive geometry and max value 255"};
    }
    position = skip_space(bytes, position);
    const auto source_size = static_cast<size_t>(width * height * source_channels);
    if (source_size > bytes.size() - position) {
        throw CodecError{CodecErrorCode::MalformedInput, "truncated PPM pixels"};
    }
    auto result = make_image(width, height, options.output_format);
    const auto output_channels = channel_count(options.output_format);
    for (int64_t pixel = 0; pixel < width * height; ++pixel) {
        const auto source = position + static_cast<size_t>(pixel * source_channels);
        const auto target = static_cast<size_t>(pixel) * output_channels;
        for (size_t channel = 0; channel < output_channels; ++channel) {
            if (channel == 3 || (channel == 1 && output_channels == 2)) {
                result.pixels[target + channel] = 255;
            } else {
                result.pixels[target + channel] = bytes[source +
                    (source_channels == 1 ? 0 : std::min<size_t>(channel, 2))];
            }
        }
    }
    return result;
}

std::vector<uint8_t> encode_ppm(ImageView image) {
    validate(image);
    const auto channels = channel_count(image.format);
    const std::string header = "P6\n" + std::to_string(image.width) + " " +
        std::to_string(image.height) + "\n255\n";
    std::vector<uint8_t> result(header.begin(), header.end());
    result.reserve(result.size() + static_cast<size_t>(image.width * image.height * 3));
    for (int64_t y = 0; y < image.height; ++y) {
        for (int64_t x = 0; x < image.width; ++x) {
            const auto source = static_cast<size_t>(y) * image.row_stride +
                static_cast<size_t>(x) * channels;
            for (size_t channel = 0; channel < 3; ++channel) {
                result.push_back(image.pixels[source +
                    (image.format == PixelFormat::Gray8 || image.format == PixelFormat::GrayAlpha8
                        ? 0 : channel)]);
            }
        }
    }
    return result;
}

}  // namespace tahoma::vision