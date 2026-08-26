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

The current writer creates one image-only page. General PDF editing, text,
annotations, and multi-page assembly are not yet exposed.
