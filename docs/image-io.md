# PNG and JPEG I/O

## Read an image

`load()` detects PNG or JPEG from the encoded bytes and returns packed RGB8
pixels by default.

```cpp
#include <tahoma/vision/io.h>

auto image = tahoma::vision::load("input.png");
// image.pixels is HWC RGB8; image.row_stride is width * 3.
```

Request grayscale output when decoding either format:

```cpp
auto gray = tahoma::vision::load(
    "input.jpg", {.output_format = tahoma::vision::PixelFormat::Gray8});
```

## Write PNG or JPEG

The output extension selects the codec when `EncodeOptions::format` is
`Unknown`.

```cpp
tahoma::vision::save("output.png", image.view());

tahoma::vision::EncodeOptions jpeg{.format = tahoma::vision::Format::Jpeg};
jpeg.jpeg.quality = 92;
tahoma::vision::save("output.jpg", image.view(), jpeg);
```

PNG round-trips are pixel-exact. JPEG is lossy; compare geometry and use an
application-appropriate pixel-error tolerance rather than byte equality.

## Work in memory

```cpp
auto png_bytes = tahoma::vision::encode_png(image.view());
auto decoded_png = tahoma::vision::decode(png_bytes);

auto jpeg_bytes = tahoma::vision::encode_jpeg(
    image.view(), {.quality = 95});
auto decoded_jpeg = tahoma::vision::decode(jpeg_bytes);
```
