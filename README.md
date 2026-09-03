# Tahoma Vision

Tahoma Vision is a standalone C++23 library for provider-neutral image and
document decoding without a tensor-framework dependency.

This is my attempt to replace [pytorch/vision](https://github.com/pytorch/vision). It differs from torchvision library in these ways:
* Torch-free which is bulky. No libtorch dependency (bulky, and lot of dynamic libs) or any such tensor backend. Bring your own tensor library.  
* PNG is powered by [thammegowda/pigzpp](https://github.com/thammegowda/pigzpp) PNG -- a faster PNG library
* PDF support, using PDFium. 

## Design boundaries

- Public codec values are owned or viewed interleaved host pixels.
- The base target does not depend on Tahoma, Torch, ATen, c10, CUDA, ORT, or  any inference provider.
- Tensor adapters belong to consuming repositories.
- PDFium is optional and disabled by default.
- In-memory APIs are primary; file helpers are convenience APIs.
- Public headers and implementations are colocated under `tahoma/vision/`.
- Required source repositories are pinned Git submodules under `libs/`.
- Large optional binary dependencies use small CMake adapters under `libs/`.

## Build

```sh
git submodule update --init --recursive
make debug
make test
make release
```

Run `make help` for install, PDFium, refresh, and cleanup targets. Override
parallelism with `JOBS`, and the installation destination with
`INSTALL_PREFIX`.

See [docs/](docs/) for PNG/JPEG read-write examples and PDF page rendering or
image-to-PDF examples.

Direct dependency pins are `libs/pigzpp` at `v1.1.0` and
`libs/libjpeg-turbo` at `3.2.0`. With `TAHOMA_VISION_PDF=ON`, the
`cmake/pdfium.cmake` setup function downloads a checksum-pinned non-V8 PDFium
binary and the build fetches pinned pugixml source for restricted SVG text-layer
parsing; `libs/pdfium` contains its dependency-policy notes.
Official PDFium source and build documentation remain at
https://pdfium.googlesource.com/pdfium/.

The installable CMake target is `TahomaVision::Vision`. This repository is
pre-release and API/ABI stability is not yet guaranteed.
