#include <tahoma/vision/format.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace tahoma::vision {
namespace {

template <size_t Size>
bool starts_with(
        std::span<const uint8_t> encoded,
        const std::array<uint8_t, Size>& signature) noexcept {
    return encoded.size() >= signature.size() &&
        std::equal(signature.begin(), signature.end(), encoded.begin());
}

bool is_svg(std::span<const uint8_t> encoded) noexcept {
    if (encoded.empty()) return false;
    constexpr size_t sniff_limit = 4096;
    const auto length = std::min(encoded.size(), sniff_limit);
    std::string_view text{
        reinterpret_cast<const char*>(encoded.data()), length};
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.remove_prefix(3);
    }
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return false;
    text.remove_prefix(first);

    const auto svg = text.find("<svg");
    if (svg == std::string_view::npos) return false;
    const auto declaration = text.find("<?xml");
    const auto comment = text.find("<!--");
    return svg == 0 || declaration == 0 || comment == 0;
}

}  // namespace

Format detect_format(std::span<const uint8_t> encoded) noexcept {
    static constexpr std::array<uint8_t, 8> png{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    static constexpr std::array<uint8_t, 2> jpeg{0xff, 0xd8};
    static constexpr std::array<uint8_t, 5> pdf{'%', 'P', 'D', 'F', '-'};

    if (starts_with(encoded, png)) return Format::Png;
    if (starts_with(encoded, jpeg)) return Format::Jpeg;
    if (starts_with(encoded, pdf)) return Format::Pdf;
    if (is_svg(encoded)) return Format::Svg;
    return Format::Unknown;
}

std::string_view name_of(Format format) noexcept {
    switch (format) {
        case Format::Unknown: return "unknown";
        case Format::Png: return "png";
        case Format::Jpeg: return "jpeg";
        case Format::Svg: return "svg";
        case Format::Pdf: return "pdf";
    }
    return "unknown";
}

}  // namespace tahoma::vision
