# Test resources

- `pattern.png`: lossless 64x48 RGB gradient/checker fixture.
- `pattern.jpg`: quality-95 encoding of the same source pixels.
- `one-page.pdf`: the same RGB fixture embedded on one 64x48-point page.

Regenerate all three resources with:

```sh
cmake -S . -B build-debug-pdf -DTAHOMA_VISION_PDF=ON \
  -DTAHOMA_VISION_BUILD_TESTS=ON
cmake --build build-debug-pdf --target tahoma_vision_generate_test_resources
```
