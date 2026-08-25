#include <tahoma/vision/image.h>

int main() {
    const auto image = tahoma::vision::make_image(
        2, 3, tahoma::vision::PixelFormat::RGB8);
    return image.pixels.size() == 18 ? 0 : 1;
}