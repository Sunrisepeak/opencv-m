# opencv-m Architecture

[中文](architecture.md) · **English**

This document describes the structure of opencv-m, the responsibility of each layer, and the
reasoning behind the principal design decisions. It is addressed to developers who intend to
modify this repository, or who need to understand the general problem of wrapping a large
header-based C++ library in C++23 modules.

---

## 1. Objectives and constraints

opencv-m must satisfy three requirements simultaneously; in existing approaches these conflict:

1. **Module-based consumption**: `import opencv.cv;` replaces `#include <opencv2/opencv.hpp>`
   while the API spelling remains unchanged (`cv::Mat`, `cv::cvtColor`).
2. **No OpenCV installation and no CMake on the consumer side**: once the dependency is
   declared, mcpp performs the entire build.
3. **No reduction in functionality**: imgcodecs (PNG/JPEG), videoio (FFmpeg backend) and dnn
   are available on Linux, macOS and Windows alike, with runtime SIMD dispatch preserved.

Two constraints govern the design. First, OpenCV's build configuration is produced by CMake:
`cvconfig.h`, `cv_cpu_config.h` and the per-module SIMD dispatch translation units are all
configure-time artefacts, which a consumer cannot obtain without running CMake. Second, the
export semantics of C++ modules are stricter than those of headers: macros do not cross a module
boundary, and entities with internal linkage (`static inline`) cannot be re-exported through
`export using` — clang rejects this outright, gcc merely tolerates it.

The architecture of this repository is a systematic response to those two constraints.

## 2. Layering

```
consumer project
  └─ import opencv.cv;                     ← module interface layer
        └─ src/*.cppm  +  src/*.inc        ← export surface (generated + hand-written replacements)
              └─ third_party/opencv-5.0.0  ← vendored upstream source (patch-free)
                   +  gen/                 ← frozen configuration snapshots (CMake substitute)
                   +  mcpp.toml / build.mcpp ← build description (CMakeLists substitute)
                   +  compat.ffmpeg        ← the single external dependency (videoio backend)
```

### 2.1 Vendored upstream source — `third_party/opencv-5.0.0/`

A pruned import of the official OpenCV 5.0.0 tag tarball, pinned by sha256 and carrying **no
patches**. Eight modules are retained (core, imgproc, imgcodecs, highgui, videoio, dnn, flann,
geometry) together with seven third-party components (libjpeg-turbo, libpng, zlib, protobuf,
mlas, flatbuffers, dlpack). The test and perf trees and the language-binding directories are
removed, with the exception of `modules/python/src2`, whose `hdr_parser.py` is an input to the
export-surface generator. The result is 1969 files, 48 MB.

The patch-free property is a deliberate constraint: on an upstream version bump,
`tools/vendor/import_opencv.sh` re-imports the tree and no patch set has to be replayed. All
platform differences are pushed outward into `gen/` and `mcpp.toml`.

### 2.2 Frozen configuration snapshots — `gen/`

The headers and derived translation units that CMake emits at configure time are committed here
as **real, already-generated files**:

| Directory | Contents | Size |
|---|---|---|
| `gen/common/` | artefacts byte-identical across all three platforms (SIMD dispatch wrapper TUs, `opencv_modules.hpp`, …) plus `gen/common/synth/` (fonts, OpenCL kernels) | 257 files |
| `gen/linux/`, `gen/macosx/`, `gen/windows/` | platform-dependent `cvconfig.h`, `cv_cpu_config.h`, …, each with `INCLUDE_DIRS.txt` and `DNN_SOURCES.txt` | 43–45 files each |

The snapshots were ported once from the retired `compat.opencv` descriptor by
`tools/vendor/port_descriptor.py`, a process that included cross-platform de-duplication,
conflict detection and byte-level verification. Committing generated artefacts rather than
running CMake at build time is what makes "no CMake on the consumer side" attainable; the cost
is that an upstream bump requires the port to be re-run.

### 2.3 Build description — `mcpp.toml` and `build.mcpp`

`mcpp.toml` (about 1500 lines) carries everything that can be declared: source globs, public
include directories, 51 per-glob compile-option groups assigned to their OS under
`[target.'cfg(...)'.build.flags]`, platform-conditional source sets under
`[target.'cfg(os)'.build]`, the `[features]` definitions, and the single external dependency
`compat.ffmpeg`.

`build.mcpp` covers only the three things the manifest grammar **cannot** express, and nothing
further:

1. reading `gen/<os>/INCLUDE_DIRS.txt` for the target platform and emitting private include
   directories (the manifest has no platform-conditional `include_dirs` key);
2. injecting the platform-dependent sources listed in `gen/<os>/DNN_SOURCES.txt` when the `dnn`
   feature is active (the manifest has no platform × feature cross dimension);
3. embedding `assets/WenQuanYiMicroHei.ttf.gz` as a hex header when the `unifont` feature is
   active (equivalent to CMake's `ocv_blob2hdr`).

The file is written modules-first and uses the typed API via `import mcpp;`. Note that
`import std;` is not available in the build.mcpp compilation context — only the bundled `mcpp`
module is wired up there — so textual std includes remain, and must precede the import.

### 2.4 Module interface layer — `src/`

Each `opencv.<mod>` is self-contained: the corresponding upstream headers are included in the
global module fragment, and names are exported individually in the purview via
`export using cv::name;`. The modules do not import one another; `opencv.cv` is the sole
aggregate entry point and `export import`s the submodules.

The export surface is composed of two parts:

- **Generated** — `src/gen_exports/*.inc` (2267 lines). `tools/gen_exports.py` invokes upstream
  `hdr_parser.py` to enumerate the `CV_EXPORTS_W` surface, then overlays the manually curated
  names in `tools/curated/<mod>.txt` — entities the wrapper generator cannot see, such as the
  template typedef families (`Point2i`, `Vec3b`, `Matx33f`), the `InputArray` family and the
  traits. Names are de-duplicated across modules, so each entity has exactly one home module.
- **Hand-written replacements** — `src/*_ops.inc` and `src/core_fns.inc`; see §3.1.

`tools/prune_loop.py` closes the loop around the generator: on a failed build it parses the
errors that point into `gen_exports/*.inc`, appends the offending names together with their
reason to `tools/curated/<mod>.prune.txt`, regenerates and retries until the build is green.
431 lines have been pruned to date.

## 3. Principal design decisions

### 3.1 Replacement layer for internally-linked entities

OpenCV defines its operators and a number of named free functions as `static inline` in headers.
Such entities have internal linkage and cannot be re-exported through `export using`. This
repository replaces that surface with equivalent `inline` (external linkage) definitions, in
three files:

| File | Coverage |
|---|---|
| `src/core_ops.inc` | `saturate_cast`, Point/Size/Rect/Range/Scalar, Mat/MatExpr operators |
| `src/matx_ops.inc` | Matx/Vec algebra |
| `src/core_fns.inc` | named free functions (`cv::format`, `cv::print`, … re-implemented to upstream semantics) |

This layer is the direct reason `import opencv.cv;` compiles under clang (macOS/Windows) and gcc
alike. In mixed translation units — those that both include headers and import modules — the
upstream exact-match `static inline` overloads remain more specialised, so semantics are
unchanged.

### 3.2 Deduction form of template parameters (a clang BMI constraint)

An empirically established constraint applies on clang 20 and 22: a function or operator template
whose parameters are not fully bound by its **first argument**, once serialised into a module BMI,
crashes the frontend of any importer that uses that name. clang 18 is unaffected, making this an
18→20 regression. The affected shapes include the four-`int` `Matx × Matx` multiplication,
`determinant` over `Matx<_Tp,m,m>`, the two-typename `+=`/`-=`, and the comma-initialiser `<<`.

`src/matx_ops.inc` therefore uses **whole-type deduction**: the templates are declared as
`template<typename _MA, typename _MB>` and re-bound by self-checking constraints such as
`__is_same(_MA, typename _MA::mat_type)`, so that every template parameter is determined by the
argument types themselves. The form is semantically equivalent under gcc and has been
regression-verified. A minimal reproducer and the analysis are recorded in mcpp#256.

### 3.3 Treatment of macros

A module cannot export a macro. Object-like constant macros (`CV_8U`, `CV_PI`, `CV_MAKETYPE`, …)
are exported as `cv::` constexpr values or functions retaining the original spelling.
Function-like macros (`CV_Assert`, `CV_Error`, the version macros) cannot be handled this way;
a translation unit that needs the original spelling should `#include <opencv-m/macros.hpp>`
before the import.

### 3.4 Platform-conditional compilation and the Windows stub layer

Since mcpp 0.0.102 the per-glob flag tables can be made platform-conditional
(`[target.'cfg(...)'.build.flags]`, mcpp#258), and the descriptor is partitioned accordingly:
the entries byte-identical between the Linux and macOS snapshots live in `cfg(unix)`, the
x86_64 dispatch TUs and nasm `.asm` in `cfg(linux)`, the neon TUs in `cfg(macos)`, and the
Windows tables in `cfg(windows)`. An entry whose predicate does not match the resolved target
never enters the glob table, so no platform's build sees another platform's globs — and the
structural dead-glob warnings (about 26 per build before the migration) are gone on every
platform.

The Windows/unix difference comprises both **additions and removals** — the zlib group, for
instance, defines `HAVE_UNISTD_H=1` on unix and must leave it undefined on Windows. Conditional
entries are appended after the base and take effect under GNU last-wins, so removals are now
expressible; the Windows TUs are still carried by the **stub-namespace** arrangement — every
Windows translation unit is a one-line `#include` stub under `gen/windows/tu/w*/` (703 stubs in
total), giving each TU a stable path identity of its own. Removing the stub layer altogether
(compiling `third_party/**` directly on Windows and undoing the unix defines per cfg) is no
longer blocked by the manifest grammar and remains a separate, later clean-up.

### 3.5 Feature partitioning

| Feature | Contents | Rationale |
|---|---|---|
| `dnn` | adds the `import opencv.dnn;` interface and the underlying dnn sources (including vendored protobuf) | over 300 additional translation units, built only for consumers that opt in; the `opencv.cv` umbrella does not include dnn by default |
| `unifont` | embeds the WenQuanYi Micro Hei font, enabling Unicode/CJK `putText` through `FontFace("uni")` | the font payload and the rendering path are only meaningful to consumers that need them |

The gemm backend of `dnn` is selected per platform: Linux and macOS use mlas (x86 AVX/AVX2/AVX512,
arm NEON), while Windows uses OpenCV's built-in `fast_gemm`. Upstream mlas x86 assembly is written
in GAS/ELF syntax and clang-cl cannot emit COFF from it, so the upstream path of "fall back to
fast_gemm when no assembly is available" is followed. This platform distinction relies on the
per-OS feature semantics introduced by mcpp#253, hence the requirement of mcpp ≥ 0.0.101.

### 3.6 Dependency boundary

There is exactly one external dependency: `compat.ffmpeg`, which provides the implementation
behind the videoio FFmpeg backend (`cap_ffmpeg`). Everything else is contained in this
repository. On the package-index side only a thin `opencv.lua` pointing at this repository's
releases remains; upstream opencv.org distributions are no longer referenced directly.

## 4. Verification strategy

CI is a platform × toolchain matrix; all five legs are required:

| Leg | Platform and toolchain |
|---|---|
| linux-gcc | Ubuntu / gcc 16.1.0 (additionally builds the `dnn,unifont` features and lints template placeholders) |
| linux-llvm-22 | Ubuntu / llvm 22.1.8 |
| linux-musl-static | Ubuntu / gcc 16.1.0 `--target x86_64-linux-musl`, statically linked |
| macos-llvm | macOS 15 / llvm (arm64) |
| windows-llvm | Windows / llvm |

Each leg cold-builds roughly 2700 object files; the `~/.mcpp/registry` cache carries toolchains
and the built `compat.ffmpeg` across runs. The test suite comprises six executables covering
export-surface completeness (`api_surface_test`), read/write round-trips including FFmpeg mp4
(`roundtrip_test`), mixed module/header translation units (`full_mix_test`, `macros_mix_test`),
and the two feature interfaces (`dnn_module_test`, `unifont_module_test`).

The example projects are not executed in CI: each is a path-dependency consumer that cold-builds
the entire library in its own `target/` (about 40 minutes on a four-core runner), and its
coverage is already contained in `mcpp test`.

## 5. Upstream upgrade procedure

1. Import the new tarball with `tools/vendor/import_opencv.sh` (the script downloads and verifies
   it itself and does not depend on the host environment).
2. Regenerate the `gen/` snapshots and re-check the cross-platform differences and conflicts.
3. Regenerate the export surface with `python3 tools/gen_exports.py`, then converge to a green
   build with `python3 tools/prune_loop.py`.
4. Publish a release once all five CI legs are green, and update the version pointer in the thin
   `opencv.lua` on the index side.

## 6. Known temporary structures

| Item | Removal condition |
|---|---|
| the 703 stubs under `gen/windows/tu/w*/` and the corresponding `cfg(windows)` glob entries | no longer grammar-blocked since mcpp#258; removing the stub layer (compiling `third_party/**` directly on Windows, undoing the unix defines per cfg) is a separate, later clean-up — see [#17](https://github.com/Sunrisepeak/opencv-m/issues/17) |
| the llvm CI leg installing gcc first | mcpp#259 (`toolchain install llvm` does not pull its glibc runtime) is fixed |
| the whole-type deduction form in `src/matx_ops.inc` | may be reconsidered once the clang BMI regression is fixed, though the present form is harmless in itself |
