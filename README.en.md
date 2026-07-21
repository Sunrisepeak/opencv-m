# opencv

> A C++23 module wrapper over OpenCV 5. `import opencv.cv;` replaces header inclusion while the
> API spelling remains unchanged; the build is fully featured on three platforms, with dnn and
> unifont available as opt-in features.

[中文](README.md) · **English**

[![Release](https://img.shields.io/github/v/release/Sunrisepeak/opencv-m)](https://github.com/Sunrisepeak/opencv-m/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Module](https://img.shields.io/badge/module-ok-green.svg)](https://en.cppreference.com/w/cpp/language/modules)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

| [mcpp build tool](https://github.com/mcpp-community/mcpp) · [package index](https://github.com/mcpp-community/mcpp-index) · [OpenCV upstream](https://github.com/opencv/opencv) · [Architecture](docs/architecture.en.md) · [Issues](https://github.com/Sunrisepeak/opencv-m/issues) · [Releases](https://github.com/Sunrisepeak/opencv-m/releases) |
|:---:|
| [![CI](https://github.com/Sunrisepeak/opencv-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Sunrisepeak/opencv-m/actions/workflows/ci.yml) |

## Characteristics

- **Module-based import.** Consumer code obtains the OpenCV interface through
  `import opencv.cv;` without header inclusion, while the API and its usage remain those of
  upstream OpenCV.
- **Built from source, with no CMake on the consumer side.** The OpenCV 5.0.0 sources are
  vendored under `third_party/`, the configure-time artefacts are committed as real generated
  files under `gen/`, and mcpp compiles the whole of OpenCV — NASM SIMD included, with runtime
  dispatch preserved — from this repository's `mcpp.toml`. The FFmpeg backend of videoio is
  supplied by `compat.ffmpeg`. A single repository carries both the module layer and the
  complete build.
- **Equivalent functionality on three platforms.** Linux, macOS and Windows are each verified in
  CI and each provide imgcodecs (PNG and JPEG), videoio (FFmpeg `cap_ffmpeg`) and optional dnn.
  The gemm backend of dnn is selected per platform (x86 mlas/AVX, arm mlas/NEON, built-in
  fast_gemm on Windows).
- **Opt-in features.** `dnn` enables the `import opencv.dnn;` deep-learning module; `unifont`
  enables `putText` rendering for Unicode and CJK text.

## Getting started

```bash
mcpp new myvision --template opencv && cd myvision && mcpp run -- input.png    # grayscale pipeline skeleton
```

Or declare the dependency in an existing project:

```toml
[dependencies]
opencv = "0.0.7"
```

```cpp
import opencv.cv;   // or per module: import opencv.core; import opencv.imgproc; …

int main() {
    cv::Mat img = cv::imread("in.png", cv::IMREAD_COLOR);
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::imwrite("out.png", gray);
    return 0;
}
```

## Modules

| Module | Description |
|---|---|
| `opencv.cv` | aggregate entry point, recommended by default |
| `opencv.core` | core: `Mat`, types, arithmetic and the operator replacement surface |
| `opencv.imgproc` | image processing: filtering, geometric transforms, colour spaces, drawing |
| `opencv.imgcodecs` | image reading and writing (PNG and JPEG) |
| `opencv.videoio` | video I/O (V4L2 and FFmpeg backends, all three platforms) |
| `opencv.highgui` | high-level GUI (headless) |
| `opencv.flann` / `opencv.geometry` | dependency-closure modules |
| `opencv.dnn` | deep learning (requires the `dnn` feature) |

Each `opencv.<mod>` is a self-contained global module fragment that exports only its own surface;
the modules do not import one another, and `opencv.cv` aggregates them through `export import`.
Object-like constant macros (`CV_8UC3`, `CV_PI`, `CV_MAKETYPE`, …) are exported as `cv::`
constexpr entities retaining the original spelling. A translation unit that needs the original
spelling of a function-like macro (`CV_Assert`, the version macros) should include
`<opencv-m/macros.hpp>` before the import.

## Features

| Feature | Description |
|---|---|
| `dnn` | adds the `import opencv.dnn;` interface (`Net`, `blobFromImage`, `readNet`, …) and builds the underlying dnn together with the vendored protobuf. Available on all three platforms (per-OS features, mcpp#253): Linux and macOS use mlas (x86 AVX/AVX2/AVX512, arm NEON), while Windows uses OpenCV's built-in `fast_gemm` — upstream mlas x86 assembly is GAS/ELF syntax from which clang-cl cannot emit COFF, so the upstream path of falling back to fast_gemm in the absence of assembly is followed |
| `unifont` | embeds the WenQuanYi Micro Hei font, enabling `putText` for Unicode and CJK text through `FontFace("uni")` |

```toml
[dependencies]
opencv = { version = "0.0.7", features = ["dnn"] }
```

The dnn layer adds over 300 translation units, is built only for consumers that enable it
explicitly, and does not enter the `opencv.cv` aggregate by default. The per-OS feature
semantics require mcpp ≥ 0.0.101.

## Examples

| Example | Contents |
|---|---|
| [`examples/probe`](examples/probe) | version and build information (no input required) |
| [`examples/gray_pipeline`](examples/gray_pipeline) | a minimal read, grayscale and write pipeline |
| [`examples/workspace`](examples/workspace) | a multi-member workspace |

```bash
cd examples/gray_pipeline && mcpp run -- input.png
```

## Build and toolchains

The package does not pin a toolchain; mcpp resolves the environment default. CI covers five
combinations: Linux/gcc 16, Linux/llvm 22, Linux/gcc `--target x86_64-linux-musl` (statically
linked), macOS/llvm (arm64) and Windows/llvm.

The upstream OpenCV 5.0.0 sources are vendored under `third_party/opencv-5.0.0/` as a pruned
import of the official tag tarball, pinned by sha256 and free of patches; the process is
reproducible through `tools/vendor/import_opencv.sh`. The platform-dependent configure-time
snapshots live in `gen/` and were ported once from the retired compat.opencv descriptor by
`tools/vendor/port_descriptor.py`. On the package-index side only a thin `opencv.lua` pointing at
this repository's releases remains.

Operators and a number of named free functions are defined `static inline` upstream. Such
entities have internal linkage and cannot be re-exported through `export using` — clang rejects
this outright, gcc merely tolerates it. This repository replaces that surface with equivalent
external-linkage definitions across `src/core_ops.inc` (saturate_cast, Point/Size/Rect/Range/
Scalar, Mat/MatExpr), `src/matx_ops.inc` (Matx/Vec algebra) and `src/core_fns.inc` (named free
functions). This is the reason `import opencv.cv;` compiles under both clang and gcc.

The overall structure, the responsibility of each layer and the reasoning behind the design are
documented in the [architecture description](docs/architecture.en.md).

> [!NOTE]
> The project is at an early stage and its interface may change. Questions and suggestions are
> welcome as [issues](https://github.com/Sunrisepeak/opencv-m/issues).

## License

The wrapper code is MIT-licensed. The vendored OpenCV itself (`third_party/opencv-5.0.0/`) is
Apache-2.0, with each third-party component retaining its own original license.
