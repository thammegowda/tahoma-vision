#pragma once

#include <filesystem>

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

}  // namespace tahoma::vision
