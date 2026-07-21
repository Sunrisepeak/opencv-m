# opencv

> OpenCV 5 的 C++23 模块化封装 — `import opencv.cv;` 即用 · 三平台全功能 · dnn/unifont 特性可插拔

[![Release](https://img.shields.io/github/v/release/Sunrisepeak/opencv-m)](https://github.com/Sunrisepeak/opencv-m/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Module](https://img.shields.io/badge/module-ok-green.svg)](https://en.cppreference.com/w/cpp/language/modules)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

| [mcpp 构建工具](https://github.com/mcpp-community/mcpp) · [包索引 mcpp-index](https://github.com/mcpp-community/mcpp-index) · [OpenCV 上游](https://github.com/opencv/opencv) · [Issues](https://github.com/Sunrisepeak/opencv-m/issues) · [Releases](https://github.com/Sunrisepeak/opencv-m/releases) |
|:---:|
| [![CI](https://github.com/Sunrisepeak/opencv-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Sunrisepeak/opencv-m/actions/workflows/ci.yml) |

## 核心特性

- **纯模块导入** — `import opencv.cv;`,消费者代码零 `#include`,用的仍是你熟悉的 `cv::` API 与习惯
- **从源码构建,消费端不跑 CMake** — OpenCV 5.0.0 源码 vendored 在 `third_party/`,冻结配置快照是 `gen/` 下的真实文件,mcpp 按本仓库 `mcpp.toml` 直接编译整套 OpenCV(含 NASM SIMD,**运行时 dispatch 保留**),videoio 的 FFmpeg 后端由 `compat.ffmpeg` 闭合;单仓库承载模块层 + 完整构建
- **三平台全功能** — Linux / macOS / Windows 三平台 CI,每个平台都带 imgcodecs(PNG+JPEG)、videoio(FFmpeg `cap_ffmpeg`)与可选 dnn(`import opencv.cv/dnn` 三平台齐);dnn 各平台自适应 gemm 后端(x86 mlas/AVX、arm mlas/NEON、windows 内置 fast_gemm)
- **特性可插拔** — `features = ["dnn"]` 解锁 `import opencv.dnn;` 深度学习模块;`unifont` 解锁 Unicode/CJK `putText`

## 快速开始

```bash
mcpp new myvision --template opencv && cd myvision && mcpp run -- input.png    # 灰度管线骨架
```

或在已有项目中手动接入:

```toml
[dependencies]
opencv = "0.0.6"
```

```cpp
import opencv.cv;   // 或按模块:import opencv.core; import opencv.imgproc; …

int main() {
    cv::Mat img = cv::imread("in.png", cv::IMREAD_COLOR);
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::imwrite("out.png", gray);   // 还是你熟悉的 C++ API,只是不用 #include
    return 0;
}
```

## 模块一览

| 模块 | 说明 |
|---|---|
| `opencv.cv` | umbrella 聚合,推荐默认入口 |
| `opencv.core` | 核心:`Mat` / 类型 / 算术 / 运算符替换面 |
| `opencv.imgproc` | 图像处理:滤波 / 几何变换 / 颜色空间 / 绘制 |
| `opencv.imgcodecs` | 图像读写(PNG + JPEG) |
| `opencv.videoio` | 视频 I/O(V4L2 + **FFmpeg** 后端,三平台) |
| `opencv.highgui` | 高层 GUI(headless) |
| `opencv.flann` / `opencv.geometry` | 依赖闭包模块 |
| `opencv.dnn` | 深度学习(`dnn` 特性,见下) |

每个 `opencv.<mod>` 是自包含 GMF,只导出自身表面、互不 `import`;`import opencv.cv;` 是推荐入口。常量宏(`CV_8UC3` / `CV_PI` / `CV_MAKETYPE` …)以 `cv::` constexpr 导出;需要原始宏拼写(`CV_Assert`、版本宏)的 TU,在 import 前 `#include <opencv-m/macros.hpp>`。

## Features

| Feature | 说明 |
|---|---|
| `dnn` | 深度学习模块:加 `import opencv.dnn;` 接口(`Net` / `blobFromImage` / `readNet` …)并构建底层 dnn(+ vendored protobuf)。**三平台**(per-OS 特性,mcpp#253):Linux/macOS 走 mlas(x86 AVX/AVX2/AVX512、arm NEON);windows 走 OpenCV 内置 `fast_gemm`(AVX/AVX2 的 `.cpp` kernel)—— 上游 mlas 的 x86 汇编是 GAS/ELF,clang-cl 出不了 COFF,故照上游"无 ASM 就回退 fast_gemm"的路子跳过 mlas |
| `unifont` | Unicode/CJK `putText` 覆盖 —— 纯 forward,内嵌 WenQuanYi Micro Hei 字体,`FontFace("uni")` 渲染 |

```toml
[dependencies]
opencv = { version = "0.0.6", features = ["dnn"] }
```

按需构建(dnn 底层多 300+ TU,只给显式开启的消费者);`dnn` 默认不进 `opencv.cv` umbrella。需 mcpp ≥ 0.0.101(per-OS features)。

## 示例

| 示例 | 内容 |
|---|---|
| [`examples/probe`](examples/probe) | 版本 / 构建信息(无需输入) |
| [`examples/gray_pipeline`](examples/gray_pipeline) | 读图 → 灰度 → 写图的最小管线 |
| [`examples/workspace`](examples/workspace) | 多成员 workspace 示例 |

```bash
cd examples/gray_pipeline && mcpp run -- input.png
```

## 工具链与运行时

包不固定工具链(mcpp 解析环境默认;已本地验证 gcc 16 / llvm 22 / gcc-musl `--target x86_64-linux-musl`)。上游 OpenCV 5.0.0 源码 **vendored 于 `third_party/opencv-5.0.0/`**(官方 tag tarball 裁剪导入,sha256 锁定,零补丁;`tools/vendor/import_opencv.sh` 可复现),per-OS 冻结配置快照在 `gen/`(由 `tools/vendor/port_descriptor.py` 一次性从退役的 compat.opencv 描述符移植);包索引侧只剩指向本仓库 Release 的薄 `opencv.lua`。运算符与命名自由函数的 `static inline` 表面无法跨模块边界(clang 直接拒绝 `export` internal-linkage 的 using-declaration,gcc 只是宽容),已由 `src/core_ops.inc`(saturate_cast、Point/Size/Rect/Range/Scalar、Mat/MatExpr)+ `src/matx_ops.inc`(Matx/Vec 代数)+ `src/core_fns.inc`(static-inline 命名函数)替换 —— 这正是 `import opencv.cv;` 能在 clang(macOS/Windows)与 gcc 上都编译的原因。

> [!NOTE]
> 早期版本,接口可能调整。问题与想法欢迎提 [issue](https://github.com/Sunrisepeak/opencv-m/issues)。

## License

封装代码 MIT;vendored 的 OpenCV 本体(`third_party/opencv-5.0.0/`)**Apache-2.0**(内含各 3rdparty 组件原始许可)。
