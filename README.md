# Tahoma Vision

Tahoma Vision is a standalone C++23 library for provider-neutral image and
document decoding without a tensor-framework dependency.

This is my attempt to replace
[pytorch/vision](https://github.com/pytorch/vision). It differs from
torchvision library in these ways:
* Torch-free, with no bulky libtorch dependency. Bring your own tensor library!
* PNG is powered by [thammegowda/pigzpp](https://github.com/thammegowda/pigzpp)
	PNG -- a faster PNG library
* PDF support, using PDFium. 

## Design boundaries

- Public codec values are owned or viewed interleaved host pixels.
- The base target does not depend on Tahoma, Torch/ATen/c10, CUDA, ORT, or any inference provider.
- Tensor adapters belong to consuming repositories.
- PDFium is optional and disabled by default.
- In-memory APIs are primary; file helpers are convenience APIs.
- Public headers and implementations are colocated under `tahoma/vision/` 
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

Required dependency sources are pinned by the Git submodules at
`libs/pigzpp` and `libs/libjpeg-turbo`. With `TAHOMA_VISION_PDF=ON`, the
`cmake/pdfium.cmake` setup function downloads a checksum-pinned non-V8 PDFium
binary and the build fetches pinned pugixml source for restricted SVG text-layer
parsing; `libs/pdfium` contains its dependency-policy notes.
Official PDFium source and build documentation remain at
https://pdfium.googlesource.com/pdfium/.

The installable CMake target is `TahomaVision::Vision`. This repository is
pre-release and API/ABI stability is not yet guaranteed.

## Credits and license

Tahoma Vision's first-party source is licensed under the [MIT License](LICENSE).
Third-party components retain their own licenses:

- PNG support uses [pigzpp](https://github.com/thammegowda/pigzpp), based on
	Mark Adler's pigz, together with zlib-ng and Google's Zopfli.
- JPEG support uses [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo).
	This software is based in part on the work of the Independent JPEG Group.
- Optional PDF support uses
	[PDFium](https://pdfium.googlesource.com/pdfium/), binary packages maintained
	by Benoit Blanchon, and
	[pugixml](https://github.com/zeux/pugixml) by Arseny Kapoulkine.

See [NOTICES.md](NOTICES.md) for third-party licenses, credits, and redistribution requirements.
