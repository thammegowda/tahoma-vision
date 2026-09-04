# Third-party notices

Tahoma Vision's first-party source is licensed under the MIT License. Third-party components retain their own licenses.
Exact source revisions are recorded by Git submodule links and CMake dependency pins.

## Required dependencies

| Component | Source | License |
|---|---|---|
| pigzpp | https://github.com/thammegowda/pigzpp | zlib (`libs/pigzpp/LICENSE`) |
| zlib-ng | https://github.com/zlib-ng/zlib-ng | zlib (`libs/pigzpp/third_party/zlib-ng/LICENSE.md`) |
| Zopfli | https://github.com/google/zopfli | Apache-2.0 (`libs/pigzpp/third_party/zopfli/COPYING`) |
| libjpeg-turbo | https://github.com/libjpeg-turbo/libjpeg-turbo | BSD-style, IJG, and zlib |
| | | `libs/libjpeg-turbo/LICENSE.md` and `README.ijg` |

Pigzpp is based on pigz by Mark Adler. Tahoma Vision uses a modified pigzpp revision with embedding changes.

This software is based in part on the work of the Independent JPEG Group.

## Present but not built

| Component | Source | License |
|---|---|---|
| ISA-L | https://github.com/intel/isa-l | BSD-3-Clause (`libs/pigzpp/third_party/isa-l/LICENSE`) |
| nanobind | https://github.com/wjakob/nanobind | BSD-3-Clause (`libs/pigzpp/third_party/nanobind/LICENSE`) |
| robin-map | https://github.com/Tessil/robin-map | MIT (`libs/pigzpp/third_party/nanobind/ext/robin_map/LICENSE`) |

## Optional PDF dependencies

| Component | Source | License |
|---|---|---|
| pugixml | https://github.com/zeux/pugixml | MIT (`LICENSE.md` in the fetched source) |
| PDFium | https://pdfium.googlesource.com/pdfium/ | BSD-style and bundled permissive licenses |
| PDFium binary package | https://github.com/bblanchon/pdfium-binaries | MIT |

PDFium is downloaded only when `TAHOMA_VISION_PDF=ON`. Its archive contains the complete licenses for PDFium, zlib,
Little CMS, simdutf, fast_float, libpng, libjpeg-turbo, FreeType, Anti-Grain Geometry, LLVM libc, ICU/Unicode, Abseil,
and OpenJPEG. Redistributors of the optional PDF build must retain the archive's
`LICENSE` file and `licenses/` directory.

The PDFium binary publisher is not affiliated with Google or Foxit.