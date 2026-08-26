#pragma once

#include <filesystem>
#include <string_view>

#include <tahoma/vision/codec.h>

namespace tahoma::vision {

struct EncodeOptions {
    Format format{Format::Unknown};
    PngEncodeOptions png;
    JpegEncodeOptions jpeg;
};

Image load(
    const std::filesystem::path& path,
    const DecodeOptions& options = {});
void save(
    const std::filesystem::path& path, ImageView image,
    const EncodeOptions& options = {});

[[nodiscard]] std::string base64_encode(std::span<const uint8_t> bytes);
[[nodiscard]] std::vector<uint8_t> base64_decode(std::string_view input);
[[nodiscard]] std::vector<uint8_t> resolve_image_bytes(
    const std::string& source);

}  // namespace tahoma::vision
