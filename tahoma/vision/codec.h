#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <tahoma/vision/format.h>
#include <tahoma/vision/image.h>

namespace tahoma::vision {

enum class CodecErrorCode : uint8_t {
    UnsupportedFormat,
    UnsupportedFeature,
    MalformedInput,
    ResourceLimit,
    Io,
    Backend,
};

class CodecError : public std::runtime_error {
public:
    CodecError(CodecErrorCode code, std::string message);

    [[nodiscard]] CodecErrorCode code() const noexcept { return code_; }

private:
    CodecErrorCode code_;
};

struct DecodeOptions {
    PixelFormat output_format{PixelFormat::RGB8};
    bool apply_exif_orientation{true};
    uint64_t max_pixels{268435456};
    uint64_t max_decoded_bytes{1073741824};
};

enum class PngPreset : uint8_t {
    Fast,
    Balanced,
    Small,
};

struct PngEncodeOptions {
    PngPreset preset{PngPreset::Fast};
    size_t threads{};
};

struct JpegEncodeOptions {
    int quality{95};
};

[[nodiscard]] Image decode(
    std::span<const uint8_t> encoded,
    const DecodeOptions& options = {});
[[nodiscard]] std::vector<uint8_t> encode_png(
    ImageView image, const PngEncodeOptions& options = {});
[[nodiscard]] std::vector<uint8_t> encode_jpeg(
    ImageView image, const JpegEncodeOptions& options = {});
[[nodiscard]] std::vector<uint8_t> encode_ppm(ImageView image);

}  // namespace tahoma::vision
