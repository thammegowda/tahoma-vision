#include <tahoma/vision/pdf.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

#include <pugixml.hpp>

#include <tahoma/vision/codec.h>

namespace tahoma::vision {
namespace {

constexpr std::string_view svg_namespace{"http://www.w3.org/2000/svg"};

struct SvgText {
    size_t page_index{};
    std::string text;
    std::string font_name;
    double font_size{};
    PdfColor fill;
    PdfMatrix matrix;
};

void require_options(const PdfTextLayerOptions& options) {
    if (options.max_text_elements == 0 || options.max_svg_bytes == 0) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "PDF text-layer limits must be positive"};
    }
}

std::string number(double value) {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

std::string hex_color(PdfColor color) {
    std::ostringstream output;
    output << '#' << std::hex << std::setfill('0')
           << std::setw(2) << static_cast<unsigned>(color.red)
           << std::setw(2) << static_cast<unsigned>(color.green)
           << std::setw(2) << static_cast<unsigned>(color.blue);
    return output.str();
}

std::string standard_font_name(std::string_view name) {
    if (name.size() > 7 && name[6] == '+' &&
        std::all_of(name.begin(), name.begin() + 6, [](char value) {
            return value >= 'A' && value <= 'Z';
        })) {
        name.remove_prefix(7);
    }

    static const std::array<std::string_view, 14> fonts{
        "Courier", "Courier-Bold", "Courier-Oblique",
        "Courier-BoldOblique", "Helvetica", "Helvetica-Bold",
        "Helvetica-Oblique", "Helvetica-BoldOblique", "Times-Roman",
        "Times-Bold", "Times-Italic", "Times-BoldItalic", "Symbol",
        "ZapfDingbats",
    };
    if (std::find(fonts.begin(), fonts.end(), name) != fonts.end()) {
        return std::string{name};
    }

    std::string lower{name};
    std::transform(lower.begin(), lower.end(), lower.begin(), [](char value) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    });
    const auto bold = lower.find("bold") != std::string::npos;
    const auto italic = lower.find("italic") != std::string::npos ||
        lower.find("oblique") != std::string::npos;
    if (lower.find("courier") != std::string::npos ||
        lower.find("mono") != std::string::npos) {
        if (bold && italic) return "Courier-BoldOblique";
        if (bold) return "Courier-Bold";
        if (italic) return "Courier-Oblique";
        return "Courier";
    }
    if (lower.find("times") != std::string::npos ||
        lower.find("serif") != std::string::npos) {
        if (bold && italic) return "Times-BoldItalic";
        if (bold) return "Times-Bold";
        if (italic) return "Times-Italic";
        return "Times-Roman";
    }
    if (bold && italic) return "Helvetica-BoldOblique";
    if (bold) return "Helvetica-Bold";
    if (italic) return "Helvetica-Oblique";
    return "Helvetica";
}

PdfMatrix pdf_to_svg_matrix(const PdfMatrix& matrix, double page_height) {
    return {
        matrix.a, -matrix.b, -matrix.c, matrix.d,
        matrix.e, page_height - matrix.f,
    };
}

PdfMatrix svg_to_pdf_matrix(const PdfMatrix& matrix, double page_height) {
    return pdf_to_svg_matrix(matrix, page_height);
}

std::string matrix_value(const PdfMatrix& matrix) {
    return "matrix(" + number(matrix.a) + ' ' + number(matrix.b) + ' ' +
        number(matrix.c) + ' ' + number(matrix.d) + ' ' + number(matrix.e) +
        ' ' + number(matrix.f) + ')';
}

void set_number_attribute(
        pugi::xml_node node, const char* name, double value) {
    node.append_attribute(name).set_value(number(value).c_str());
}

void require_allowed_attributes(
        pugi::xml_node node,
        const std::unordered_set<std::string_view>& allowed) {
    for (const auto attribute : node.attributes()) {
        if (!allowed.contains(attribute.name())) {
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "unsupported restricted SVG attribute '" +
                    std::string{attribute.name()} + "'"};
        }
    }
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(
            static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(
            static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

double parse_number(std::string_view value, std::string_view name) {
    value = trim(value);
    double result{};
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        !std::isfinite(result)) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG has an invalid " + std::string{name}};
    }
    return result;
}

std::array<double, 6> parse_numbers(
        std::string_view value, size_t count, std::string_view name) {
    std::array<double, 6> result{};
    size_t index{};
    for (size_t item = 0; item < count; ++item) {
        while (index < value.size() &&
               (std::isspace(static_cast<unsigned char>(value[index])) ||
                value[index] == ',')) {
            ++index;
        }
        if (index == value.size()) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "restricted SVG has an invalid " + std::string{name}};
        }
        const auto parsed = std::from_chars(
            value.data() + index, value.data() + value.size(), result[item]);
        if (parsed.ec != std::errc{} || !std::isfinite(result[item])) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "restricted SVG has an invalid " + std::string{name}};
        }
        index = static_cast<size_t>(parsed.ptr - value.data());
    }
    while (index < value.size() &&
           std::isspace(static_cast<unsigned char>(value[index]))) {
        ++index;
    }
    if (index != value.size()) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG has an invalid " + std::string{name}};
    }
    return result;
}

PdfMatrix parse_matrix(std::string_view value) {
    value = trim(value);
    constexpr std::string_view prefix{"matrix("};
    if (!value.starts_with(prefix) || !value.ends_with(')')) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG text transform must be a matrix"};
    }
    value.remove_prefix(prefix.size());
    value.remove_suffix(1);
    const auto values = parse_numbers(value, 6, "text transform");
    return {
        values[0], values[1], values[2], values[3], values[4], values[5],
    };
}

std::array<double, 4> parse_view_box(pugi::xml_node node) {
    const auto attribute = node.attribute("viewBox");
    if (!attribute) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG page requires viewBox"};
    }
    const auto values = parse_numbers(attribute.value(), 4, "viewBox");
    return {values[0], values[1], values[2], values[3]};
}

uint8_t hex_byte(char high, char low) {
    const auto digit = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    const auto high_value = digit(high);
    const auto low_value = digit(low);
    if (high_value < 0 || low_value < 0) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG fill must be #RRGGBB"};
    }
    return static_cast<uint8_t>((high_value << 4) | low_value);
}

PdfColor parse_fill(pugi::xml_node node) {
    const std::string_view fill = node.attribute("fill").value();
    if (fill.size() != 7 || fill.front() != '#') {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG fill must be #RRGGBB"};
    }
    const auto opacity = node.attribute("fill-opacity")
        ? parse_number(node.attribute("fill-opacity").value(), "fill opacity")
        : 1.0;
    if (opacity < 0.0 || opacity > 1.0) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG fill opacity must be between zero and one"};
    }
    return {
        hex_byte(fill[1], fill[2]), hex_byte(fill[3], fill[4]),
        hex_byte(fill[5], fill[6]),
        static_cast<uint8_t>(std::lround(opacity * 255.0)),
    };
}

std::string text_content(pugi::xml_node node) {
    std::string result;
    for (const auto child : node.children()) {
        if (child.type() != pugi::node_pcdata &&
            child.type() != pugi::node_cdata) {
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "restricted SVG text cannot contain child elements"};
        }
        result += child.value();
    }
    if (result.empty()) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG text must not be empty"};
    }
    return result;
}

std::vector<SvgText> parse_text_svg(
        std::string_view svg, const PdfDocument& background,
        const PdfTextLayerOptions& options) {
    if (svg.size() > options.max_svg_bytes) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "restricted SVG exceeds the configured size limit"};
    }

    pugi::xml_document document;
    const auto parsed = document.load_buffer(
        svg.data(), svg.size(), pugi::parse_default,
        pugi::encoding_utf8);
    if (!parsed) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG parse failed: " +
                std::string{parsed.description()}};
    }
    for (const auto child : document.children()) {
        if (child.type() != pugi::node_declaration &&
            child.type() != pugi::node_element) {
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "restricted SVG contains unsupported document markup"};
        }
    }

    const auto root = document.document_element();
    if (!root || std::string_view{root.name()} != "svg" ||
        std::string_view{root.attribute("xmlns").value()} != svg_namespace) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG requires an SVG namespace root"};
    }
    require_allowed_attributes(root, {
        "xmlns", "version", "width", "height", "viewBox",
    });

    std::vector<SvgText> result;
    size_t page_index{};
    for (const auto page_node : root.children()) {
        if (page_node.type() != pugi::node_element ||
            std::string_view{page_node.name()} != "svg") {
            throw CodecError{
                CodecErrorCode::UnsupportedFeature,
                "restricted SVG root may contain only page SVG elements"};
        }
        if (page_index >= background.page_count()) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "restricted SVG has more pages than the background PDF"};
        }
        require_allowed_attributes(page_node, {
            "id", "x", "y", "width", "height", "viewBox",
        });
        const auto expected_id = "page-" + std::to_string(page_index);
        if (std::string_view{page_node.attribute("id").value()} != expected_id) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "restricted SVG pages must be ordered and named page-N"};
        }
        const auto page = background.page(page_index);
        const auto view_box = parse_view_box(page_node);
        if (std::abs(view_box[0]) > 1e-6 || std::abs(view_box[1]) > 1e-6 ||
            std::abs(view_box[2] - page.width_points) > 1e-3 ||
            std::abs(view_box[3] - page.height_points) > 1e-3) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "restricted SVG page geometry does not match the background PDF"};
        }
        if (std::any_of(
                page.objects.begin(), page.objects.end(), [](const auto& object) {
                    return object.type == PdfPageObjectType::Text;
                })) {
            throw CodecError{
                CodecErrorCode::MalformedInput,
                "background PDF still contains top-level text objects"};
        }

        for (const auto text_node : page_node.children()) {
            if (text_node.type() != pugi::node_element ||
                std::string_view{text_node.name()} != "text") {
                throw CodecError{
                    CodecErrorCode::UnsupportedFeature,
                    "restricted page SVG may contain only text elements"};
            }
            require_allowed_attributes(text_node, {
                "id", "x", "y", "transform", "font-family", "font-size",
                "fill", "fill-opacity",
            });
            if (std::string_view{text_node.attribute("x").value()} != "0" ||
                std::string_view{text_node.attribute("y").value()} != "0") {
                throw CodecError{
                    CodecErrorCode::UnsupportedFeature,
                    "restricted SVG positions text through transform matrices"};
            }
            const auto font_name = standard_font_name(
                text_node.attribute("font-family").value());
            const auto font_size = parse_number(
                text_node.attribute("font-size").value(), "font size");
            if (font_size <= 0.0) {
                throw CodecError{
                    CodecErrorCode::MalformedInput,
                    "restricted SVG font size must be positive"};
            }
            result.push_back({
                .page_index = page_index,
                .text = text_content(text_node),
                .font_name = font_name,
                .font_size = font_size,
                .fill = parse_fill(text_node),
                .matrix = svg_to_pdf_matrix(
                    parse_matrix(text_node.attribute("transform").value()),
                    page.height_points),
            });
            if (result.size() > options.max_text_elements) {
                throw CodecError{
                    CodecErrorCode::ResourceLimit,
                    "restricted SVG exceeds the configured text element limit"};
            }
        }
        ++page_index;
    }
    if (page_index != background.page_count()) {
        throw CodecError{
            CodecErrorCode::MalformedInput,
            "restricted SVG page count does not match the background PDF"};
    }
    return result;
}

}  // namespace

PdfTextLayer split_pdf_text_layer(
        std::span<const uint8_t> encoded, std::string_view password,
        const PdfTextLayerOptions& options) {
    require_options(options);
    auto document = PdfDocument::open(encoded, password);

    pugi::xml_document svg;
    auto declaration = svg.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "UTF-8";
    auto root = svg.append_child("svg");
    root.append_attribute("xmlns") = svg_namespace.data();
    root.append_attribute("version") = "1.1";

    double total_height{};
    double maximum_width{};
    size_t text_count{};
    const auto page_count = document.page_count();
    for (size_t page_index = 0; page_index < page_count; ++page_index) {
        const auto page = document.page(page_index);
        auto page_node = root.append_child("svg");
        page_node.append_attribute("id").set_value(
            ("page-" + std::to_string(page_index)).c_str());
        set_number_attribute(page_node, "x", 0.0);
        set_number_attribute(page_node, "y", total_height);
        set_number_attribute(page_node, "width", page.width_points);
        set_number_attribute(page_node, "height", page.height_points);
        page_node.append_attribute("viewBox").set_value(
            ("0 0 " + number(page.width_points) + ' ' +
             number(page.height_points)).c_str());

        for (const auto& object : page.objects) {
            if (object.type != PdfPageObjectType::Text || !object.text ||
                object.text->text.empty()) {
                continue;
            }
            if (++text_count > options.max_text_elements) {
                throw CodecError{
                    CodecErrorCode::ResourceLimit,
                    "PDF exceeds the configured text element limit"};
            }
            auto text_node = page_node.append_child("text");
            text_node.append_attribute("id").set_value(
                ("page-" + std::to_string(page_index) + "-text-" +
                 std::to_string(object.index)).c_str());
            text_node.append_attribute("x") = "0";
            text_node.append_attribute("y") = "0";
            text_node.append_attribute("transform").set_value(matrix_value(
                pdf_to_svg_matrix(
                    object.matrix.value_or(PdfMatrix{}), page.height_points))
                    .c_str());
            text_node.append_attribute("font-family").set_value(
                standard_font_name(object.text->font_name).c_str());
            set_number_attribute(
                text_node, "font-size", object.text->font_size_points);
            const auto fill = object.text->fill_color.value_or(PdfColor{});
            text_node.append_attribute("fill").set_value(hex_color(fill).c_str());
            set_number_attribute(
                text_node, "fill-opacity",
                static_cast<double>(fill.alpha) / 255.0);
            text_node.text().set(object.text->text.c_str());
        }

        for (auto iterator = page.objects.rbegin();
             iterator != page.objects.rend(); ++iterator) {
            if (iterator->type == PdfPageObjectType::Text) {
                document.remove_object(page_index, iterator->index);
            }
        }
        total_height += page.height_points;
        maximum_width = std::max(maximum_width, page.width_points);
    }
    set_number_attribute(root, "width", maximum_width);
    set_number_attribute(root, "height", total_height);
    root.append_attribute("viewBox").set_value(
        ("0 0 " + number(maximum_width) + ' ' + number(total_height)).c_str());

    std::ostringstream serialized;
    svg.save(serialized, "  ", pugi::format_default, pugi::encoding_utf8);
    auto text_svg = std::move(serialized).str();
    if (text_svg.size() > options.max_svg_bytes) {
        throw CodecError{
            CodecErrorCode::ResourceLimit,
            "generated SVG exceeds the configured size limit"};
    }
    return {
        .background_pdf = document.save(),
        .text_svg = std::move(text_svg),
    };
}

std::vector<uint8_t> combine_pdf_text_layer(
        std::span<const uint8_t> background_pdf, std::string_view text_svg,
        std::string_view password, const PdfTextLayerOptions& options) {
    require_options(options);
    auto document = PdfDocument::open(background_pdf, password);
    const auto elements = parse_text_svg(text_svg, document, options);
    for (const auto& element : elements) {
        static_cast<void>(document.add_text(
            element.page_index, element.text,
            {
                .font_name = element.font_name,
                .font_size_points = element.font_size,
                .fill_color = element.fill,
                .matrix = element.matrix,
            }));
    }
    return document.save();
}

}  // namespace tahoma::vision