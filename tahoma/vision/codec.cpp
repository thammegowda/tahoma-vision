#include <tahoma/vision/codec.h>

#include <utility>

namespace tahoma::vision {

Image decode_png(std::span<const uint8_t>, const DecodeOptions&);
Image decode_jpeg(std::span<const uint8_t>, const DecodeOptions&);
Image decode_ppm(std::span<const uint8_t>, const DecodeOptions&);

CodecError::CodecError(CodecErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

Image decode(
        std::span<const uint8_t> encoded, const DecodeOptions& options) {
    switch (detect_format(encoded)) {
        case Format::Png: return decode_png(encoded, options);
        case Format::Jpeg: return decode_jpeg(encoded, options);
        case Format::Ppm: return decode_ppm(encoded, options);
        case Format::Pdf:
            throw CodecError{
                CodecErrorCode::UnsupportedFormat,
                "PDF input must be opened as a document"};
        case Format::Svg:
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "SVG rasterization is not enabled"};
        default:
            throw CodecError{
                CodecErrorCode::UnsupportedFormat,
                "encoded bytes are not a supported image format"};
    }
}

}  // namespace tahoma::vision
