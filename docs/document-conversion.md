# HTML, XML, and PDF conversion

PDFium renders and edits PDF files, but it is not an HTML or XML layout
engine. These conversions require optional backends and are not exact inverse
operations.

Tahoma Vision exposes the backend-neutral contract in
`<tahoma/vision/document_conversion.h>`:

```cpp
tahoma::vision::DocumentConverter& converter = get_application_converter();
tahoma::vision::EncodedDocument input{
	.format = tahoma::vision::DocumentFormat::Html,
	.bytes = html_bytes,
	.base_uri = "file:///workspace/document/",
};

if (!converter.supports(input.format, tahoma::vision::DocumentFormat::Pdf)) {
	throw std::runtime_error("HTML to PDF backend is unavailable");
}
auto pdf = converter.convert(
	input, tahoma::vision::DocumentFormat::Pdf,
	{.allow_network = false, .allow_local_files = false});
```

The base library provides the contract but no markup conversion engine yet.
Applications can supply a backend without coupling image codecs to Chromium or
an XSL-FO runtime.

## HTML or XHTML to PDF

Use a browser print engine such as headless Chromium. A future
`HtmlToPdfBackend` API should accept:

- HTML/XHTML bytes and a base URL;
- page size, margins, orientation, and print CSS settings;
- an explicit network and local-resource access policy;
- deadline/cancellation and maximum output limits.

The backend should return encoded PDF bytes, which can then be inspected or
rendered through the PDF API.

## XML to PDF

Generic XML has no visual semantics. It requires one of:

- an XSLT transform to XHTML followed by the HTML backend;
- an XSL-FO stylesheet and an FO processor such as Apache FOP;
- a domain-specific renderer supplied by the application.

A future `XmlToPdfBackend` must therefore receive the XML format and stylesheet
or transformation policy explicitly. Tahoma Vision should not guess how
arbitrary XML maps to pages.

## PDF to HTML or XML

PDF stores positioned drawing commands rather than semantic document markup.
Conversion is lossy extraction, not round-trip serialization. A future
`PdfExtractionBackend` can expose:

- page text spans with coordinates, fonts, and reading-order confidence;
- embedded images and links;
- HTML serialization for visual approximation;
- XML serialization of the neutral extracted document model.

The neutral extraction model should be the stable API. HTML and XML are
serializers over that model, so adding another output format does not require
another PDF parser.

## Proposed optional targets

| Target | Responsibility |
|---|---|
| `TahomaVision::HtmlToPdf` | Chromium-backed HTML/XHTML print-to-PDF |
| `TahomaVision::XmlToPdf` | XSLT/XSL-FO or application-provided XML layout |
| `TahomaVision::PdfExtract` | PDFium-backed neutral text/layout extraction |

None of these targets belongs in the base image codec target, and none should
enable network or unrestricted filesystem access by default.
