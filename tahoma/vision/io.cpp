#include <tahoma/vision/io.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace tahoma::vision {
namespace {

Format format_from_extension(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (extension == ".png") return Format::Png;
    if (extension == ".jpg" || extension == ".jpeg") return Format::Jpeg;
    if (extension == ".ppm" || extension == ".pgm") return Format::Ppm;
    return Format::Unknown;
}

}  // namespace

Image load(
        const std::filesystem::path& path, const DecodeOptions& options) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw CodecError{
            CodecErrorCode::Io, "failed to open image: " + path.string()};
    }
    std::vector<uint8_t> bytes{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    if (!stream.eof() && stream.fail()) {
        throw CodecError{
            CodecErrorCode::Io, "failed to read image: " + path.string()};
    }
    return decode(bytes, options);
}

void save(
        const std::filesystem::path& path, ImageView image,
        const EncodeOptions& options) {
    const auto format = options.format == Format::Unknown
        ? format_from_extension(path) : options.format;
    std::vector<uint8_t> bytes;
    if (format == Format::Png) {
        bytes = encode_png(image, options.png);
    } else if (format == Format::Jpeg) {
        bytes = encode_jpeg(image, options.jpeg);
    } else if (format == Format::Ppm) {
        bytes = encode_ppm(image);
    } else {
        throw CodecError{
            CodecErrorCode::UnsupportedFormat,
            "output extension does not select PNG or JPEG"};
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        throw CodecError{
            CodecErrorCode::Io, "failed to write image: " + path.string()};
    }
}

std::string base64_encode(std::span<const uint8_t> bytes) {
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((bytes.size() + 2) / 3) * 4);
    for (size_t offset = 0; offset < bytes.size(); offset += 3) {
        const uint32_t value = (uint32_t{bytes[offset]} << 16) |
            (offset + 1 < bytes.size() ? uint32_t{bytes[offset + 1]} << 8 : 0) |
            (offset + 2 < bytes.size() ? bytes[offset + 2] : 0);
        result.push_back(alphabet[(value >> 18) & 0x3f]);
        result.push_back(alphabet[(value >> 12) & 0x3f]);
        result.push_back(offset + 1 < bytes.size() ? alphabet[(value >> 6) & 0x3f] : '=');
        result.push_back(offset + 2 < bytes.size() ? alphabet[value & 0x3f] : '=');
    }
    return result;
}

std::vector<uint8_t> base64_decode(std::string_view input) {
    std::array<int8_t, 256> values{};
    values.fill(-1);
    constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t index = 0; index < alphabet.size(); ++index) {
        values[static_cast<uint8_t>(alphabet[index])] = static_cast<int8_t>(index);
    }
    std::vector<uint8_t> result;
    uint32_t accumulator = 0;
    int bits = 0;
    for (const char character : input) {
        if (std::isspace(static_cast<unsigned char>(character))) continue;
        if (character == '=') break;
        const auto value = values[static_cast<uint8_t>(character)];
        if (value < 0) throw std::invalid_argument("invalid base64 image payload");
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<uint8_t>(accumulator >> bits));
            accumulator &= (uint32_t{1} << bits) - 1;
        }
    }
    return result;
}

std::vector<uint8_t> resolve_image_bytes(const std::string& source) {
    if (source.empty()) throw std::invalid_argument("image source is empty");
    if (source.starts_with("data:")) {
        const auto comma = source.find(',');
        if (comma == std::string::npos ||
            source.substr(0, comma).find(";base64") == std::string::npos) {
            throw std::invalid_argument("image data URL must use base64 encoding");
        }
        return base64_decode(std::string_view{source}.substr(comma + 1));
    }
    if (source.starts_with("http://") || source.starts_with("https://")) {
        throw std::invalid_argument("HTTP image sources are not supported");
    }
    const std::string path = source.starts_with("file://")
        ? source.substr(7) : source;
    std::error_code error;
    if (std::filesystem::is_regular_file(path, error)) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("cannot open image file: " + path);
        const auto size = input.tellg();
        input.seekg(0);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        if (size > 0 && !input.read(reinterpret_cast<char*>(bytes.data()), size)) {
            throw std::runtime_error("cannot read image file: " + path);
        }
        return bytes;
    }
    return base64_decode(source);
}

}  // namespace tahoma::vision
