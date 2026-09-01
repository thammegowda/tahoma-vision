#include <tahoma/vision/pdf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string>
#include <utility>

#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fpdf_text.h>
#include <fpdfview.h>

#include <tahoma/vision/codec.h>

namespace tahoma::vision {
namespace {

struct PdfiumRuntime {
    PdfiumRuntime() { FPDF_InitLibrary(); }
    ~PdfiumRuntime() { FPDF_DestroyLibrary(); }

    std::mutex mutex;
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
using TextPageHandle = ScopedHandle<FPDF_TEXTPAGE, FPDFText_ClosePage>;

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

PdfiumRuntime& pdfium_runtime() {
    static PdfiumRuntime runtime;
    return runtime;
}

void initialize_pdfium() {
    static_cast<void>(pdfium_runtime());
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

size_t document_page_count(FPDF_DOCUMENT document) {
    const auto count = FPDF_GetPageCount(document);
    if (count < 0) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium returned an invalid page count"};
    }
    return static_cast<size_t>(count);
}

PageHandle load_page(FPDF_DOCUMENT document, size_t page_index) {
    const auto count = document_page_count(document);
    if (page_index >= count || page_index > static_cast<size_t>(
            std::numeric_limits<int>::max())) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PDF page index is out of range"};
    }
    PageHandle page{FPDF_LoadPage(document, static_cast<int>(page_index))};
    if (!page) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF page load failed: " + pdf_error(FPDF_GetLastError())};
    }
    return page;
}

FPDF_PAGEOBJECT get_page_object(
        FPDF_PAGE page, size_t page_index, size_t object_index) {
    const auto count = FPDFPage_CountObjects(page);
    if (count < 0 || object_index >= static_cast<size_t>(count) ||
        object_index > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF object index is out of range on page " +
                std::to_string(page_index)};
    }
    auto* object = FPDFPage_GetObject(page, static_cast<int>(object_index));
    if (!object) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium could not load page object"};
    }
    return object;
}

float pdf_float(double value, std::string_view name) {
    if (!std::isfinite(value) ||
        value < -std::numeric_limits<float>::max() ||
        value > std::numeric_limits<float>::max()) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF " + std::string{name} + " is not finite or representable"};
    }
    return static_cast<float>(value);
}

void append_utf8(std::string& output, uint32_t code_point) {
    if (code_point <= 0x7f) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else if (code_point <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    }
}

std::string utf16_to_utf8(std::span<const FPDF_WCHAR> input) {
    std::string output;
    for (size_t index = 0; index < input.size(); ++index) {
        uint32_t code_point = input[index];
        if (code_point == 0) break;
        if (code_point >= 0xd800 && code_point <= 0xdbff) {
            if (index + 1 < input.size() && input[index + 1] >= 0xdc00 &&
                input[index + 1] <= 0xdfff) {
                code_point = 0x10000 + ((code_point - 0xd800) << 10) +
                    (input[++index] - 0xdc00);
            } else {
                code_point = 0xfffd;
            }
        } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
            code_point = 0xfffd;
        }
        append_utf8(output, code_point);
    }
    return output;
}

std::vector<FPDF_WCHAR> utf8_to_utf16(std::string_view input) {
    if (input.empty()) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PDF text must not be empty"};
    }
    std::vector<FPDF_WCHAR> output;
    for (size_t index = 0; index < input.size();) {
        const auto first = static_cast<uint8_t>(input[index]);
        uint32_t code_point{};
        size_t continuation_count{};
        if (first <= 0x7f) {
            code_point = first;
        } else if (first >= 0xc2 && first <= 0xdf) {
            code_point = first & 0x1f;
            continuation_count = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            code_point = first & 0x0f;
            continuation_count = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            code_point = first & 0x07;
            continuation_count = 3;
        } else {
            throw CodecError{
                CodecErrorCode::MalformedInput, "PDF text is not valid UTF-8"};
        }
        if (continuation_count > input.size() - index - 1) {
            throw CodecError{
                CodecErrorCode::MalformedInput, "PDF text is not valid UTF-8"};
        }
        for (size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto byte = static_cast<uint8_t>(input[index + offset]);
            if ((byte & 0xc0) != 0x80) {
                throw CodecError{
                    CodecErrorCode::MalformedInput,
                    "PDF text is not valid UTF-8"};
            }
            code_point = (code_point << 6) | (byte & 0x3f);
        }
        if ((continuation_count == 2 && code_point < 0x800) ||
            (continuation_count == 3 && code_point < 0x10000) ||
            code_point > 0x10ffff ||
            (code_point >= 0xd800 && code_point <= 0xdfff) ||
            code_point == 0) {
            throw CodecError{
                CodecErrorCode::MalformedInput, "PDF text is not valid UTF-8"};
        }
        if (code_point <= 0xffff) {
            output.push_back(static_cast<FPDF_WCHAR>(code_point));
        } else {
            code_point -= 0x10000;
            output.push_back(static_cast<FPDF_WCHAR>(
                0xd800 + (code_point >> 10)));
            output.push_back(static_cast<FPDF_WCHAR>(
                0xdc00 + (code_point & 0x3ff)));
        }
        index += continuation_count + 1;
    }
    output.push_back(0);
    return output;
}

PdfPageObjectType page_object_type(int type) {
    switch (type) {
        case FPDF_PAGEOBJ_TEXT: return PdfPageObjectType::Text;
        case FPDF_PAGEOBJ_PATH: return PdfPageObjectType::Path;
        case FPDF_PAGEOBJ_IMAGE: return PdfPageObjectType::Image;
        case FPDF_PAGEOBJ_SHADING: return PdfPageObjectType::Shading;
        case FPDF_PAGEOBJ_FORM: return PdfPageObjectType::Form;
        default: return PdfPageObjectType::Unknown;
    }
}

std::string text_object_text(
        FPDF_PAGEOBJECT object, FPDF_TEXTPAGE text_page) {
    const auto byte_count = FPDFTextObj_GetText(
        object, text_page, nullptr, 0);
    if (byte_count == 0) return {};
    std::vector<FPDF_WCHAR> buffer(
        (byte_count + sizeof(FPDF_WCHAR) - 1) / sizeof(FPDF_WCHAR));
    if (FPDFTextObj_GetText(
            object, text_page, buffer.data(), byte_count) != byte_count) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium could not extract object text"};
    }
    return utf16_to_utf8(buffer);
}

std::string text_object_font_name(FPDF_PAGEOBJECT object) {
    const auto font = FPDFTextObj_GetFont(object);
    if (!font) return {};
    const auto size = FPDFFont_GetBaseFontName(font, nullptr, 0);
    if (size == 0) return {};
    std::string result(size, '\0');
    if (FPDFFont_GetBaseFontName(font, result.data(), result.size()) != size) {
        return {};
    }
    if (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}

PdfPageObject inspect_page_object(
        FPDF_PAGEOBJECT object, size_t index, FPDF_TEXTPAGE text_page) {
    PdfPageObject result{
        .index = index,
        .type = page_object_type(FPDFPageObj_GetType(object)),
    };
    float left{};
    float bottom{};
    float right{};
    float top{};
    if (FPDFPageObj_GetBounds(object, &left, &bottom, &right, &top)) {
        result.bounds = PdfRectangle{left, bottom, right, top};
    }
    FS_MATRIX matrix{};
    if (FPDFPageObj_GetMatrix(object, &matrix)) {
        result.matrix = PdfMatrix{
            matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f};
    }
    if (result.type == PdfPageObjectType::Text) {
        PdfTextObject text{
            .text = text_object_text(object, text_page),
            .font_name = text_object_font_name(object),
        };
        float font_size{};
        if (FPDFTextObj_GetFontSize(object, &font_size)) {
            text.font_size_points = font_size;
        }
        unsigned int red{};
        unsigned int green{};
        unsigned int blue{};
        unsigned int alpha{};
        if (FPDFPageObj_GetFillColor(
                object, &red, &green, &blue, &alpha)) {
            text.fill_color = PdfColor{
                static_cast<uint8_t>(red), static_cast<uint8_t>(green),
                static_cast<uint8_t>(blue), static_cast<uint8_t>(alpha)};
        }
        result.text = std::move(text);
    }
    return result;
}

Image render_document_page(
        FPDF_DOCUMENT document, const PdfRenderOptions& options) {
    require_dpi(options.dpi);
    if (options.output_format != PixelFormat::RGB8 &&
        options.output_format != PixelFormat::RGBA8) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "PDF rendering supports only RGB8 and RGBA8 output"};
    }

    const auto page = load_page(document, options.page_index);
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

std::vector<uint8_t> save_document(FPDF_DOCUMENT document) {
    std::vector<uint8_t> output;
    VectorWriter writer{{1, write_block}, &output};
    if (!FPDF_SaveAsCopy(document, &writer.base, FPDF_NO_INCREMENTAL)) {
        throw CodecError{CodecErrorCode::Backend, "PDF save failed"};
    }
    return output;
}

}  // namespace

class PdfDocument::Impl {
public:
    explicit Impl(std::span<const uint8_t> source)
        : bytes{source.begin(), source.end()} {}

    ~Impl() {
        const auto lock = std::scoped_lock{pdfium_runtime().mutex};
        if (document) FPDF_CloseDocument(document);
    }

    std::vector<uint8_t> bytes;
    FPDF_DOCUMENT document{};
};

PdfDocument::PdfDocument(std::unique_ptr<Impl> impl)
    : impl_{std::move(impl)} {}

PdfDocument::PdfDocument(PdfDocument&&) noexcept = default;
PdfDocument& PdfDocument::operator=(PdfDocument&&) noexcept = default;
PdfDocument::~PdfDocument() = default;

PdfDocument PdfDocument::open(
        std::span<const uint8_t> encoded, std::string_view password) {
    if (encoded.empty()) {
        throw CodecError{
            CodecErrorCode::MalformedInput, "PDF input is empty"};
    }
    initialize_pdfium();
    auto impl = std::make_unique<Impl>(encoded);
    const std::string password_storage{password};
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    impl->document = FPDF_LoadMemDocument64(
        impl->bytes.data(), impl->bytes.size(),
        password.empty() ? nullptr : password_storage.c_str());
    if (!impl->document) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF open failed: " + pdf_error(FPDF_GetLastError())};
    }
    return PdfDocument{std::move(impl)};
}

size_t PdfDocument::page_count() const {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    return document_page_count(impl_->document);
}

PdfPage PdfDocument::page(size_t page_index) const {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    const auto page = load_page(impl_->document, page_index);
    TextPageHandle text_page{FPDFText_LoadPage(page.get())};
    if (!text_page) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium could not load page text"};
    }
    PdfPage result{
        .index = page_index,
        .width_points = FPDF_GetPageWidthF(page.get()),
        .height_points = FPDF_GetPageHeightF(page.get()),
    };
    const auto count = FPDFPage_CountObjects(page.get());
    if (count < 0) {
        throw CodecError{
            CodecErrorCode::Backend,
            "PDFium returned an invalid page object count"};
    }
    result.objects.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        auto* object = FPDFPage_GetObject(page.get(), index);
        if (!object) {
            throw CodecError{
                CodecErrorCode::Backend, "PDFium could not load page object"};
        }
        result.objects.push_back(inspect_page_object(
            object, static_cast<size_t>(index), text_page.get()));
    }
    return result;
}

Image PdfDocument::render_page(const PdfRenderOptions& options) const {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    return render_document_page(impl_->document, options);
}

void PdfDocument::remove_object(size_t page_index, size_t object_index) {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    const auto page = load_page(impl_->document, page_index);
    auto* object = get_page_object(page.get(), page_index, object_index);
    if (!FPDFPage_RemoveObject(page.get(), object)) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium could not remove page object"};
    }
    PageObjectHandle removed{object};
    if (!FPDFPage_GenerateContent(page.get())) {
        throw CodecError{
            CodecErrorCode::Backend,
            "PDFium could not regenerate modified page content"};
    }
}

void PdfDocument::replace_text(
        size_t page_index, size_t object_index, std::string_view text) {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    const auto encoded_text = utf8_to_utf16(text);
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    const auto page = load_page(impl_->document, page_index);
    auto* object = get_page_object(page.get(), page_index, object_index);
    if (FPDFPageObj_GetType(object) != FPDF_PAGEOBJ_TEXT) {
        throw CodecError{
            CodecErrorCode::UnsupportedFeature,
            "PDF object is not editable text"};
    }
    if (!FPDFText_SetText(object, encoded_text.data()) ||
        !FPDFPage_GenerateContent(page.get())) {
        throw CodecError{
            CodecErrorCode::Backend,
            "PDFium could not replace or regenerate page text"};
    }
}

size_t PdfDocument::add_text(
        size_t page_index, std::string_view text,
        const PdfTextOptions& options) {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    if (options.font_name.empty() ||
        options.font_name.find('\0') != std::string::npos) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF standard font name must not be empty"};
    }
    const auto font_size = pdf_float(
        options.font_size_points, "font size");
    if (font_size <= 0.0f) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF font size must be positive"};
    }
    const FS_MATRIX matrix{
        pdf_float(options.matrix.a, "matrix value"),
        pdf_float(options.matrix.b, "matrix value"),
        pdf_float(options.matrix.c, "matrix value"),
        pdf_float(options.matrix.d, "matrix value"),
        pdf_float(options.matrix.e, "matrix value"),
        pdf_float(options.matrix.f, "matrix value"),
    };
    const auto encoded_text = utf8_to_utf16(text);

    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    const auto page = load_page(impl_->document, page_index);
    const auto object_index = FPDFPage_CountObjects(page.get());
    if (object_index < 0) {
        throw CodecError{
            CodecErrorCode::Backend,
            "PDFium returned an invalid page object count"};
    }
    PageObjectHandle object{FPDFPageObj_NewTextObj(
        impl_->document, options.font_name.c_str(), font_size)};
    if (!object || !FPDFText_SetText(object.get(), encoded_text.data()) ||
        !FPDFPageObj_SetFillColor(
            object.get(), options.fill_color.red, options.fill_color.green,
            options.fill_color.blue, options.fill_color.alpha) ||
        !FPDFPageObj_SetMatrix(object.get(), &matrix) ||
        !FPDFPage_InsertObject(page.get(), object.get())) {
        throw CodecError{
            CodecErrorCode::Backend, "PDFium could not create page text"};
    }
    static_cast<void>(object.release());
    if (!FPDFPage_GenerateContent(page.get())) {
        throw CodecError{
            CodecErrorCode::Backend,
            "PDFium could not regenerate page content with added text"};
    }
    return static_cast<size_t>(object_index);
}

std::vector<uint8_t> PdfDocument::save() const {
    if (!impl_ || !impl_->document) {
        throw CodecError{CodecErrorCode::Backend, "PDF document is closed"};
    }
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    return save_document(impl_->document);
}

size_t pdf_page_count(
        std::span<const uint8_t> encoded, std::string_view password) {
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    const auto document = open_document(encoded, password);
    return document_page_count(document.get());
}

Image render_pdf_page(
        std::span<const uint8_t> encoded,
        const PdfRenderOptions& options) {
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
    const auto document = open_document(encoded, options.password);
    return render_document_page(document.get(), options);
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
    const auto lock = std::scoped_lock{pdfium_runtime().mutex};
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

    return save_document(document.get());
}

void save_pdf(
        const std::filesystem::path& path, ImageView image,
        const PdfEncodeOptions& options) {
    write_file(path, encode_pdf(image, options));
}

}  // namespace tahoma::vision
