# Changelog

All notable changes to Tahoma Vision are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). The version is set in `CMakeLists.txt`.

## [0.2.0] - 2026-11-21

### Fixed

- PNG decoding no longer rejects valid images with
  `PNG color type is not supported`. Palette (indexed-color), 1/2/4/16-bit, color-keyed
  (`tRNS`), and Adam7-interlaced PNGs now decode to the requested output format.
  Documents saved as 8-bit palette PNGs previously failed translation in DocMT Studio.

### Changed

- Bundled pigzpp updated to 1.2.0 (see `libs/pigzpp/CHANGELOG.md`).
- The PNG preflight check still rejects invalid headers (unsupported bit depth,
  compression, filter, or interlace method) with `UnsupportedFeature`.
