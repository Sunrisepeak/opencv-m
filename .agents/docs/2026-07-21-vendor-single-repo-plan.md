# Vendor OpenCV Single-Repo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fold the entire OpenCV 5.0.0 source build (today: `pkgs/c/compat.opencv.lua`, 421KB descriptor in mcpp-index) into this repo — vendored source + committed config snapshots + mcpp.toml — so the index keeps only a thin `pkgs/o/opencv.lua` pointing at opencv-m releases, and verify the vendored build locally on Linux with **gcc 16.1.0**, **llvm 22.1.8**, and **gcc-musl (`--target x86_64-linux-musl`)**.

**Architecture:** `third_party/opencv-5.0.0/` (pruned upstream source, committed) + `gen/{common,linux,macosx,windows}/` (the descriptor's 249 per-OS `generated_files` become real reviewable files; byte-identical-across-OS files dedupe into `common/`) + `mcpp.toml` carries everything the descriptor grammar carried (per-glob flags via `[[build.flags]]`, per-OS sources/cflags via `[target.'cfg(<os>)'.build]`, features with per-glob flags) + a small root `build.mcpp` covering only the manifest-grammar gaps (per-OS include dir selection, unifont hex-embed, later per-OS×feature source selection). Build-time synthesis (fonts, OpenCL kernels, jpeg12/16 stubs) is retired: outputs are produced ONCE by running the descriptor's own embedded build.mcpp in a harness and committing the results byte-faithfully.

**Tech Stack:** mcpp 0.0.101 (toolchains: gcc 16.1.0 default, llvm 22.1.8/20.1.7, gcc 15.1.0/16.1.0-musl for x86_64-linux-musl — all pre-installed, see `mcpp toolchain list`), OpenCV 5.0.0 pinned tarball (sha256 `b0528f5a1d379d59d4701cb28c36e22214cc51cf64594e5b56f2d3e6c0233095`, extracted at `~/.cache/opencv-m/opencv-5.0.0`), python3 + `/usr/bin/lua` for the one-time descriptor port.

## Global Constraints

- mcpp floor: **0.0.101** (`.xlings.json` already pins `xim:mcpp 0.0.101`).
- OpenCV source: **5.0.0 exactly**, tarball sha256 `b0528f5a1d379d59d4701cb28c36e22214cc51cf64594e5b56f2d3e6c0233095`; **zero source patches** (same rule the compat descriptor obeyed).
- Port must be **faithful**: every flag/define/source the linux block of `compat.opencv.lua` (mcpp-index main, post-#253 revision) expresses must land in mcpp.toml/build.mcpp with identical values. No "improvements" during the port.
- The module layer (`src/*.cppm`, `src/*.inc`, `src/gen_exports/`) is **not touched semantically** — per-glob flag scoping must keep `__OPENCV_BUILD`-family defines away from `src/**` TUs, exactly as the package split did.
- `[dependencies]` keeps `compat.ffmpeg = "8.1.2"` (ffmpeg stays a compat package; folding it is a separate future decision). `compat.opencv` and `compat.gtest` handling per Task 4.
- Branch: `feat/vendor-single-repo` (created off `chore/mcpp-0.0.101`, which is main+1).
- Manifest-grammar gaps confirmed against mcpp source (`src/manifest/types.cppm`, `src/manifest/toml.cppm`, v0.0.101) — do NOT work around them by inventing keys; unknown keys are hard parse errors:
  - `[target.'cfg(os)'.build]` supports ONLY `cflags/cxxflags/ldflags/sources` (+ `.dependencies`). No `include_dirs`, no `defines` (use `-D` inside cxxflags/cflags), no per-glob `flags`, no `features`.
  - No per-OS features in the manifest (#253 is descriptor-only) → root `build.mcpp` owns per-OS×feature logic.
  - No `mcpp build --toolchain` CLI flag → llvm verify uses `mcpp toolchain default`.
- OS-selection idiom in this plan: `[target.'cfg(linux)'.build]` (linux = glibc leg), `[target.'cfg(env = "musl")'.build]` for musl deltas if needed.

## Verification Matrix (the session goal)

| Leg | Command | Success criterion |
|---|---|---|
| gcc | `mcpp build && mcpp test` | all 6 gtest suites pass (roundtrip incl. FFmpeg videoio) |
| gcc + features | `mcpp test --features dnn,unifont` | DnnModule + UnifontModule pass |
| llvm | `mcpp toolchain default llvm@22.1.8; mcpp build --no-cache && mcpp test` | same suites pass; restore `gcc@16.1.0` default after |
| musl | `mcpp build --target x86_64-linux-musl --static && mcpp test --target x86_64-linux-musl` | build links statically; tests pass (deps incl. compat.ffmpeg cross-built for musl) |

---

### Task 1: Vendor import script + vendored tree

**Files:**
- Create: `tools/vendor/import_opencv.sh`
- Create: `third_party/opencv-5.0.0/` (result of running it)

**Interfaces:**
- Produces: `third_party/opencv-5.0.0/{modules,3rdparty,include,LICENSE,COPYRIGHT}` — the path every glob in Task 3's mcpp.toml starts with.

**Keep list** (everything else pruned): `LICENSE COPYRIGHT README.md include/` + `modules/{core,imgproc,imgcodecs,highgui,videoio,dnn,flann,geometry}` (each minus `test/ perf/ misc/java misc/objc misc/python`; dnn keeps `misc/{caffe,onnx,tensorflow,tflite}` — the generated .pb.cc/.pb.h live there and are in the sources list) + `modules/python/src2/` (hdr_parser.py, used by tools/gen_exports.py) + `3rdparty/{libjpeg-turbo,libpng,zlib,protobuf,mlas,flatbuffers,dlpack}` + `3rdparty/readme.txt`. Dropped: `samples doc docs_sphinx apps platforms cmake hal` (hal/ is not in any include_dir or source glob of the descriptor), other modules, other 3rdparty. Expected size ≈ 45MB.

- [ ] **Step 1: Write `tools/vendor/import_opencv.sh`**

```bash
#!/usr/bin/env bash
# One-time (re-)import of the pinned OpenCV source into third_party/.
# Usage: tools/vendor/import_opencv.sh [path-to-extracted-opencv-5.0.0]
set -euo pipefail
SRC="${1:-$HOME/.cache/opencv-m/opencv-5.0.0}"
PIN_SHA="b0528f5a1d379d59d4701cb28c36e22214cc51cf64594e5b56f2d3e6c0233095"
DST="$(git rev-parse --show-toplevel)/third_party/opencv-5.0.0"
[ -f "$SRC/CMakeLists.txt" ] || { echo "not an opencv tree: $SRC" >&2; exit 1; }
# Provenance check when the tarball sits next to the tree (best-effort).
TAR="$(dirname "$SRC")/opencv-5.0.0.tar.gz"
if [ -f "$TAR" ]; then
    echo "$PIN_SHA  $TAR" | sha256sum -c - || exit 1
fi
rm -rf "$DST"; mkdir -p "$DST"
KEEP_MODULES="core imgproc imgcodecs highgui videoio dnn flann geometry"
KEEP_3RDPARTY="libjpeg-turbo libpng zlib protobuf mlas flatbuffers dlpack"
cp "$SRC"/{LICENSE,COPYRIGHT,README.md} "$DST/"
cp -r "$SRC/include" "$DST/include"
mkdir -p "$DST/modules" "$DST/3rdparty" "$DST/modules/python"
for m in $KEEP_MODULES; do cp -r "$SRC/modules/$m" "$DST/modules/$m"; done
cp -r "$SRC/modules/python/src2" "$DST/modules/python/src2"
for t in $KEEP_3RDPARTY; do cp -r "$SRC/3rdparty/$t" "$DST/3rdparty/$t"; done
cp "$SRC/3rdparty/readme.txt" "$DST/3rdparty/"
# Slim the kept modules: test/perf trees and non-C++ binding misc dirs.
for m in $KEEP_MODULES; do
    rm -rf "$DST/modules/$m"/{test,perf}
    for b in java objc python js; do rm -rf "$DST/modules/$m/misc/$b"; done
done
du -sh "$DST"
```

- [ ] **Step 2: Run it and eyeball the tree**

Run: `chmod +x tools/vendor/import_opencv.sh && tools/vendor/import_opencv.sh`
Expected: prints a size around 40–50M; `third_party/opencv-5.0.0/modules/dnn/misc/onnx/opencv-onnx.pb.cc` exists; `third_party/opencv-5.0.0/modules/imgproc/fonts/` contains the builtin `*.ttf.gz` fonts.

- [ ] **Step 3: Commit (vendor commit kept separate from logic commits)**

```bash
git add third_party tools/vendor/import_opencv.sh
git commit -m "feat: vendor pruned OpenCV 5.0.0 source under third_party/ (tarball sha b0528f5a, zero patches)"
```

---

### Task 2: One-time descriptor port — gen/ trees + manifest fragments

**Files:**
- Create: `tools/vendor/port_descriptor.py` (parses compat.opencv.lua via real lua → JSON, emits everything)
- Create: `tools/vendor/dump_descriptor.lua` (10-line lua→JSON shim)
- Create: `gen/common/**`, `gen/linux/**` (+ `gen/macosx/**`, `gen/windows/**` staged for later, same run)
- Create: `assets/WenQuanYiMicroHei.ttf.gz` (unifont blob, sha256 `70c5634fe8326a20a18f4d634d08c510f2c05f6613d2d5aa3566f162cc02804f`)
- Scratch (not committed): descriptor copy + extracted embedded build.mcpp + its outputs

**Interfaces:**
- Consumes: `third_party/opencv-5.0.0/` from Task 1.
- Produces: `gen/<os>/…` file trees whose relative paths (under a `gen/<os>/` prefix replacing the descriptor's `mcpp_generated/`) Task 3's sources/include logic references; `manifest-fragments/{linux.toml,features.toml}` with ready-to-paste `[target.'cfg(linux)'.build]`, `[[build.flags]]`, `[features]` bodies (all `*/` source globs rewritten to `third_party/opencv-5.0.0/`, all `mcpp_generated/` paths rewritten to `gen/linux/` or `gen/common/`).

Port mechanics (implement exactly this; the descriptor is machine-generated and regular):

1. Fetch the CURRENT `pkgs/c/compat.opencv.lua` from mcpp-index main into scratch; record its git commit hash in the commit message (this is the last-ever sync point).
2. `dump_descriptor.lua`: `package = dofile(arg[1])` is not how descriptors load — they ARE a `package = {...}` assignment; so shim = `dofile` the file after prepending `local function noop() end` if needed, then walk the `package` global and print JSON (strings/tables/numbers only). Long strings (`[[...]]`, `[==[...]==]`) come through as plain strings.
3. `port_descriptor.py`:
   - For each os in linux/macosx/windows: write every `generated_files["<path>"]` except `build.mcpp` and `tu_manifest.txt` to `gen-stage/<os>/<path minus mcpp_generated/>`.
   - Dedupe: files byte-identical across ALL OS blocks that contain them move to `gen/common/`, the rest to `gen/<os>/`.
   - jpeg12/16 stubs: parse each os's `tu_manifest.txt` (lines `<group>|<real-src>|<define>=<val>,...`, `?<feature>` prefix = feature-gated) and materialize each stub as a real file `gen/<os-or-common>/tu/<group>/<group-prefixed-name>.c` containing the `#define` lines followed by `#include "<repo-relative real source>"`. (Port the exact stub-emission logic from the descriptor's embedded build.mcpp — read it, don't guess the format.)
   - fonts + OpenCL kernels: extract the embedded `build.mcpp` C++ to scratch, `g++ -std=c++23 -O1` it, run it with `MCPP_OUT_DIR=<scratch-out>`, `MCPP_MANIFEST_DIR` pointing at a scratch dir laid out so its opencv-source discovery finds `third_party/opencv-5.0.0`, and NO unifont feature env. Harvest `builtin_font_{sans,italic}.h` + `opencl_kernels_*.{cpp,hpp}` into `gen/common/` (verify: these are OS-independent transforms; assert byte-equal if produced per-OS).
   - Emit TOML fragments with the path rewrites; per-glob flag tables keep entry ORDER (last-wins semantics).
4. unifont blob: `curl -L` the GLOBAL url from compat.opencv-unifont.lua, verify sha256 `70c5634f…`, save to `assets/`.

- [ ] **Step 1: Write dump_descriptor.lua + port_descriptor.py per the mechanics above** (complete code lives in the tools; keep them committed — they are the audit trail of the port)
- [ ] **Step 2: Run the port**

Run: `python3 tools/vendor/port_descriptor.py --descriptor <scratch>/compat.opencv.lua --opencv third_party/opencv-5.0.0 --out .`
Expected: `gen/linux` + `gen/common` populated (≈249 files total for linux incl. dedupe; opencl_kernels_*.cpp and builtin_font_{sans,italic}.h in `gen/common/`); fragments written; script prints a per-OS file count table.

- [ ] **Step 3: Spot-verify faithfulness**

Run: `diff <(descriptor-extracted cvconfig.h) gen/linux/cvconfig.h` and one dispatch file, plus `grep -c 'OcvBuiltinFontSans' gen/common/builtin_font_sans.h`
Expected: byte-identical; symbol present.

- [ ] **Step 4: Commit**

```bash
git add tools/vendor gen assets
git commit -m "feat: port compat.opencv linux/mac/win frozen snapshots to gen/ as real files (descriptor @<index-commit>); commit font/CL-kernel/jpeg-stub synthesis outputs; vendor unifont blob"
```

---

### Task 3: mcpp.toml + root build.mcpp

**Files:**
- Modify: `mcpp.toml` (whole-file rewrite)
- Create: `build.mcpp`

**Interfaces:**
- Consumes: Task 2's fragments verbatim.
- Produces: the buildable package; `[features] dnn`/`unifont` keep their EXTERNAL semantics (`opencv = { features = ["dnn"] }` consumers unchanged).

mcpp.toml skeleton (fragments paste-in points marked; values come from Task 2 output, shown here abbreviated but the structure is exact):

```toml
[package]
name        = "opencv"
version     = "0.0.7"
description = "OpenCV 5.0.0 vendored + C++23 module layer — import opencv.cv; single-repo, source-built by mcpp"
license     = "MIT"                      # module layer; OpenCV itself Apache-2.0 (third_party/opencv-5.0.0/LICENSE)

[build]
sources = [
    "src/opencv.cppm", "src/cv.cppm", "src/core.cppm", "src/imgproc.cppm",
    "src/imgcodecs.cppm", "src/videoio.cppm", "src/highgui.cppm",
    "src/flann.cppm", "src/geometry.cppm",
]
include_dirs = [
    "include",
    "gen/common",
    # descriptor include_dirs with '*/' → third_party/opencv-5.0.0/ (OS-neutral subset;
    # the per-OS gen/<os> dir is emitted by build.mcpp)
    "third_party/opencv-5.0.0",
    "third_party/opencv-5.0.0/modules/core/include",
    # … full 30-odd list from fragment …
]

[[build.flags]]                          # ← paste fragment: base per-glob table, order preserved
glob = "third_party/opencv-5.0.0/modules/core/**"
defines = ["__OPENCV_BUILD=1", "..."]
# … 25 entries: module groups, 3rdparty groups, ISA suffixes, per-file extras, **/*.asm …

[target.'cfg(linux)'.build]              # ← paste fragment
cflags   = ["-msse3", "-w"]
cxxflags = ["-msse3", "-w"]
ldflags  = ["-lpthread", "-ldl"]
sources  = [ "third_party/opencv-5.0.0/modules/core/src/*.cpp", "...42 globs...",
             "gen/linux/modules/core/*.sse2.cpp", "...generated dispatch TUs...",
             "gen/common/tu/jpeg12/*.c", "gen/common/tu/jpeg16/*.c" ]

[dependencies]
compat.ffmpeg = "8.1.2"

[dev-dependencies]
compat.gtest = "1.15.2"

[features]
dnn = { sources = ["src/dnn.cppm", "third_party/opencv-5.0.0/modules/dnn/src/*.cpp", "...", "gen/linux/modules/dnn/**/*.cpp"],
        defines = ["HAVE_OPENCV_DNN"],
        flags   = [ { glob = "third_party/opencv-5.0.0/modules/dnn/**", defines = ["..."] }, ... ] }
unifont = { defines = ["HAVE_UNIFONT"] }   # blob embed handled by build.mcpp

[target.x86_64-linux-musl]
linkage = "static"

[targets.opencv]
kind = "lib"
```

Known accepted limitation to note in comments: `[features].dnn.sources` includes `gen/linux/...` dispatch TUs — per-OS×feature intersection is a manifest gap (#253 descriptor-only); when the macOS/windows legs are ported, those move out of `[features]` into build.mcpp `mcpp:source=` selection keyed on `MCPP_TARGET_OS` × `MCPP_FEATURE_DNN`. Fine for the linux-first milestone (zero-match globs on other OSes only warn — but dnn on mac would silently miss its dispatch TUs, hence the build.mcpp move later).

build.mcpp (root, complete responsibilities — keep it under ~120 lines):

```cpp
// build.mcpp — covers exactly the three manifest-grammar gaps:
//  1. per-OS generated-config include dir:  mcpp:include-dir=gen/<MCPP_TARGET_OS>
//     (linux|macosx|windows; private -I, consumers only ever need gen/common)
//  2. unifont: when MCPP_FEATURE_UNIFONT is set, hex-embed
//     assets/WenQuanYiMicroHei.ttf.gz -> $MCPP_OUT_DIR/builtin_font_uni.h
//     (symbol OcvBuiltinFontUni, same blob2hdr transform as the descriptor's
//     build.mcpp — port that function verbatim) + mcpp:include-dir=<out>
//     + mcpp:rerun-if-changed=assets/WenQuanYiMicroHei.ttf.gz
//  3. (reserved for mac/win port) per-OS×feature source selection via mcpp:source=
// Raw mcpp: stdout protocol; diagnostics to stdout non-directive lines, never stderr.
```

- [ ] **Step 1: Write mcpp.toml (paste fragments), write build.mcpp**
- [ ] **Step 2: Manifest sanity check**

Run: `mcpp build --strict --print-fingerprint`
Expected: parses, no unknown-key errors, fingerprint prints. Zero-match glob warnings acceptable ONLY for `gen/{macosx,windows}` paths.

- [ ] **Step 3: Commit**

```bash
git add mcpp.toml build.mcpp
git commit -m "feat: single-repo build — mcpp.toml carries the full OpenCV source build; build.mcpp covers per-OS include + unifont embed"
```

---

### Task 4: gcc leg green (build + test + features)

- [ ] **Step 1:** `mcpp build 2>&1 | tail -30` — iterate on port bugs (path rewrites, flag scoping, include order). Diagnose against the OLD split build's compile lines when in doubt (`ninja -t commands` technique in target/ of a compat.opencv consumer).
- [ ] **Step 2:** `mcpp test` — all 6 suites pass, incl. roundtrip (PNG/JPEG + FFmpeg mp4 videoio).
- [ ] **Step 3:** `mcpp test --features dnn,unifont` — DnnModule.BlobFromImageAndNet + UnifontModule.CjkPutTextInks pass (validates local feature defs replacing #243 forwarding + build.mcpp unifont embed).
- [ ] **Step 4:** Remove the now-dead `compat.opencv` dependency remnants: `[features]` no longer `forward=`, README/templates/examples references updated in a docs pass (own commit).
- [ ] **Step 5:** Commit `test: gcc leg green (default + dnn,unifont)`.

Note: `compat.gtest` stays a dep — dev-dependency only, out of scope for unification.

---

### Task 5: llvm leg

- [ ] **Step 1:** `mcpp toolchain default llvm@22.1.8`
- [ ] **Step 2:** `mcpp build --no-cache && mcpp test` (and `--features dnn,unifont` if time-cheap; store artifacts make it incremental)
- [ ] **Step 3:** On failure: triage clang-vs-gcc deltas. Expected-benign: warnings (`-w` already on); likely suspects: gcc-only cxxflags in ported fragments (there should be none — descriptor macosx block already builds under clang), module-layer gcc16-specific workarounds (R2 subsumption ops — clang precedent exists via macOS/windows CI so linux-clang should hold; if not, capture a minimal repro, fix in `src/*.inc` guarded by `#ifdef`, NOT in third_party).
- [ ] **Step 4:** Fallback if llvm@22.1.8 hits a toolchain/module bug: retry `llvm@20.1.7` (the macOS-proven version); if 20 passes and 22 fails, file the mcpp/llvm issue and count the leg green on 20 with the issue linked.
- [ ] **Step 5:** `mcpp toolchain default gcc@16.1.0` (ALWAYS restore), commit any fixes: `fix: llvm leg`.

---

### Task 6: musl leg

- [ ] **Step 1:** `mcpp build --target x86_64-linux-musl --static` — this cross-builds deps too (compat.ffmpeg 2282 TU + gtest for musl; budget ~10 min cold).
- [ ] **Step 2:** `mcpp test --target x86_64-linux-musl` (confirm `mcpp test --help` accepts `--target`; if not, run the built test binaries from `target/` directly — they are native x86_64).
- [ ] **Step 3:** Triage buckets, in likelihood order:
  - glibc-isms in the FROZEN snapshots (`gen/linux/cvconfig.h` probed on glibc): e.g. `HAVE_BACKTRACE`/execinfo, `HAVE_MALLOC_H` semantics. Fix via `[target.'cfg(env = "musl")'.build]` cflags (`-DCV_DISABLE_X=1`-style or `-U`/`-D` overrides) — never edit third_party. If a cvconfig value itself must flip, add `gen/linux-musl/cvconfig.h` (copy + minimal delta) and have build.mcpp prefer it when `MCPP_TARGET_ENV=musl`; document each delta.
  - compat.ffmpeg snapshot glibc-isms: WE own that descriptor (mcpp-index) — descriptor grammar has per-OS but NOT per-env blocks, so a musl-breaking ffmpeg config value means filing the mcpp issue (`mcpp.<os>` → env dimension) with the concrete repro, and for THIS goal documenting it as the musl-videoio boundary… only if it actually breaks. Try first.
  - static-link specifics: `-lpthread -ldl` are no-ops/absorbed under musl-static; dlopen-based runtime plugin paths in videoio must not be reached by tests (they aren't — FFmpeg backend is compiled-in).
- [ ] **Step 4:** Commit `feat: musl target leg (x86_64-linux-musl, static)` with a short musl-deltas section appended to this doc.

---

### Task 7: Wrap-up (this session)

- [ ] README: single-repo architecture note (index = thin pointer; compat.opencv retired upon the future index PR), verification matrix results table (gcc/llvm/musl actual outcomes).
- [ ] Update `.agents/docs/` pointer docs if any reference compat.opencv as the build carrier.
- [ ] Final commit + summary to user (PR/merge decision is the user's).

## Explicitly OUT of this session's scope (recorded for the follow-on)

1. macOS/windows leg port: gen/{macosx,windows} trees are already committed by Task 2; remaining = `[target.'cfg(macos|windows)'.build]` fragments, build.mcpp `mcpp:source=` per-OS×feature selection, CI matrix. Windows dnn per-OS flag delta (mcpp#253 shape) needs either the manifest gap filed (`[target.cfg].flags` / per-OS features) or full build.mcpp mediation.
2. Index PR: `pkgs/o/opencv.lua` → opencv-m release tarball (which now CONTAINS third_party; github auto-archive keeps working since everything is committed), delete `compat.opencv.lua` + `compat.opencv-unifont.lua`, retire `tools/compat-opencv/`, adjust index members.
3. Release economics note (accepted trade-off, decided by user 2026-07-21): every opencv-m release now re-ships ~20MB compressed and invalidates consumers' cached ~1h source build; mitigated by the stabilized module surface, low release cadence, and a possible future prebuilt channel.
4. ffmpeg single-repo unification (same pattern) — separate decision.
