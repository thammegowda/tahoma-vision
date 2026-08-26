#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace tahoma::vision {

enum class Format : uint8_t {
    Unknown,
    Png,
    Jpeg,
    Ppm,
    Svg,
    Pdf,
};

[[nodiscard]] Format detect_format(std::span<const uint8_t> encoded) noexcept;
[[nodiscard]] std::string_view name_of(Format format) noexcept;

}  // namespace tahoma::vision
