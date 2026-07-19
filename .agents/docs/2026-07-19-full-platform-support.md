# opencv-m — full-platform (macOS + Windows) support

This repo's module layer (`import opencv.cv;` + per-module interfaces + the
`dnn`/`unifont` features) is platform-neutral C++23 module code. Full-platform
support is gated on **`compat.opencv` gaining macOS/Windows config snapshots**,
which is a mcpp-index concern.

**The authoritative plan lives in mcpp-index:**
`mcpp-index/.agents/docs/2026-07-19-full-platform-support-plan.md`.

opencv-m's part, as each compat platform lands:
- widen `mcpp.toml` `platforms` (`["linux"]` → `+macos` → `+windows`);
- widen the opencv/opencv-dnn/opencv-unifont/opencv-module* member `cfg` gates
  in mcpp-index from `cfg(linux)` to `+macos`/`+windows`;
- add `macos-15`/`windows-latest` jobs to this repo's `ci.yml`;
- verify the module `.cppm` layer compiles under mcpp's clang-MSVC on Windows
  (the one caveat — MSVC-proper has C++20-modules GMF bugs, but mcpp uses clang).
Sequencing: macOS-headless first (video needs `compat.ffmpeg`-macOS first).
