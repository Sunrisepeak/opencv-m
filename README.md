# opencv-m

OpenCV 5 as C++23 named modules — the `import cv2` experience for C++:

```cpp
import opencv.cv;   // or per-module: import opencv.core; import opencv.imgproc; …

int main() {
    cv::Mat img = cv::imread("in.png", cv::IMREAD_COLOR);
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::imwrite("out.png", gray);
    // … the exact same C++ API you already know — just no #include
}
```

- **Source build, no CMake at build time.** The
  [`compat.opencv5`](https://github.com/mcpplibs/mcpp-index) index package
  carries a frozen config snapshot + an embedded `build.mcpp` that synthesizes
  OpenCV's build-time generated files on the consumer; mcpp compiles all of
  OpenCV 5.0.0 (457 TUs incl. 27 NASM `.asm`, **SIMD runtime dispatch kept**)
  directly — cold build ≈ 33 s on 32 cores. This repo is only the thin module
  layer.
- **API and habits unchanged.** Every exported name is the upstream entity
  (`export using cv::…`): types, functions, enums and their enumerators.
- **Constant macros re-homed.** `CV_8UC3`, `CV_PI`, `CV_MAKETYPE` … are macros
  upstream and cannot cross a module boundary; the module exports them as
  `cv::CV_8UC3`-style constexpr. TUs that also need the raw macro surface
  (`CV_Assert`, version macros) include
  [`<opencv-m/macros.hpp>`](include/opencv-m/macros.hpp) **before** importing.
- **v0.1 scope** (the compat.opencv5 profile): core, imgproc, imgcodecs
  (PNG + JPEG), highgui (headless), videoio (V4L2) + flann, geometry.
  Linux-x86_64 first; dnn stays optional-future.

## Use

```toml
[dependencies]
opencv = "0.0.1"
```

Or start from the template: `mcpp new myvision --template imgproc`.
Examples: [`examples/probe`](examples/probe) (no input needed),
[`examples/gray_pipeline`](examples/gray_pipeline).

Validate the package: `mcpp build && mcpp test` (API surface, macros-mix TU,
PNG/JPEG roundtrips).

## Layout

```text
src/*.cppm            module layer (self-contained GMF + export using);
                      opencv.cv = umbrella, opencv = lib root
src/gen_exports/      generated export lists + skip reports
include/opencv-m/     optional macros side header
tools/                fetch_upstream.sh (pinned official tarball),
                      gen_exports.py (hdr_parser.py-driven surface +
                      tools/curated/ whitelists), prune_loop.py
                      (compile-verify + prune)
templates/  examples/ project template + runnable consumers
```

Upstream sources are NOT vendored: they reach consumers through the
`compat.opencv5` mcpp-index package (official GitHub tag tarball, GLOBAL + CN
mirror, sha256-pinned). Its descriptor + generation pipeline live in
mcpp-index (`tools/compat-opencv5/`). OpenCV bump: bump `compat.opencv5`
there first, then update the pin in `tools/fetch_upstream.sh`, run
`python3 tools/gen_exports.py && python3 tools/prune_loop.py`, review the
export-list diffs.

## Notes

- **Module design:** each opencv.\<mod\> is a self-contained GMF (textual
  OpenCV headers) exporting only its own surface; modules do NOT import each
  other (gcc 16 merges textual+imported global-module entities with
  `conflicting default argument` errors otherwise). `import opencv.cv;` is the
  supported entry; importing a single module gives that module's names only.
- Mixing textual `#include <opencv2/…>` with imports in one consumer TU is
  supported for the macros side header (include BEFORE import); broader
  header/import mixing is at-your-own-risk territory across the ecosystem.
- The export surface = hdr_parser (python-wrapper annotations) + curated
  whitelists, compile-verified; internal-linkage inline helpers
  (`cv::saturate_cast`, `cv::abs`, …) cannot be re-exported by a module —
  they remain reachable via the macros side header. See
  `src/gen_exports/*.skipped.txt`.
- License: this wrapper repo is MIT; upstream OpenCV arrives via
  `compat.opencv5` under **Apache-2.0**.
