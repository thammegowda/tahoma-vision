#include <tahoma/vision/codec.h>
#include <tahoma/vision/document_conversion.h>
#include <tahoma/vision/io.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <jpeglib.h>

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

std::vector<uint8_t> cmyk_jpeg_fixture() {
    jpeg_compress_struct encoder{};
    jpeg_error_mgr error{};
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    unsigned char* output = nullptr;
    unsigned long output_size = 0;
    jpeg_mem_dest(&encoder, &output, &output_size);
    encoder.image_width = 2;
    encoder.image_height = 1;
    encoder.input_components = 4;
    encoder.in_color_space = JCS_CMYK;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, 100, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    std::array<uint8_t, 8> pixels{
        255, 0, 0, 255,
        0, 255, 0, 255,
    };
    JSAMPROW row = pixels.data();
    jpeg_write_scanlines(&encoder, &row, 1);
    jpeg_finish_compress(&encoder);
    std::vector<uint8_t> result(output, output + output_size);
    jpeg_destroy_compress(&encoder);
    std::free(output);
    return result;
}

std::vector<uint8_t> oriented_jpeg_fixture(uint8_t orientation) {
    jpeg_compress_struct encoder{};
    jpeg_error_mgr error{};
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    unsigned char* output = nullptr;
    unsigned long output_size = 0;
    jpeg_mem_dest(&encoder, &output, &output_size);
    encoder.image_width = 2;
    encoder.image_height = 3;
    encoder.input_components = 3;
    encoder.in_color_space = JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, 100, TRUE);
    jpeg_start_compress(&encoder, TRUE);
    const std::array<uint8_t, 32> exif{
        'E', 'x', 'i', 'f', 0, 0,
        'I', 'I', 42, 0, 8, 0, 0, 0,
        1, 0,
        0x12, 0x01, 3, 0, 1, 0, 0, 0, orientation, 0, 0, 0,
        0, 0, 0, 0,
    };
    jpeg_write_marker(
        &encoder, JPEG_APP0 + 1, exif.data(), exif.size());
    std::array<uint8_t, 18> pixels{
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 0,
        255, 0, 255, 0, 255, 255,
    };
    while (encoder.next_scanline < encoder.image_height) {
        JSAMPROW row = pixels.data() + encoder.next_scanline * 6;
        jpeg_write_scanlines(&encoder, &row, 1);
    }
    jpeg_finish_compress(&encoder);
    std::vector<uint8_t> result(output, output + output_size);
    jpeg_destroy_compress(&encoder);
    std::free(output);
    return result;
}

std::pair<int64_t, int64_t> oriented_coordinate(
        int orientation, int64_t x, int64_t y,
        int64_t width, int64_t height) {
    switch (orientation) {
        case 2: return {width - 1 - x, y};
        case 3: return {width - 1 - x, height - 1 - y};
        case 4: return {x, height - 1 - y};
        case 5: return {y, x};
        case 6: return {height - 1 - y, x};
        case 7: return {height - 1 - y, width - 1 - x};
        case 8: return {y, width - 1 - x};
        default: return {x, y};
    }
}

void test_format_detection() {
    using tahoma::vision::Format;
    using tahoma::vision::detect_format;
    const std::array<uint8_t, 8> png{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const std::array<uint8_t, 3> jpeg{0xff, 0xd8, 0xff};
    const std::array<uint8_t, 2> ppm{'P', '6'};
    const std::array<uint8_t, 8> pdf{'%', 'P', 'D', 'F', '-', '1', '.', '7'};
    constexpr std::string_view svg =
        "<?xml version=\"1.0\"?>\n<svg viewBox=\"0 0 1 1\"></svg>";
    require(detect_format(png) == Format::Png, "PNG was not detected");
    require(detect_format(jpeg) == Format::Jpeg, "JPEG was not detected");
    require(detect_format(ppm) == Format::Ppm, "PPM was not detected");
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

    const auto ppm_bytes = encode_ppm(source.view());
    const auto ppm_result = decode(ppm_bytes);
    require(ppm_result.pixels == source.pixels, "PPM round-trip changed pixels");
    require(base64_decode(base64_encode(ppm_bytes)) == ppm_bytes,
            "base64 round-trip changed bytes");

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

double mean_absolute_error(
    const tahoma::vision::Image& left,
    const tahoma::vision::Image& right) {
    require(left.width == right.width && left.height == right.height,
        "image geometries differ");
    require(left.format == right.format, "image formats differ");
    require(left.pixels.size() == right.pixels.size(), "image sizes differ");
    uint64_t error = 0;
    for (size_t index = 0; index < left.pixels.size(); ++index) {
    error += static_cast<uint64_t>(std::abs(
        static_cast<int>(left.pixels[index]) - right.pixels[index]));
    }
    return static_cast<double>(error) / left.pixels.size();
}

void test_resource_round_trips() {
    using namespace tahoma::vision;
    const std::filesystem::path resources{TAHOMA_VISION_TEST_RESOURCE_DIR};
    const auto png = load(resources / "pattern.png");
    const auto jpeg = load(resources / "pattern.jpg");
    require(png.width == 64 && png.height == 48, "PNG fixture geometry changed");
    require(jpeg.width == 64 && jpeg.height == 48,
        "JPEG fixture geometry changed");

    const auto temporary = std::filesystem::temp_directory_path();
    const auto png_path = temporary / "tahoma-vision-roundtrip.png";
    save(png_path, png.view());
    require(load(png_path).pixels == png.pixels,
        "PNG resource file round-trip changed pixels");
    std::filesystem::remove(png_path);

    const auto jpeg_path = temporary / "tahoma-vision-roundtrip.jpg";
    EncodeOptions jpeg_options;
    jpeg_options.format = Format::Jpeg;
    jpeg_options.jpeg.quality = 95;
    save(jpeg_path, jpeg.view(), jpeg_options);
    const auto jpeg_roundtrip = load(jpeg_path);
        const auto error = mean_absolute_error(jpeg, jpeg_roundtrip);
        require(error < 6.0,
            "JPEG resource round-trip MAE " + std::to_string(error) +
            " exceeded tolerance");
    std::filesystem::remove(jpeg_path);
}

void test_jpeg_contract() {
    using namespace tahoma::vision;
    const auto rgb = fixture();
    for (const auto quality : {1, 95, 100}) {
        const auto encoded = encode_jpeg(rgb.view(), {.quality = quality});
        const auto decoded = decode(encoded);
        require(decoded.width == rgb.width, "JPEG width changed");
        require(decoded.height == rgb.height, "JPEG height changed");
        require(decoded.format == PixelFormat::RGB8, "JPEG format changed");
    }

    const auto gray = format_fixture(PixelFormat::Gray8);
    const auto gray_encoded = encode_jpeg(gray.view(), {.quality = 100});
    const auto gray_decoded = decode(
        gray_encoded, {.output_format = PixelFormat::Gray8});
    require(gray_decoded.format == PixelFormat::Gray8,
            "grayscale JPEG format changed");
    require(gray_decoded.pixels.size() == gray.pixels.size(),
            "grayscale JPEG size changed");
    for (size_t index = 0; index < gray.pixels.size(); ++index) {
        require(std::abs(
                    static_cast<int>(gray_decoded.pixels[index]) -
                    static_cast<int>(gray.pixels[index])) <= 1,
                "quality-100 grayscale JPEG drift exceeded one value");
    }

    require_codec_error(CodecErrorCode::MalformedInput, [&] {
        static_cast<void>(encode_jpeg(rgb.view(), {.quality = 0}));
    });
    require_codec_error(CodecErrorCode::MalformedInput, [&] {
        static_cast<void>(encode_jpeg(rgb.view(), {.quality = 101}));
    });
    require_codec_error(CodecErrorCode::UnsupportedFeature, [&] {
        const auto rgba = format_fixture(PixelFormat::RGBA8);
        static_cast<void>(encode_jpeg(rgba.view()));
    });

    auto truncated = encode_jpeg(rgb.view());
    truncated.resize(truncated.size() / 2);
    require_codec_error(CodecErrorCode::MalformedInput, [&] {
        static_cast<void>(decode(truncated));
    });
    require_codec_error(CodecErrorCode::ResourceLimit, [&] {
        const auto encoded = encode_jpeg(rgb.view());
        static_cast<void>(decode(encoded, {.max_pixels = 1}));
    });

    const auto cmyk = cmyk_jpeg_fixture();
    const auto cmyk_rgb = decode(cmyk);
    require(cmyk_rgb.width == 2 && cmyk_rgb.height == 1,
            "CMYK JPEG geometry changed");
    require(cmyk_rgb.pixels[0] > cmyk_rgb.pixels[1] &&
            cmyk_rgb.pixels[0] > cmyk_rgb.pixels[2],
            "CMYK red pixel conversion failed");
    require(cmyk_rgb.pixels[4] > cmyk_rgb.pixels[3] &&
            cmyk_rgb.pixels[4] > cmyk_rgb.pixels[5],
            "CMYK green pixel conversion failed");
    const auto cmyk_gray = decode(
        cmyk, {.output_format = PixelFormat::Gray8});
    require(cmyk_gray.pixels.size() == 2,
            "CMYK grayscale conversion size changed");

    for (int orientation = 1; orientation <= 8; ++orientation) {
        const auto encoded = oriented_jpeg_fixture(
            static_cast<uint8_t>(orientation));
        const auto baseline = decode(
            encoded, {.apply_exif_orientation = false});
        const auto oriented = decode(encoded);
        const auto swaps_axes = orientation >= 5;
        require(oriented.width ==
                    (swaps_axes ? baseline.height : baseline.width),
                "EXIF orientation width is incorrect");
        require(oriented.height ==
                    (swaps_axes ? baseline.width : baseline.height),
                "EXIF orientation height is incorrect");
        for (int64_t y = 0; y < baseline.height; ++y) {
            for (int64_t x = 0; x < baseline.width; ++x) {
                const auto [target_x, target_y] = oriented_coordinate(
                    orientation, x, y, baseline.width, baseline.height);
                const auto source_offset = static_cast<size_t>(
                    y * baseline.row_stride + x * 3);
                const auto target_offset = static_cast<size_t>(
                    target_y * oriented.row_stride + target_x * 3);
                require(std::equal(
                            baseline.pixels.begin() + source_offset,
                            baseline.pixels.begin() + source_offset + 3,
                            oriented.pixels.begin() + target_offset),
                        "EXIF orientation moved a pixel incorrectly");
            }
        }
    }
}

}  // namespace

int main() {
    try {
        test_format_detection();
        test_contracts_and_codecs();
        test_png_contract();
        test_resource_round_trips();
        test_jpeg_contract();
        std::cout << "Tahoma Vision contract and codec tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Tahoma Vision test failed: " << error.what() << '\n';
        return 1;
    }
}