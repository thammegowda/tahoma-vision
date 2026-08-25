#include <pigzpp/png.h>

#include <algorithm>
#include <array>

int main() {
    constexpr uint32_t width = 2;
    constexpr uint32_t height = 2;
    constexpr uint8_t channels = 3;
    const std::array<uint8_t, width * height * channels> pixels{
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 255,
    };
    const auto encoded = pigzpp::png::encode_buffer(
        pixels.data(), pixels.size(), width, height, channels,
        pigzpp::png::preset_options(pigzpp::png::Preset::Fast));
    const auto decoded = pigzpp::png::decode(
        encoded.data(), encoded.size());
    return decoded.width == width && decoded.height == height &&
            decoded.channels == channels &&
            decoded.pixels.size() == pixels.size() &&
            std::equal(decoded.pixels.begin(), decoded.pixels.end(),
                       pixels.begin())
        ? 0 : 1;
}
