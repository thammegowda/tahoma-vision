# PDF rendering and image-to-PDF

PDF support is optional. Configure with `TAHOMA_VISION_PDF=ON` or use
`make pdf-test`. The default renderer output is packed RGB8.

## Render a page to RGB

```cpp
#include <tahoma/vision/io.h>
#include <tahoma/vision/pdf.h>

auto page = tahoma::vision::render_pdf_page(
    "document.pdf",
    {.page_index = 0,
     .dpi = 144.0,
     .output_format = tahoma::vision::PixelFormat::RGB8});

tahoma::vision::save("page-1.png", page.view());
```

Use `pdf_page_count()` with encoded PDF bytes when selecting pages.

## Store PNG or JPEG pixels in a PDF

Decode the image first, then create a one-page PDF. Page size is derived from
the image dimensions and requested DPI.

```cpp
auto image = tahoma::vision::load("scan.jpg");  // PNG works identically.
tahoma::vision::save_pdf("scan.pdf", image.view(), {.dpi = 300.0});
```

For an in-memory result:

```cpp
auto pdf_bytes = tahoma::vision::encode_pdf(image.view(), {.dpi = 300.0});
```

The image writer creates one image-only page.

## Inspect and edit page objects

`PdfDocument` owns an editable PDFium document. It exposes low-level page
objects without imposing reading order, translation, or layout policy.

```cpp
auto document = tahoma::vision::PdfDocument::open(pdf_bytes);
auto page = document.page(0);

for (const auto& object : page.objects) {
    if (object.type == tahoma::vision::PdfPageObjectType::Text) {
        // object.text contains UTF-8 text and available font/color metadata.
    }
}

document.replace_text(0, text_object_index, "Hola, España");
document.remove_object(0, obsolete_object_index);
document.add_text(
    0, "Texto nuevo",
    {.font_name = "Helvetica",
     .font_size_points = 11.0,
     .fill_color = {.red = 20, .green = 20, .blue = 20},
     .matrix = {.e = 72.0, .f = 700.0}});

auto translated_pdf = document.save();
```

Coordinates are PDF points in the page's native bottom-left coordinate
system. Matrices use the PDF affine layout `(a, b, c, d, e, f)`.

Page object indices address top-level objects in content-stream order. Removing
an object shifts later indices, so inspect the page again before a subsequent
index-based edit. Objects nested inside form objects are reported as forms;
recursive form inspection is not yet exposed.

Replacing text preserves the existing PDF text object's font, transform, and
paint properties. The font may not contain glyphs for the replacement language.
Adding text currently accepts the PDF standard fonts; custom font embedding and
subsetting are future low-level APIs. Extracted text objects are PDF primitives,
not paragraphs or guaranteed reading-order units.

Tahoma Vision owns PDF loading, object inspection and mutation, rendering, and
serialization. Applications such as DocMT should own OCR fallback, reading
order, text grouping, translation, font selection, text fitting, collision
resolution, and visual-fidelity policy.

## Split text into an SVG layer

For workflows that edit text while preserving native non-text PDF content,
split the document into a background PDF and a restricted SVG text layer:

```cpp
auto layers = tahoma::vision::split_pdf_text_layer(pdf_bytes);

// Edit layers.text_svg with a conforming XML/SVG tool.

auto output = tahoma::vision::combine_pdf_text_layer(
    layers.background_pdf, layers.text_svg);
```

The background retains images, paths, shadings, forms, annotations, page
geometry, and other PDF resources. Top-level native text objects are removed.
The SVG is a vertically stacked document containing one nested `<svg>` per PDF
page, named `page-0`, `page-1`, and so on.

The accepted SVG profile is intentionally small:

- a root `<svg>` in the SVG namespace;
- one ordered nested `<svg>` for every background PDF page;
- direct `<text>` children with plain UTF-8 text content;
- `x="0"`, `y="0"`, an affine `transform="matrix(...)"`;
- `font-family`, `font-size`, `fill`, and optional `fill-opacity`;
- optional `id` attributes.

The combiner rejects scripts, external resources, arbitrary SVG graphics,
nested text markup, CSS, mismatched page geometry, and unknown attributes. It
also applies configurable SVG byte and text-element limits. The parser does not
load DTDs or external entities.

SVG uses a top-left page coordinate system. Split and combine convert affine
matrices to and from PDF's bottom-left coordinate system. Applications may edit
text content and supported presentation attributes without doing that
conversion themselves.

This first profile handles top-level native PDF text and the standard PDF fonts.
Text nested in form objects, text represented as paths or pixels, per-glyph
positioning, clipping, and custom embedded-font round trips are not yet
supported. Unsupported source font names are mapped to the closest standard
serif, sans-serif, or monospaced font. Consequently, the background is native
PDF and unchanged recombination is visually stable for supported text, but this
is not yet a universal lossless PDF text round trip.

A generated English-to-Spanish example is available under
`tests/resources/text-layer-demo/`. It includes the source and background PDFs,
both SVG text layers, the recombined PDF, and PNG previews. Regenerate it with
the `tahoma_vision_generate_test_resources` target described in the test
resource README.
