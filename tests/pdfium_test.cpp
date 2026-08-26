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

        auto editable = PdfDocument::open(pdf_bytes);
        require(editable.page_count() == 1,
            "editable PDF page count changed");
        require(mean_absolute_error(source, editable.render_page({.dpi = 72.0})) <
            1.0,
            "editable PDF render changed pixels");
        const auto original_page = editable.page(0);
        require(original_page.width_points == 64.0 &&
            original_page.height_points == 48.0,
            "editable PDF page geometry changed");
        require(original_page.objects.size() == 1 &&
            original_page.objects.front().type == PdfPageObjectType::Image,
            "image-backed PDF object was not discovered");

        const auto text_index = editable.add_text(
            0, "Hello & <PDF>",
            {.font_size_points = 10.0,
             .fill_color = {.red = 20, .green = 40, .blue = 60},
             .matrix = {.e = 5.0, .f = 10.0}});
        auto edited_page = editable.page(0);
        require(text_index == 1 && edited_page.objects.size() == 2,
            "added PDF text object has the wrong index");
        const auto& added = edited_page.objects.at(text_index);
        require(added.type == PdfPageObjectType::Text && added.text &&
            added.text->text == "Hello & <PDF>",
            "added PDF text was not extracted");
        require(added.matrix && added.matrix->e == 5.0 &&
            added.matrix->f == 10.0,
            "added PDF text placement changed");
        require(added.text->fill_color &&
            added.text->fill_color->red == 20 &&
            added.text->fill_color->green == 40 &&
            added.text->fill_color->blue == 60,
            "added PDF text color changed");

        const auto text_source = editable.save();
        const auto layers = split_pdf_text_layer(text_source);
        require(layers.text_svg.find("Hello &amp; &lt;PDF&gt;") !=
                std::string::npos,
            "PDF text was not XML-escaped in the SVG layer");
        auto background = PdfDocument::open(layers.background_pdf);
        require(background.page(0).objects.size() == 1 &&
                background.page(0).objects.front().type ==
                    PdfPageObjectType::Image,
            "background PDF still contains a top-level text object");
        require(mean_absolute_error(source, background.render_page({.dpi = 72.0})) <
                1.0,
            "background PDF changed non-text page content");

        const auto combined_bytes = combine_pdf_text_layer(
            layers.background_pdf, layers.text_svg);
        auto combined = PdfDocument::open(combined_bytes);
        const auto combined_page = combined.page(0);
        require(combined_page.objects.size() == 2 &&
                combined_page.objects.at(1).text &&
                combined_page.objects.at(1).text->text == "Hello & <PDF>",
            "SVG text did not survive PDF recombination");
        require(combined_page.objects.at(1).matrix &&
                combined_page.objects.at(1).matrix->e == 5.0 &&
                combined_page.objects.at(1).matrix->f == 10.0,
            "SVG text placement changed during PDF recombination");
        require(combined_page.objects.at(1).text->fill_color &&
                combined_page.objects.at(1).text->fill_color->red == 20 &&
                combined_page.objects.at(1).text->fill_color->green == 40 &&
                combined_page.objects.at(1).text->fill_color->blue == 60,
            "SVG text color changed during PDF recombination");
        require(mean_absolute_error(
                    editable.render_page({.dpi = 144.0}),
                    combined.render_page({.dpi = 144.0})) < 1.0,
            "split and recombined PDF changed visual output");

        auto translated_svg = layers.text_svg;
        const auto original_text = translated_svg.find(
            "Hello &amp; &lt;PDF&gt;");
        require(original_text != std::string::npos,
            "generated SVG does not contain expected source text");
        translated_svg.replace(
            original_text, std::string_view{"Hello &amp; &lt;PDF&gt;"}.size(),
            "Hola, España");
        const auto font_size = translated_svg.find("font-size=\"10\"");
        const auto fill_color = translated_svg.find("fill=\"#14283c\"");
        const auto fill_opacity = translated_svg.find("fill-opacity=\"1\"");
        require(font_size != std::string::npos &&
                fill_color != std::string::npos &&
                fill_opacity != std::string::npos,
            "generated SVG does not contain expected text presentation");
        translated_svg.replace(font_size, std::string_view{"font-size=\"10\""}.size(),
            "font-size=\"12\"");
        translated_svg.replace(fill_color, std::string_view{"fill=\"#14283c\""}.size(),
            "fill=\"#c83250\"");
        translated_svg.replace(fill_opacity, std::string_view{"fill-opacity=\"1\""}.size(),
            "fill-opacity=\"0.5\"");
        auto translated = PdfDocument::open(combine_pdf_text_layer(
            layers.background_pdf, translated_svg));
        const auto translated_text = translated.page(0).objects.at(1).text;
        require(translated_text && translated_text->text == "Hola, España",
            "edited SVG text did not become selectable PDF text");
        require(translated_text->font_size_points == 12.0 &&
                translated_text->fill_color &&
                translated_text->fill_color->red == 200 &&
                translated_text->fill_color->green == 50 &&
                translated_text->fill_color->blue == 80 &&
                translated_text->fill_color->alpha == 128,
            "edited SVG presentation did not become PDF text properties");

        auto unsupported_svg = layers.text_svg;
        unsupported_svg.insert(
            unsupported_svg.rfind("</svg>"), "<script>bad()</script>");
        bool rejected_unsupported_svg = false;
        try {
            static_cast<void>(combine_pdf_text_layer(
                layers.background_pdf, unsupported_svg));
        } catch (const CodecError&) {
            rejected_unsupported_svg = true;
        }
        require(rejected_unsupported_svg,
            "unsupported SVG markup was not rejected");

        bool rejected_oversized_svg = false;
        try {
            static_cast<void>(combine_pdf_text_layer(
                layers.background_pdf, layers.text_svg, {},
                {.max_svg_bytes = 16}));
        } catch (const CodecError&) {
            rejected_oversized_svg = true;
        }
        require(rejected_oversized_svg,
            "oversized SVG text layer was not rejected");

        auto persisted = PdfDocument::open(editable.save());
        require(persisted.page(0).objects.at(text_index).text->text ==
                "Hello & <PDF>",
            "added PDF text did not survive serialization");
        persisted.replace_text(0, text_index, "Hola, España");
        auto replaced = PdfDocument::open(persisted.save());
        require(replaced.page(0).objects.at(text_index).text->text ==
            "Hola, España",
            "replaced PDF text did not survive serialization");
        replaced.remove_object(0, text_index);
        auto removed = PdfDocument::open(replaced.save());
        require(removed.page(0).objects.size() == 1 &&
            removed.page(0).objects.front().type ==
                PdfPageObjectType::Image,
            "removed PDF text object survived serialization");

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