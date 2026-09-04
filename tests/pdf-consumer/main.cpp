#include <tahoma/vision/pdf.h>

int main() {
    auto image = tahoma::vision::make_image(1, 1, tahoma::vision::PixelFormat::RGB8);
    image.pixels = {32, 64, 96};
    const auto encoded = tahoma::vision::encode_pdf(image.view());
    return tahoma::vision::pdf_page_count(encoded) == 1 ? 0 : 1;
}