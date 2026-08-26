#include <tahoma/vision/io.h>
#include <tahoma/vision/pdf.h>

#include <filesystem>
#include <stdexcept>

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
    return 0;
}
