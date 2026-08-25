# Tahoma Vision

Tahoma Vision is a standalone C++23 library for provider-neutral image and
document decoding without a tensor-framework dependency.

The current bootstrap uses system libpng and libjpeg-turbo to prove the
Torch-free API and build boundary. PNG moves to pinned pigzpp before the M0
dependency milestone is complete.

## Design boundaries

- Public codec values are owned or viewed interleaved host pixels.
- The base target does not depend on Tahoma, Torch, ATen, c10, CUDA, ORT, or
  any inference provider.
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

Direct dependency pins are `libs/pigzpp` at `v1.1.0` and
`libs/libjpeg-turbo` at `3.2.0`. With `TAHOMA_VISION_PDF=ON`, the
`cmake/pdfium.cmake` setup function downloads a checksum-pinned non-V8 PDFium
binary; `libs/pdfium` contains its dependency-policy notes.
Official PDFium source and build documentation remain at
https://pdfium.googlesource.com/pdfium/.

The installable CMake target is `TahomaVision::Vision`. This repository is
pre-release and API/ABI stability is not yet guaranteed.
