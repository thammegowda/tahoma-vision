#include <tahoma/vision/format.h>

#include <array>

int main() {
    constexpr std::array<unsigned char, 8> png{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    return tahoma::vision::detect_format(png) == tahoma::vision::Format::Png
        ? 0 : 1;
}