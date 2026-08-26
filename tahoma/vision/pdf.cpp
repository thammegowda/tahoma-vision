#include <tahoma/vision/pdf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fpdfview.h>

#include <tahoma/vision/codec.h>

namespace tahoma::vision {
namespace {

struct PdfiumRuntime {
    PdfiumRuntime() { FPDF_InitLibrary(); }
    ~PdfiumRuntime() { FPDF_DestroyLibrary(); }
};

template <typename Handle, void (*Close)(Handle)>
class ScopedHandle {
public:
    explicit ScopedHandle(Handle value = nullptr) : value_{value} {}
    ~ScopedHandle() { if (value_) Close(value_); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept
        : value_{std::exchange(other.value_, nullptr)} {}
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            if (value_) Close(value_);
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] Handle get() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_; }
    [[nodiscard]] Handle release() noexcept {
        return std::exchange(value_, nullptr);
    }

private:
    Handle value_;
};

using DocumentHandle = ScopedHandle<FPDF_DOCUMENT, FPDF_CloseDocument>;
using PageHandle = ScopedHandle<FPDF_PAGE, FPDF_ClosePage>;
using BitmapHandle = ScopedHandle<FPDF_BITMAP, FPDFBitmap_Destroy>;
using PageObjectHandle = ScopedHandle<FPDF_PAGEOBJECT, FPDFPageObj_Destroy>;

struct VectorWriter {
    FPDF_FILEWRITE base;
    std::vector<uint8_t>* output;
};

int write_block(FPDF_FILEWRITE* writer, const void* data, unsigned long size) {
    auto* state = reinterpret_cast<VectorWriter*>(writer);
    try {
        const auto* begin = static_cast<const uint8_t*>(data);
        state->output->insert(state->output->end(), begin, begin + size);
        return 1;
    } catch (...) {
        return 0;
    }
}

void initialize_pdfium() {
    static PdfiumRuntime runtime;
}

std::string pdf_error(unsigned long code) {
    switch (code) {
        case FPDF_ERR_SUCCESS: return "success";
        case FPDF_ERR_UNKNOWN: return "unknown error";
        case FPDF_ERR_FILE: return "file error";
        case FPDF_ERR_FORMAT: return "invalid PDF format";
        case FPDF_ERR_PASSWORD: return "password required or incorrect";
        case FPDF_ERR_SECURITY: return "unsupported security scheme";
        case FPDF_ERR_PAGE: return "page error";
        default: return "PDFium error " + std::to_string(code);
    }
}

DocumentHandle open_document(
        std::span<const uint8_t> encoded, std::string_view password) {
    if (encoded.empty()) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PDF input is empty"};
    }
    initialize_pdfium();
    const std::string password_storage{password};
    DocumentHandle document{FPDF_LoadMemDocument64(
        encoded.data(), encoded.size(),
        password.empty() ? nullptr : password_storage.c_str())};
    if (!document) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF open failed: " + pdf_error(FPDF_GetLastError())};
    }
    return document;
}

std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw CodecError{
            CodecErrorCode::Io, "failed to open PDF: " + path.string()};
    }
    std::vector<uint8_t> result{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    if (!stream.eof() && stream.fail()) {
        throw CodecError{
            CodecErrorCode::Io, "failed to read PDF: " + path.string()};
    }
    return result;
}

void write_file(
        const std::filesystem::path& path, std::span<const uint8_t> bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        throw CodecError{
            CodecErrorCode::Io, "failed to write PDF: " + path.string()};
    }
}

int render_dimension(double points, double dpi) {
    const auto pixels = std::ceil(points * dpi / 72.0);
    if (!std::isfinite(pixels) || pixels <= 0.0 ||
        pixels > static_cast<double>(std::numeric_limits<int>::max())) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "rendered PDF page dimensions exceed backend limits"};
    }
    return static_cast<int>(pixels);
}

void require_dpi(double dpi) {
    if (!std::isfinite(dpi) || dpi <= 0.0) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PDF DPI must be positive"};
    }
}

}  // namespace

size_t pdf_page_count(
        std::span<const uint8_t> encoded, std::string_view password) {
    const auto document = open_document(encoded, password);
    const auto count = FPDF_GetPageCount(document.get());
    if (count < 0) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium returned an invalid page count"};
    }
    return static_cast<size_t>(count);
}

Image render_pdf_page(
        std::span<const uint8_t> encoded,
        const PdfRenderOptions& options) {
    require_dpi(options.dpi);
    if (options.output_format != PixelFormat::RGB8 &&
        options.output_format != PixelFormat::RGBA8) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "PDF rendering supports only RGB8 and RGBA8 output"};
    }

    const auto document = open_document(encoded, options.password);
    const auto page_count = FPDF_GetPageCount(document.get());
    if (page_count <= 0 || options.page_index >= static_cast<size_t>(page_count)) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PDF page index is out of range"};
    }
    PageHandle page{FPDF_LoadPage(
        document.get(), static_cast<int>(options.page_index))};
    if (!page) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF page load failed: " + pdf_error(FPDF_GetLastError())};
    }

    const auto width = render_dimension(
        FPDF_GetPageWidthF(page.get()), options.dpi);
    const auto height = render_dimension(
        FPDF_GetPageHeightF(page.get()), options.dpi);
    const auto pixel_count = static_cast<uint64_t>(width) * height;
    const auto row_stride = packed_row_bytes(width, options.output_format);
    const auto output_bytes = required_buffer_bytes(
        width, height, options.output_format, row_stride);
    if (pixel_count > options.max_pixels ||
        output_bytes > options.max_decoded_bytes) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "rendered PDF page exceeds configured limits"};
    }

    BitmapHandle bitmap{FPDFBitmap_Create(width, height, 1)};
    if (!bitmap) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium bitmap allocation failed"};
    }
    FPDFBitmap_FillRect(bitmap.get(), 0, 0, width, height, 0xffffffff);
    FPDF_RenderPageBitmap(
        bitmap.get(), page.get(), 0, 0, width, height, 0, FPDF_ANNOT);

    auto result = make_image(width, height, options.output_format);
    const auto* input = static_cast<const uint8_t*>(
        FPDFBitmap_GetBuffer(bitmap.get()));
    const auto input_stride = FPDFBitmap_GetStride(bitmap.get());
    const auto output_channels = channel_count(options.output_format);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const auto* source = input + row * input_stride + column * 4;
            auto* target = result.pixels.data() +
                static_cast<size_t>(row) * result.row_stride +
                static_cast<size_t>(column) * output_channels;
            target[0] = source[2];
            target[1] = source[1];
            target[2] = source[0];
            if (output_channels == 4) target[3] = source[3];
        }
    }
    return result;
}

Image render_pdf_page(
        const std::filesystem::path& path,
        const PdfRenderOptions& options) {
    return render_pdf_page(read_file(path), options);
}

std::vector<uint8_t> encode_pdf(
        ImageView image, const PdfEncodeOptions& options) {
    validate(image);
    require_dpi(options.dpi);
    if (image.format != PixelFormat::RGB8 &&
        image.format != PixelFormat::RGBA8) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "PDF encoding supports only RGB8 and RGBA8 input"};
    }
    if (image.width > std::numeric_limits<int>::max() ||
        image.height > std::numeric_limits<int>::max()) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "PDF image dimensions exceed backend limits"};
    }

    initialize_pdfium();
    DocumentHandle document{FPDF_CreateNewDocument()};
    if (!document) {
        throw CodecError{
            CodecErrorCode::Backend, "PDF document creation failed"};
    }
    const auto page_width = static_cast<double>(image.width) * 72.0 / options.dpi;
    const auto page_height = static_cast<double>(image.height) * 72.0 / options.dpi;
    PageHandle page{FPDFPage_New(
        document.get(), 0, page_width, page_height)};
    if (!page) {
        throw CodecError{CodecErrorCode::Backend, "PDF page creation failed"};
    }

    const auto bitmap_stride = static_cast<size_t>(image.width) * 4;
    std::vector<uint8_t> bitmap_pixels(
        bitmap_stride * static_cast<size_t>(image.height));
    const auto source_channels = channel_count(image.format);
    for (int64_t row = 0; row < image.height; ++row) {
        for (int64_t column = 0; column < image.width; ++column) {
            const auto* source = image.pixels.data() +
                static_cast<size_t>(row) * image.row_stride +
                static_cast<size_t>(column) * source_channels;
            auto* target = bitmap_pixels.data() +
                static_cast<size_t>(row) * bitmap_stride +
                static_cast<size_t>(column) * 4;
            target[0] = source[2];
            target[1] = source[1];
            target[2] = source[0];
            target[3] = source_channels == 4 ? source[3] : 255;
        }
    }

    BitmapHandle bitmap{FPDFBitmap_CreateEx(
        static_cast<int>(image.width), static_cast<int>(image.height),
        FPDFBitmap_BGRA, bitmap_pixels.data(),
        static_cast<int>(bitmap_stride))};
    if (!bitmap) {
        throw CodecError{CodecErrorCode::Backend, "PDF image bitmap failed"};
    }
    PageObjectHandle image_object{FPDFPageObj_NewImageObj(document.get())};
    if (!image_object || !FPDFImageObj_SetBitmap(
            nullptr, 0, image_object.get(), bitmap.get()) ||
        !FPDFImageObj_SetMatrix(
            image_object.get(), page_width, 0, 0, page_height, 0, 0) ||
        !FPDFPage_InsertObject(page.get(), image_object.get())) {
        throw CodecError{
            CodecErrorCode::Backend, "PDF image object creation failed"};
    }
    static_cast<void>(image_object.release());
    if (!FPDFPage_GenerateContent(page.get())) {
        throw CodecError{
            CodecErrorCode::Backend, "PDF page content generation failed"};
    }

    std::vector<uint8_t> output;
    VectorWriter writer{{1, write_block}, &output};
    if (!FPDF_SaveAsCopy(
            document.get(), &writer.base, FPDF_NO_INCREMENTAL)) {
        throw CodecError{CodecErrorCode::Backend, "PDF save failed"};
    }
    return output;
}

void save_pdf(
        const std::filesystem::path& path, ImageView image,
        const PdfEncodeOptions& options) {
    write_file(path, encode_pdf(image, options));
}

}  // namespace tahoma::vision
