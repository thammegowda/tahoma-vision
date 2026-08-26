#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <tahoma/vision/image.h>

namespace tahoma::vision {

struct PdfRenderOptions {
    size_t page_index{};
    double dpi{144.0};
    PixelFormat output_format{PixelFormat::RGB8};
    uint64_t max_pixels{268435456};
    uint64_t max_decoded_bytes{1073741824};
    std::string password;
};

struct PdfEncodeOptions {
    double dpi{72.0};
};

[[nodiscard]] size_t pdf_page_count(
    std::span<const uint8_t> encoded, std::string_view password = {});
[[nodiscard]] Image render_pdf_page(
    std::span<const uint8_t> encoded,
    const PdfRenderOptions& options = {});
[[nodiscard]] Image render_pdf_page(
    const std::filesystem::path& path,
    const PdfRenderOptions& options = {});

[[nodiscard]] std::vector<uint8_t> encode_pdf(
    ImageView image, const PdfEncodeOptions& options = {});
void save_pdf(
    const std::filesystem::path& path, ImageView image,
    const PdfEncodeOptions& options = {});

}  // namespace tahoma::vision
