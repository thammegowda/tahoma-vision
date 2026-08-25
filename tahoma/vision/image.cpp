#include <tahoma/vision/image.h>

#include <limits>
#include <stdexcept>

namespace tahoma::vision {
namespace {

size_t checked_multiply(size_t left, size_t right, const char* message) {
    if (right != 0 && left > std::numeric_limits<size_t>::max() / right) {
        throw std::overflow_error(message);
    }
    return left * right;
}

}  // namespace

size_t channel_count(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::Gray8: return 1;
        case PixelFormat::GrayAlpha8: return 2;
        case PixelFormat::RGB8: return 3;
        case PixelFormat::RGBA8: return 4;
    }
    return 0;
}

ImageView Image::view() const noexcept {
    return {width, height, format, row_stride, pixels};
}

size_t packed_row_bytes(int64_t width, PixelFormat format) {
    if (width <= 0) {
        throw std::invalid_argument("image width must be positive");
    }
    return checked_multiply(
        static_cast<size_t>(width), channel_count(format),
        "image row byte count overflows size_t");
}

size_t required_buffer_bytes(
        int64_t width, int64_t height, PixelFormat format,
        size_t row_stride) {
    if (height <= 0) {
        throw std::invalid_argument("image height must be positive");
    }
    const auto row_bytes = packed_row_bytes(width, format);
    if (row_stride < row_bytes) {
        throw std::invalid_argument(
            "image row stride is smaller than its packed row size");
    }
    const auto preceding_rows = checked_multiply(
        static_cast<size_t>(height - 1), row_stride,
        "image buffer byte count overflows size_t");
    if (preceding_rows > std::numeric_limits<size_t>::max() - row_bytes) {
        throw std::overflow_error("image buffer byte count overflows size_t");
    }
    return preceding_rows + row_bytes;
}

void validate(ImageView image) {
    const auto required = required_buffer_bytes(
        image.width, image.height, image.format, image.row_stride);
    if (image.pixels.size() < required) {
        throw std::invalid_argument(
            "image pixel buffer is smaller than its geometry and stride");
    }
}

void validate(const Image& image) {
    validate(image.view());
}

Image make_image(int64_t width, int64_t height, PixelFormat format) {
    const auto row_stride = packed_row_bytes(width, format);
    const auto size = required_buffer_bytes(
        width, height, format, row_stride);
    return {width, height, format, row_stride, std::vector<uint8_t>(size)};
}

}  // namespace tahoma::vision
