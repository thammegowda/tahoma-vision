#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tahoma::vision {

enum class PixelFormat : uint8_t {
    Gray8,
    GrayAlpha8,
    RGB8,
    RGBA8,
};

[[nodiscard]] size_t channel_count(PixelFormat format) noexcept;

struct ImageView {
    int64_t width{};
    int64_t height{};
    PixelFormat format{PixelFormat::RGB8};
    size_t row_stride{};
    std::span<const uint8_t> pixels;
};

struct Image {
    int64_t width{};
    int64_t height{};
    PixelFormat format{PixelFormat::RGB8};
    size_t row_stride{};
    std::vector<uint8_t> pixels;

    [[nodiscard]] ImageView view() const noexcept;
};

[[nodiscard]] size_t packed_row_bytes(
    int64_t width, PixelFormat format);
[[nodiscard]] size_t required_buffer_bytes(
    int64_t width, int64_t height, PixelFormat format, size_t row_stride);
void validate(ImageView image);
void validate(const Image& image);

[[nodiscard]] Image make_image(
    int64_t width, int64_t height,
    PixelFormat format = PixelFormat::RGB8);
[[nodiscard]] Image crop(
    ImageView image, int64_t x, int64_t y,
    int64_t width, int64_t height);

}  // namespace tahoma::vision
