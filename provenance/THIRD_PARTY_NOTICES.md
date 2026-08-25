# Third-party notices

## pigzpp

- Repository: https://github.com/thammegowda/pigzpp
- Revision: `3eda915ebc977d8b7cd93f41a8e256801345fc07` (`v1.1.0`)
- Location: `libs/pigzpp`
- License: zlib License; see `libs/pigzpp/LICENSE`

Pigzpp has its own pinned submodules. Initialize this repository recursively.
Those dependencies use the zlib license (zlib-ng), BSD-3-Clause (ISA-L and
nanobind), and Apache-2.0 (Zopfli); retain their complete license and notice
texts in source and binary distributions.

## libjpeg-turbo

- Repository: https://github.com/libjpeg-turbo/libjpeg-turbo
- Revision: `c85e6b905bf237038faa936dab160ebfc5da0344` (`3.2.0`)
- Location: `libs/libjpeg-turbo`
- Licenses: BSD-style, IJG, and zlib licenses; see
	`libs/libjpeg-turbo/LICENSE.md`

## PDFium binary package (optional download)

- Upstream source: https://pdfium.googlesource.com/pdfium/
- Binary publisher: https://github.com/bblanchon/pdfium-binaries
- Release: `chromium/8021` (`PDFium 154.0.8021.0`)
- Adapter: `libs/pdfium`
- Distribution: downloaded only when `TAHOMA_VISION_PDF=ON`

The binary publisher is not affiliated with Google or Foxit. CMake verifies a
platform-specific SHA-256 before extraction. Retain the PDFium and bundled
third-party license files from the downloaded archive in binary distributions.

TorchVision-derived code and an SVG renderer are not yet distributed by this
repository. Add their source revisions and notices before import.
