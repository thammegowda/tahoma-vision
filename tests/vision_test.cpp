#include <tahoma/vision/codec.h>
#include <tahoma/vision/io.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

template <typename Function>
void require_codec_error(
        tahoma::vision::CodecErrorCode code, Function&& function) {
    try {
        function();
    } catch (const tahoma::vision::CodecError& error) {
        require(error.code() == code, "codec returned the wrong error code");
        return;
    }
    throw std::runtime_error("codec accepted invalid input");
}

tahoma::vision::Image fixture() {
    using namespace tahoma::vision;
    auto result = make_image(3, 2, PixelFormat::RGB8);
    result.pixels = {
        255, 0, 0, 0, 255, 0, 0, 0, 255,
        32, 64, 96, 128, 160, 192, 224, 192, 160,
    };
    return result;
}

tahoma::vision::Image format_fixture(tahoma::vision::PixelFormat format) {
    using namespace tahoma::vision;
    auto result = make_image(3, 2, format);
    for (size_t index = 0; index < result.pixels.size(); ++index) {
        result.pixels[index] = static_cast<uint8_t>(index * 17 + 3);
    }
    return result;
}

void test_format_detection() {
    using tahoma::vision::Format;
    using tahoma::vision::detect_format;
    const std::array<uint8_t, 8> png{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const std::array<uint8_t, 3> jpeg{0xff, 0xd8, 0xff};
    const std::array<uint8_t, 8> pdf{'%', 'P', 'D', 'F', '-', '1', '.', '7'};
    constexpr std::string_view svg =
        "<?xml version=\"1.0\"?>\n<svg viewBox=\"0 0 1 1\"></svg>";
    require(detect_format(png) == Format::Png, "PNG was not detected");
    require(detect_format(jpeg) == Format::Jpeg, "JPEG was not detected");
    require(detect_format(pdf) == Format::Pdf, "PDF was not detected");
    require(
        detect_format(std::span{
            reinterpret_cast<const uint8_t*>(svg.data()), svg.size()}) ==
            Format::Svg,
        "SVG was not detected");
    require(detect_format({}) == Format::Unknown, "empty input was detected");
}

void test_contracts_and_codecs() {
    using namespace tahoma::vision;
    const auto source = fixture();
    validate(source);

    auto short_view = source.view();
    short_view.pixels = short_view.pixels.first(short_view.pixels.size() - 1);
    try {
        validate(short_view);
        throw std::runtime_error("short image buffer was accepted");
    } catch (const std::invalid_argument&) {
    }

    const auto png_bytes = encode_png(source.view());
    const auto png_result = decode(png_bytes);
    require(png_result.pixels == source.pixels, "PNG round-trip changed pixels");

    const auto jpeg_bytes = encode_jpeg(source.view(), {.quality = 100});
    const auto jpeg_result = decode(jpeg_bytes);
    require(jpeg_result.width == source.width, "JPEG width changed");
    require(jpeg_result.height == source.height, "JPEG height changed");
    require(jpeg_result.pixels.size() == source.pixels.size(),
            "JPEG output size changed");

    const auto path = std::filesystem::temp_directory_path() /
        "tahoma-vision-smoke.png";
    save(path, source.view());
    require(load(path).pixels == source.pixels, "file round-trip changed pixels");
    std::filesystem::remove(path);

    require_codec_error(CodecErrorCode::ResourceLimit, [&] {
        static_cast<void>(decode(png_bytes, {.max_pixels = 1}));
    });
    const std::array<uint8_t, 5> pdf{'%', 'P', 'D', 'F', '-'};
    require_codec_error(CodecErrorCode::UnsupportedFormat, [&] {
        static_cast<void>(decode(pdf));
    });
    try {
        static_cast<void>(packed_row_bytes(
            std::numeric_limits<int64_t>::max(), PixelFormat::RGBA8));
        throw std::runtime_error("overflowing geometry was accepted");
    } catch (const std::overflow_error&) {
    }
}

void test_png_contract() {
    using namespace tahoma::vision;
    for (const auto format : {
             PixelFormat::Gray8, PixelFormat::GrayAlpha8,
             PixelFormat::RGB8, PixelFormat::RGBA8}) {
        const auto source = format_fixture(format);
        for (const auto preset : {
                 PngPreset::Fast, PngPreset::Balanced, PngPreset::Small}) {
            const auto encoded = encode_png(source.view(), {.preset = preset});
            const auto decoded = decode(encoded, {.output_format = format});
            require(decoded.format == format, "PNG format changed");
            require(decoded.pixels == source.pixels,
                    "PNG format round-trip changed pixels");
        }
    }

    const auto source = fixture();
    const auto packed_stride = source.row_stride;
    const auto strided_stride = packed_stride + 5;
    std::vector<uint8_t> strided(
        strided_stride * static_cast<size_t>(source.height), 0xee);
    for (int64_t row = 0; row < source.height; ++row) {
        std::copy_n(
            source.pixels.begin() + row * packed_stride, packed_stride,
            strided.begin() + row * strided_stride);
    }
    const ImageView strided_view{
        .width = source.width,
        .height = source.height,
        .format = source.format,
        .row_stride = strided_stride,
        .pixels = strided,
    };
    const auto strided_encoded = encode_png(strided_view);
    require(decode(strided_encoded).pixels == source.pixels,
            "strided PNG encoding changed pixels");

    require_codec_error(CodecErrorCode::UnsupportedFeature, [&] {
        static_cast<void>(encode_png(source.view(), {.threads = 2}));
    });

    auto malformed = encode_png(source.view());
    malformed[malformed.size() / 2] ^= 0x80;
    require_codec_error(CodecErrorCode::MalformedInput, [&] {
        static_cast<void>(decode(malformed));
    });
    malformed = encode_png(source.view());
    malformed.pop_back();
    require_codec_error(CodecErrorCode::MalformedInput, [&] {
        static_cast<void>(decode(malformed));
    });

    auto unsupported = encode_png(source.view());
    unsupported[24] = 16;
    require_codec_error(CodecErrorCode::UnsupportedFeature, [&] {
        static_cast<void>(decode(unsupported));
    });
    unsupported = encode_png(source.view());
    unsupported[28] = 1;
    require_codec_error(CodecErrorCode::UnsupportedFeature, [&] {
        static_cast<void>(decode(unsupported));
    });
}

}  // namespace

int main() {
    try {
        test_format_detection();
        test_contracts_and_codecs();
        test_png_contract();
        std::cout << "Tahoma Vision contract and codec tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Tahoma Vision test failed: " << error.what() << '\n';
        return 1;
    }
}