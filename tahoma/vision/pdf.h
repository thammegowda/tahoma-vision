#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
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

struct PdfColor {
    uint8_t red{};
    uint8_t green{};
    uint8_t blue{};
    uint8_t alpha{255};
};

struct PdfRectangle {
    double left{};
    double bottom{};
    double right{};
    double top{};
};

struct PdfMatrix {
    double a{1.0};
    double b{};
    double c{};
    double d{1.0};
    double e{};
    double f{};
};

enum class PdfPageObjectType : uint8_t {
    Unknown,
    Text,
    Path,
    Image,
    Shading,
    Form,
};

struct PdfTextObject {
    std::string text;
    std::string font_name;
    double font_size_points{};
    std::optional<PdfColor> fill_color;
};

struct PdfPageObject {
    size_t index{};
    PdfPageObjectType type{PdfPageObjectType::Unknown};
    std::optional<PdfRectangle> bounds;
    std::optional<PdfMatrix> matrix;
    std::optional<PdfTextObject> text;
};

struct PdfPage {
    size_t index{};
    double width_points{};
    double height_points{};
    std::vector<PdfPageObject> objects;
};

struct PdfTextOptions {
    std::string font_name{"Helvetica"};
    double font_size_points{12.0};
    PdfColor fill_color{};
    PdfMatrix matrix;
};

struct PdfTextLayer {
    std::vector<uint8_t> background_pdf;
    std::string text_svg;
};

struct PdfTextLayerOptions {
    size_t max_text_elements{100000};
    size_t max_svg_bytes{16777216};
};

class PdfDocument {
public:
    [[nodiscard]] static PdfDocument open(
        std::span<const uint8_t> encoded, std::string_view password = {});

    PdfDocument(PdfDocument&&) noexcept;
    PdfDocument& operator=(PdfDocument&&) noexcept;
    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;
    ~PdfDocument();

    [[nodiscard]] size_t page_count() const;
    [[nodiscard]] PdfPage page(size_t page_index) const;
    [[nodiscard]] Image render_page(
        const PdfRenderOptions& options = {}) const;

    void remove_object(size_t page_index, size_t object_index);
    void replace_text(
        size_t page_index, size_t object_index, std::string_view text);
    [[nodiscard]] size_t add_text(
        size_t page_index, std::string_view text,
        const PdfTextOptions& options = {});

    [[nodiscard]] std::vector<uint8_t> save() const;

private:
    class Impl;

    explicit PdfDocument(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] PdfTextLayer split_pdf_text_layer(
    std::span<const uint8_t> encoded, std::string_view password = {},
    const PdfTextLayerOptions& options = {});
[[nodiscard]] std::vector<uint8_t> combine_pdf_text_layer(
    std::span<const uint8_t> background_pdf, std::string_view text_svg,
    std::string_view password = {},
    const PdfTextLayerOptions& options = {});

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
