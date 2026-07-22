# opencv

> OpenCV 5 的 C++23 模块化封装。以 `import opencv.cv;` 取代头文件包含,API 拼写保持不变;
> 三平台全功能构建,dnn 与 unifont 以特性形式按需启用。

**中文** · [English](README.en.md)

[![Release](https://img.shields.io/github/v/release/Sunrisepeak/opencv-m)](https://github.com/Sunrisepeak/opencv-m/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Module](https://img.shields.io/badge/module-ok-green.svg)](https://en.cppreference.com/w/cpp/language/modules)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

| [mcpp 构建工具](https://github.com/mcpp-community/mcpp) · [包索引 mcpp-index](https://github.com/mcpp-community/mcpp-index) · [OpenCV 上游](https://github.com/opencv/opencv) · [架构设计](docs/architecture.md) · [Issues](https://github.com/Sunrisepeak/opencv-m/issues) · [Releases](https://github.com/Sunrisepeak/opencv-m/releases) |
|:---:|
| [![CI](https://github.com/Sunrisepeak/opencv-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Sunrisepeak/opencv-m/actions/workflows/ci.yml) |

## 特性

- **模块化导入**。消费者代码通过 `import opencv.cv;` 获得 OpenCV 接口,不需要头文件包含,
  而所用 API 与调用习惯与上游一致。
- **源码构建,消费端不执行 CMake**。OpenCV 5.0.0 源码内置于 `third_party/`,构建期配置以
  已生成的真实文件形式提交在 `gen/`,mcpp 依据本仓库 `mcpp.toml` 直接编译整套 OpenCV
  (含 NASM SIMD,运行时 dispatch 保留);videoio 的 FFmpeg 后端由 `compat.ffmpeg` 提供。
  模块层与完整构建由单一仓库承载。
- **三平台等价功能**。Linux、macOS、Windows 三平台均通过 CI 验证,各自具备 imgcodecs
  (PNG 与 JPEG)、videoio(FFmpeg `cap_ffmpeg`)与可选的 dnn;dnn 按平台选择 gemm 后端
  (x86 mlas/AVX、arm mlas/NEON、Windows 内置 fast_gemm)。
- **特性可选**。`dnn` 启用 `import opencv.dnn;` 深度学习模块,`unifont` 启用 Unicode 与
  中日韩字符的 `putText` 渲染。

## 快速开始

```bash
mcpp new myvision --template opencv && cd myvision && mcpp run -- input.png    # 灰度管线骨架
```

或在既有项目中声明依赖:

```toml
[dependencies]
opencv = "0.0.10"
```

```cpp
import opencv.cv;   // 或按模块:import opencv.core; import opencv.imgproc; …

int main() {
    cv::Mat img = cv::imread("in.png", cv::IMREAD_COLOR);
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::imwrite("out.png", gray);
    return 0;
}
```

## 模块

| 模块 | 说明 |
|---|---|
| `opencv.cv` | 聚合入口,推荐默认使用 |
| `opencv.core` | 核心:`Mat`、类型、算术与运算符替换面 |
| `opencv.imgproc` | 图像处理:滤波、几何变换、颜色空间、绘制 |
| `opencv.imgcodecs` | 图像读写(PNG 与 JPEG) |
| `opencv.videoio` | 视频 I/O(V4L2 与 FFmpeg 后端,三平台) |
| `opencv.highgui` | 高层 GUI(headless) |
| `opencv.flann` / `opencv.geometry` | 依赖闭包模块 |
| `opencv.dnn` | 深度学习(需启用 `dnn` 特性) |

每个 `opencv.<mod>` 是自包含的全局模块片段,仅导出自身表面,模块之间不互相导入;
`opencv.cv` 通过 `export import` 聚合。对象式常量宏(`CV_8UC3`、`CV_PI`、`CV_MAKETYPE` 等)
以保持原拼写的 `cv::` constexpr 形式导出;需要函数式宏(`CV_Assert`、版本宏)原始拼写的
翻译单元,应在 import 之前包含 `<opencv-m/macros.hpp>`。

## 特性开关

| 特性 | 说明 |
|---|---|
| `dnn` | 追加 `import opencv.dnn;` 接口(`Net`、`blobFromImage`、`readNet` 等)并构建底层 dnn 与内置 protobuf。三平台可用(per-OS 特性,mcpp#253):Linux 与 macOS 使用 mlas(x86 AVX/AVX2/AVX512,arm NEON);Windows 使用 OpenCV 内置 `fast_gemm` —— 上游 mlas 的 x86 汇编为 GAS/ELF 语法,clang-cl 无法产出 COFF,故沿用上游"无汇编即回退 fast_gemm"的路径 |
| `unifont` | 内嵌 WenQuanYi Micro Hei 字体,经 `FontFace("uni")` 支持 Unicode 与中日韩字符的 `putText` |

```toml
[dependencies]
opencv = { version = "0.0.10", features = ["dnn"] }
```

dnn 底层多出 300 余个翻译单元,仅对显式启用者构建,且默认不进入 `opencv.cv` 聚合模块。
要求 mcpp ≥ 0.0.102:per-OS 特性语义(mcpp#253)与按 OS 条件化的逐 glob 编译选项
(`[target.'cfg(...)'.build.flags]`,mcpp#258)。

## 示例

| 示例 | 内容 |
|---|---|
| [`examples/probe`](examples/probe) | 版本与构建信息(无需输入) |
| [`examples/gray_pipeline`](examples/gray_pipeline) | 读图、灰度化、写图的最小管线 |
| [`examples/workspace`](examples/workspace) | 多成员 workspace 示例 |

```bash
cd examples/gray_pipeline && mcpp run -- input.png
```

## 构建与工具链

本包不固定工具链,由 mcpp 解析环境默认值。CI 覆盖五种组合:Linux/gcc 16、Linux/llvm 22、
Linux/gcc `--target x86_64-linux-musl`(静态链接)、macOS/llvm(arm64)、Windows/llvm。

上游 OpenCV 5.0.0 源码内置于 `third_party/opencv-5.0.0/`,来自官方 tag tarball 的裁剪导入,
sha256 锁定且不含补丁,`tools/vendor/import_opencv.sh` 可复现该过程。平台相关的构建期配置
快照位于 `gen/`,由 `tools/vendor/port_descriptor.py` 一次性移植自已退役的 compat.opencv
描述符。包索引侧仅保留指向本仓库 Release 的薄 `opencv.lua`。

运算符与部分命名自由函数在上游以 `static inline` 定义,属内部链接,无法经 `export using`
重新导出(clang 直接拒绝,gcc 宽容)。本仓库以外部链接的等价定义替换该表面,分布于
`src/core_ops.inc`(saturate_cast,Point/Size/Rect/Range/Scalar,Mat/MatExpr)、
`src/matx_ops.inc`(Matx/Vec 代数)与 `src/core_fns.inc`(命名自由函数)。这是
`import opencv.cv;` 能在 clang 与 gcc 上同时编译的原因。

整体结构、各层职责与设计依据详见[架构设计说明](docs/architecture.md)。

> [!NOTE]
> 本项目处于早期阶段,接口可能调整。问题与建议欢迎提交
> [issue](https://github.com/Sunrisepeak/opencv-m/issues)。

## 许可

封装代码采用 MIT 许可。内置的 OpenCV 本体(`third_party/opencv-5.0.0/`)采用 Apache-2.0 许可,
其中各第三方组件保留各自原始许可。
