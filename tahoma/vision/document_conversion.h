#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tahoma::vision {

enum class DocumentFormat : uint8_t {
    Pdf,
    Html,
    Xhtml,
    Xml,
    XslFo,
};

struct EncodedDocument {
    DocumentFormat format{DocumentFormat::Pdf};
    std::vector<uint8_t> bytes;
    std::string base_uri;
};

struct DocumentConversionOptions {
    double page_width_points{612.0};
    double page_height_points{792.0};
    double margin_top_points{36.0};
    double margin_right_points{36.0};
    double margin_bottom_points{36.0};
    double margin_left_points{36.0};
    bool allow_network{};
    bool allow_local_files{};
    uint64_t max_output_bytes{1073741824};
};

class DocumentConverter {
public:
    virtual ~DocumentConverter() = default;

    [[nodiscard]] virtual bool supports(
        DocumentFormat source, DocumentFormat target) const noexcept = 0;
    [[nodiscard]] virtual EncodedDocument convert(
        const EncodedDocument& source, DocumentFormat target,
        const DocumentConversionOptions& options = {}) const = 0;
};

}  // namespace tahoma::vision
