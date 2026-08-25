# PDFium binary adapter

This directory intentionally contains no PDFium source or binaries.

When `TAHOMA_VISION_PDF=ON`, `cmake/pdfium.cmake` downloads a pinned,
checksum-verified non-V8 binary from
[`bblanchon/pdfium-binaries`](https://github.com/bblanchon/pdfium-binaries)
and exposes it as `PDFium::PDFium`.

- Upstream source: https://pdfium.googlesource.com/pdfium/
- Binary release: `chromium/8021` (`PDFium 154.0.8021.0`)
- Supported defaults: Linux, macOS, and Windows on x64 or arm64
- Local/offline override: `-DTAHOMA_PDFIUM_ROOT=/path/to/extracted/package`
- Custom archive override: set both `TAHOMA_PDFIUM_URL` and
  `TAHOMA_PDFIUM_SHA256`

The binary publisher is a third party and is not affiliated with Google or
Foxit. Updating the pinned release requires updating every platform checksum
in `cmake/pdfium.cmake` and recording the provenance change.
