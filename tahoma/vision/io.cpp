#include <tahoma/vision/io.h>

#include <algorithm>
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

}  // namespace tahoma::vision
