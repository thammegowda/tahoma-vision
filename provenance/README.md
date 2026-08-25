# Source provenance

Record every imported or adapted source file here before it is added.

For TorchVision-derived code, record:

- upstream repository URL and commit;
- original file paths;
- upstream license and required notices;
- local destination paths;
- semantic changes made to remove Torch dependencies;
- the process for checking relevant upstream fixes.

## Pinned dependency state

The top-level pins and licenses are listed in `THIRD_PARTY_NOTICES.md`.
Pigzpp `v1.1.0` contains these nested gitlinks:

| Component | Revision | License |
|---|---|---|
| zlib-ng | `a56d0201b7895055acd8f8d190cca066f8a7f520` | zlib |
| ISA-L | `c196241ae89b1aa4f62efeb849a937c011b3a926` | BSD-3-Clause |
| Zopfli | `ccf9f0588d4a4509cb1040310ec122243e670ee6` | Apache-2.0 |
| nanobind | `2a61ad2494d09fecb2e13322c1383342c299900d` | BSD-3-Clause |

The M0 core-only proof initializes only Zopfli and uses the system zlib ABI.
Distribution builds must initialize selected dependencies recursively and
retain their complete notices.

### Local pigzpp build adaptation

The pin temporarily carries an upstreamable build-only patch. It adds
embedding-aware defaults, `PIGZPP_BUILD_CLI` and install/package options,
target-owned C++23, and guards against changing a parent project's build type,
C++ standard, IPO, or `BUILD_SHARED_LIBS` policy. One dead local variable was
removed from the zlib compression path so strict `-Werror` parents can embed
the target; this does not change generated code or behavior.
`tests/pigzpp-embedding` verifies the build invariants and performs a PNG
encode/decode round-trip through `pigzpp_lib`.
