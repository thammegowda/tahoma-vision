#include <tahoma/vision/io.h>
#include <tahoma/vision/pdf.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

double mean_absolute_error(
        const tahoma::vision::Image& left,
        const tahoma::vision::Image& right) {
    require(left.width == right.width && left.height == right.height,
            "PDF round-trip geometry changed");
    require(left.pixels.size() == right.pixels.size(),
            "PDF round-trip byte count changed");
    uint64_t error = 0;
    for (size_t index = 0; index < left.pixels.size(); ++index) {
        error += static_cast<uint64_t>(std::abs(
            static_cast<int>(left.pixels[index]) - right.pixels[index]));
    }
    return static_cast<double>(error) / left.pixels.size();
}

}  // namespace

int main() {
    try {
        using namespace tahoma::vision;
        const std::filesystem::path resources{TAHOMA_VISION_TEST_RESOURCE_DIR};
        const auto source = load(resources / "pattern.png");
        const auto pdf_path = resources / "one-page.pdf";

        const auto pdf_bytes = encode_pdf(source.view(), {.dpi = 72.0});
        require(pdf_page_count(pdf_bytes) == 1, "encoded PDF page count changed");
        const auto rendered = render_pdf_page(
            pdf_bytes, {.dpi = 72.0, .output_format = PixelFormat::RGB8});
        require(mean_absolute_error(source, rendered) < 1.0,
                "RGB to PDF to RGB changed pixels");

        require(pdf_page_count(std::span<const uint8_t>{pdf_bytes}) == 1,
                "PDF byte API returned the wrong page count");
        const auto fixture_render = render_pdf_page(
            pdf_path, {.dpi = 72.0, .output_format = PixelFormat::RGB8});
        require(fixture_render.width == 64 && fixture_render.height == 48,
                "PDF fixture rendered at the wrong size");

        const auto temporary = std::filesystem::temp_directory_path();
        const auto output_pdf = temporary / "tahoma-vision-image.pdf";
        const auto output_png = temporary / "tahoma-vision-pdf-page.png";
        save_pdf(output_pdf, source.view(), {.dpi = 72.0});
        const auto file_render = render_pdf_page(
            output_pdf, {.dpi = 72.0, .output_format = PixelFormat::RGB8});
        save(output_png, file_render.view());
        require(load(output_png).pixels == file_render.pixels,
                "rendered PDF page PNG round-trip changed pixels");
        std::filesystem::remove(output_pdf);
        std::filesystem::remove(output_png);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Tahoma Vision PDF test failed: " << error.what() << '\n';
        return 1;
    }
}