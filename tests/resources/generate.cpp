#include <tahoma/vision/io.h>
#include <tahoma/vision/pdf.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace {

void write_bytes(
        const std::filesystem::path& path,
        std::span<const uint8_t> bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("failed to write " + path.string());
    }
}

void write_text(
        const std::filesystem::path& path, std::string_view text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output.write(text.data(), static_cast<std::streamsize>(text.size()))) {
        throw std::runtime_error("failed to write " + path.string());
    }
}

void replace_once(
        std::string& text, std::string_view before, std::string_view after) {
    const auto position = text.find(before);
    if (position == std::string::npos) {
        throw std::runtime_error("generated SVG does not contain expected text");
    }
    text.replace(position, before.size(), after);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("expected resource output directory");
    const std::filesystem::path output{argv[1]};
    std::filesystem::create_directories(output);

    auto image = tahoma::vision::make_image(
        64, 48, tahoma::vision::PixelFormat::RGB8);
    for (int64_t row = 0; row < image.height; ++row) {
        for (int64_t column = 0; column < image.width; ++column) {
            auto* pixel = image.pixels.data() +
                static_cast<size_t>(row) * image.row_stride +
                static_cast<size_t>(column) * 3;
            pixel[0] = static_cast<uint8_t>(column * 255 / (image.width - 1));
            pixel[1] = static_cast<uint8_t>(row * 255 / (image.height - 1));
            pixel[2] = static_cast<uint8_t>(((row / 8 + column / 8) % 2) * 192 + 32);
        }
    }

    tahoma::vision::save(output / "pattern.png", image.view());
    tahoma::vision::EncodeOptions jpeg{.format = tahoma::vision::Format::Jpeg};
    jpeg.jpeg.quality = 95;
    tahoma::vision::save(output / "pattern.jpg", image.view(), jpeg);
    tahoma::vision::save_pdf(
        output / "one-page.pdf", image.view(), {.dpi = 72.0});

    const auto demo = output / "text-layer-demo";
    std::filesystem::create_directories(demo);
    auto document = tahoma::vision::PdfDocument::open(
        tahoma::vision::encode_pdf(image.view(), {.dpi = 36.0}));
    static_cast<void>(document.add_text(
        0, "Hello, world!",
        {.font_name = "Helvetica-Bold",
         .font_size_points = 9.0,
         .fill_color = {.red = 20, .green = 35, .blue = 80},
         .matrix = {.e = 8.0, .f = 68.0}}));
    static_cast<void>(document.add_text(
        0, "This text becomes an editable SVG layer.",
        {.font_name = "Helvetica",
         .font_size_points = 5.0,
         .fill_color = {.red = 45, .green = 55, .blue = 70},
         .matrix = {.e = 8.0, .f = 56.0}}));

    const auto source_pdf = document.save();
    const auto layers = tahoma::vision::split_pdf_text_layer(source_pdf);
    auto translated_svg = layers.text_svg;
    replace_once(translated_svg, "Hello, world!", "¡Hola, mundo!");
    replace_once(
        translated_svg,
        "This text becomes an editable SVG layer.",
        "Este texto se convierte en una capa SVG editable.");
    replace_once(translated_svg, "font-size=\"9\"", "font-size=\"10\"");
    replace_once(translated_svg, "fill=\"#142350\"", "fill=\"#b42318\"");
    const auto translated_pdf = tahoma::vision::combine_pdf_text_layer(
        layers.background_pdf, translated_svg);

    write_bytes(demo / "01-source.pdf", source_pdf);
    write_bytes(demo / "02-background.pdf", layers.background_pdf);
    write_text(demo / "03-text-source.svg", layers.text_svg);
    write_text(demo / "04-text-spanish.svg", translated_svg);
    write_bytes(demo / "05-recombined-spanish.pdf", translated_pdf);
    tahoma::vision::save(
        demo / "01-source.png",
        tahoma::vision::render_pdf_page(source_pdf, {.dpi = 144.0}).view());
    tahoma::vision::save(
        demo / "02-background.png",
        tahoma::vision::render_pdf_page(
            layers.background_pdf, {.dpi = 144.0}).view());
    tahoma::vision::save(
        demo / "05-recombined-spanish.png",
        tahoma::vision::render_pdf_page(
            translated_pdf, {.dpi = 144.0}).view());
    return 0;
}
