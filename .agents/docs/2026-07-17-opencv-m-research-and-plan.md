# opencv-m — OpenCV 5.0 模块化封装层:调研 + 规划文档

> **状态**: 待 review(核心决策点见 §5)
> **日期**: 2026-07-17
> **后续**: 核心点 review 通过后,再产出逐任务的实施计划(bite-sized tasks, TDD)

**Goal:** 以 OpenCV 5.0.0 源码为范本,构建一个 mcpp 工程 `opencv-m`:OpenCV 源码单独放一个目录、由 mcpp **直接源码构建**(不预构建、构建期不依赖 CMake),外加一个 C++ 模块封装层,让使用者写 `import opencv.cv;` / `import opencv.core;`(对标 Python 的 `import cv2`),完全不改变 OpenCV 的 API 用法和习惯——只是不用头文件了。

**Architecture:** 单一 mcpp 包。`third_party/opencv/`(原封不动的 5.0.0 源码)+ `gen/`(一次性快照的 CMake 生成文件,纯文本、可审计)+ `src/*.cppm`(GMF `#include` + `export using` 重导出的模块封装层)。导出面由 OpenCV 自带的 `hdr_parser.py`(Python 绑定同款解析器)辅助生成。

**Tech Stack:** mcpp ≥ 0.0.93 / gcc 16.1.0(mcpp 默认 Linux 工具链)/ C++23 modules / OpenCV 5.0.0(tag, 2026-06-06 发布)

## 全局约束(来自需求,逐条对应)

| 约束 | 落实方式 |
|---|---|
| OpenCV 5.0 源码单独放一个目录 | `third_party/opencv/`,与上游 tag 逐字节一致,不混入封装层代码 |
| 源码构建,不预构建 OpenCV | mcpp `[build].sources` glob 直接编译 OpenCV 的 .cpp/.c;构建期零 CMake(已 POC 验证) |
| 只有必要时才改 OpenCV 源码 | 目前 POC 达成 **0 补丁**;若未来必要,走 `patches/` 显式补丁 + 记录理由 |
| 不改变 OpenCV 用法和使用习惯 | 只做 `export using ::cv::X` 重导出,`cv::` 命名空间、函数签名、类型全部原样 |
| `import opencv.cv;`,参考 python import | 每个 OpenCV module 一个 C++ module(`opencv.core` 等)+ 汇总模块 `opencv.cv`;导出面以 Python 绑定可见 API 为基准起步 |
| 整个项目为 mcpp 工程 | 单包 `mcpp.toml`,遵循 mcpp-style-ref 编码风格与 imgui-m 仓库形态(Form-A `-m` 仓库) |

---

## 1. 可行性验证(本次已完成的 POC,最重要的结论先说)

在写本文档前,先做了端到端 spike(scratchpad `spike2/`),**全部跑通**:

```text
src/main.cpp:
    import std;
    import opencv.core;
    int main() {
        cv::Mat m(4, 4, cv::TYPE_8UC1, cv::Scalar(7));
        auto s = cv::mean(m);
        std::println("opencv {} mean={} total={}", cv::getVersionString(), s[0], m.total());
    }

$ mcpp run
opencv 5.0.0 mean=7 total=16
```

- **构建内容**: OpenCV core 全部 114 个 .cpp + 3rdparty/zlib 15 个 .c + `opencl_kernels_core.cpp` + 模块封装 `.cppm` + 消费者 `main.cpp`,共 205 个对象文件,mcpp 一次性编译链接,构建期无 CMake。
- **冷构建耗时**: 17.8s wall(32 核,4m37s CPU),增量构建秒级。
- **工具链**: mcpp 0.0.93 自动解析 `gcc@16.1.0`(即 mcpp 的 Linux 默认),`import std` 与 GMF 重头文件共存无问题。
- **0 处 OpenCV 源码修改**。

POC 期间踩到并解决的事实(直接影响设计):

1. **消费者侧唯一需要的生成头**是 `opencv2/opencv_modules.hpp`(`core/base.hpp:52` 无条件 include);内容只是若干 `#define HAVE_OPENCV_<M>`,手写/脚本生成均可。`cvconfig.h`、`cv_cpu_config.h` 均在 `#ifdef __OPENCV_BUILD` 之下,只有编译 OpenCV 自身才需要。
2. **编译 OpenCV 自身需要的生成文件全集**(core、关 dispatch 时):`cvconfig.h`、`cv_cpu_config.h`、`custom_hal.hpp`、`opencv_data_config.hpp`、`opencv_modules.hpp`、`version_string.inc`、18 个 `*.simd_declarations.hpp`、`opencl_kernels_core.{hpp,cpp}`。全部是确定性纯文本,可一次生成后 checked-in。
3. **OpenCL 内核文件**即使 `WITH_OPENCL=OFF` 也被源码无条件 include;但生成器 `cmake/cl2cpp.cmake` 可用 `cmake -P` 独立运行(只是把 `.cl` 转成字符串常量,内容自身有 `#ifdef HAVE_OPENCL` 保护),一次生成即可提交。
4. zlib 的 .c 需要 `-DZ_HAVE_UNISTD_H -DHAVE_UNISTD_H -D_POSIX_C_SOURCE=200809L`(C11 严格模式下 POSIX 声明)。
5. **mcpp 的 sources glob 不跟随目录软链接**——OpenCV 源码必须真实 vendor 进仓库(这本来就是需求)。
6. `include_dirs` 生效可靠;`[build].cxxflags` 里的相对 `-I` 对 `.cppm` 编译单元不生效(用 `include_dirs` 即可)。
7. OpenCV 源码 TU 需要 `-D__OPENCV_BUILD=1`;单包内该宏是包全局的,封装层 `.cppm` 同样带着编译,已验证无副作用(gen/ 里的 cvconfig.h 本来就提供)。

> POC 复现物(mcpp.toml、cppm、生成文件清单)见附录 A。

## 2. 调研:OpenCV 5.0.0 源码结构

### 2.1 模块清单与依赖(来自各 `modules/*/CMakeLists.txt` 的 `ocv_add_module`)

拓扑序:`core → flann → geometry → imgproc → {imgcodecs, photo, video, dnn, stereo, features} → {videoio, objdetect} → {highgui, calib, ptcloud, stitching}`。

| 模块 | 角色 | 直接依赖 | 备注(vs 4.x) |
|---|---|---|---|
| core | 核心数据结构/运算 | — | |
| flann | 近似最近邻 | core | |
| geometry | 2D/3D 几何原语 | flann | **新**(dev 期叫 `3d`,正式版更名 geometry) |
| imgproc | 图像处理 | core, geometry | |
| imgcodecs | 图像编解码 | imgproc | 17 种编解码器全部可选,可 0 codec 构建 |
| videoio | 视频 IO | imgproc, imgcodecs | |
| highgui | GUI 窗口 | imgproc (+imgcodecs, videoio 可选) | |
| photo | 计算摄影 | imgproc, geometry | |
| video | 视频分析/跟踪 | imgproc | |
| features | 特征点 | imgproc, geometry (+flann, dnn 可选) | **features2d 更名** |
| objdetect | 目标检测 | core, imgproc, features | Haar/HOG 移去 contrib |
| dnn | DNN 推理 | core, imgproc, geometry | 27 万 LOC,需 protobuf/flatbuffers |
| stereo | 双目深度 | imgproc, geometry | **calib3d 拆分** |
| calib | 相机标定 | imgproc, objdetect, flann, geometry, stereo | **calib3d 拆分** |
| ptcloud | 点云/网格 | geometry, imgproc, flann | **新** |
| stitching | 拼接 | imgproc, features, geometry, flann | |
| world / ts / python / java / js / objc | 聚合构建 / 测试 / 绑定 | — | 与封装层无关 |

- 4.x → 5.0 关键变化:`calib3d` 拆为 `geometry + calib + stereo`;`features2d → features`;`ml`、`gapi` 等移至 contrib;**传统 C API(core_c.h 等)已删除**——API 面是纯 C++ `namespace cv`,对模块化封装极其有利。
- 官方保留了兼容头 `opencv2/calib3d.hpp`、`opencv2/features2d.hpp`(仅 include 新头)。
- 上游 issue [#27308](https://github.com/opencv/opencv/issues/27308) 提出 5.x 提供 C++20 modules,**无人认领、无 PR**;官方 roadmap 说"后续 5.x 再说"。第三方封装无冲突,且可抢占生态位。

### 2.2 规模(封装工作量评估)

公共头文件共 **283 个**。Top:core 145 头/23 万 LOC、imgproc 5 头/13.4 万 LOC、dnn 12 头/27 万 LOC、flann 36 头(几乎纯模板 header-only)。**导出面大头在 core**;dnn 代码量大但 API 面小。

### 2.3 构建期生成文件的机制(全部可快照)

| 生成文件 | 生成器 | 谁需要 |
|---|---|---|
| `opencv2/opencv_modules.hpp` | `opencv_modules.hpp.in`(就是 HAVE_OPENCV_* 列表) | **消费者也需要**(唯一) |
| `cvconfig.h` / `cv_cpu_config.h` / `custom_hal.hpp` / `opencv_data_config.hpp` | cmake templates | 仅编译 OpenCV 自身(`__OPENCV_BUILD` 门控) |
| `version_string.inc` | `getBuildInformation()` 字符串 | 仅 core 的 system.cpp |
| `<file>.simd_declarations.hpp`(+ 开 dispatch 时的 `<file>.<OPT>.cpp` 存根) | `ocv_add_dispatched_file`(OpenCVCompilerOptimizations.cmake) | 仅编译 OpenCV 自身 |
| `opencl_kernels_<m>.{hpp,cpp}` | `cmake -P cmake/cl2cpp.cmake`(.cl → 字符串常量) | 仅编译 OpenCV 自身 |
| dnn 的 `*.pb.cc` | protoc(仓库内 `modules/dnn/misc/` 有预生成副本) | 仅 dnn |

**SIMD dispatch 机制**:49 个 `*.simd.hpp`,CMake 为每个生成 N 份 per-ISA `.cpp` 存根并施加 per-file 编译选项(`-mavx2` 等),运行时 `CV_CPU_DISPATCH` 选择。**mcpp 目前没有 per-source flags**,所以 v0.x 采取"固定 baseline、关 dispatch"(见 §5 决策点 4)。

## 3. 调研:模块封装技术与先例

### 3.1 技术:GMF `#include` + `export using`(imgui-m 同款)

```cpp
module;
#include <opencv2/core.hpp>       // 全局模块片段(GMF)
export module opencv.core;

export namespace cv {
    using ::cv::Mat;
    using ::cv::imread; // ...
    inline constexpr int TYPE_8UC1 = CV_8UC1;   // 宏 → constexpr 常量
}
```

- **标准符合性**:[module.interface]/5 明确允许 export using 指向 GMF 中具外部链接的实体;实体仍附着于全局模块,**名字改编(mangling)不变**——这就是为什么封装层能和"传统方式编译出的 OpenCV 对象文件"直接链接(POC 已实证;`import std` 对 libc++/libstdc++ 二进制也是同一原理)。
- **先例**:Vulkan-Hpp `vulkan.cppm`(GMF include + export using 块)、fmt `src/fmt.cc`(宏侵入式 + `FMT_ATTACH_TO_GLOBAL_MODULE`)、libc++ std module、Boost 模块化原型(clang 上 export-using 可行,MSVC 有 GMF 模板特化被丢弃的坑)。
- **编译器现状(2026 中)**:Clang 为参考平台;GCC 16 modules 仍标"experimental",但 POC 在 gcc 16.1 上以真实导出面跑通;MSVC 有已知坑(std::hash 特化丢弃等)。矩阵:**gcc16(mcpp Linux 默认)为主、clang 为验证、MSVC 延后**。

### 3.2 两个必须直面的问题

1. **宏不可导出**。OpenCV 消费者习惯用的 `CV_8UC1`、`CV_PI`、`CV_Assert`、`CV_Error` 都是宏。方案(业界仅此三种,Vulkan-Hpp/fmt/Boost 各占其一):
   - 纯常量宏 → 模块内 `inline constexpr` 重导出(注意 5.0 新增 `CV_16BF/CV_32U/CV_64U/CV_64S/CV_Bool`);
   - 函数式宏(`CV_Assert` 等)→ 附带一个小的纯文本头 `opencv-m/macros.hpp`(内部只 define 宏,展开引用的 `cv::error` 由模块导出提供);
   - 文档声明:要 100% 宏习惯的场景直接退回 `#include`。
   - 命名待定:见 §5 决策点 3(Python 里没有宏,`cv2.CV_8UC1` 本来就是常量对象,所以 constexpr 化恰好符合"python import 可参考")。
2. **同一程序里混用 `#include <opencv2/...>` 与 `import opencv.*`** 是主要脆弱点(OpenCV 已有真实 bug 报告 [#27899](https://github.com/opencv/opencv/issues/27899):.cppm 与 .cpp 各 include 一次导致重载歧义)。策略:消费者要么全 import(推荐、文档主推),要么 include-before-import;封装层是仓库里唯一在模块上下文 include OpenCV 头的地方。

### 3.3 导出面怎么来:复用 OpenCV 自己的 `hdr_parser.py`

OpenCV Python 绑定的 API 枚举器 `modules/python/src2/hdr_parser.py` 可独立运行(已验证:喂 `preprocessor_definitions={'CV_VERSION_MAJOR':5,...}` 后,解析 `core.hpp` 得 117 条声明:106 函数 + 类 + 枚举)。**用它写一个生成器 `tools/gen_exports.py`,把每个 OpenCV module 的绑定可见 API 自动转成 `export using` 清单**——这正是"python import 可参考"的工程化落点:`import opencv.core` 的初始导出面 == `import cv2` 能看到的面,再人工补充 Python 侧没有的 C++ 惯用面(`Mat::ptr`、`MatExpr` 运算符、`InputArray` 族、迭代器等——运算符和成员随类型自动可用,不需逐个导出)。生成结果提交进仓库(生成器只是开发期工具,消费者构建不依赖 Python)。

## 4. 调研:mcpp 生态与 Rust 做法

### 4.1 mcpp 生态现状(直接相关)

- **`compat.opencv` 4.13.0 已存在于 mcpp-index**(`pkgs/c/compat.opencv.lua`,455 行):在 xpkg `install()` 钩子里跑 OpenCV 自己的 CMake+Ninja 出静态库,消费者 `ldflags` 链接。这正是用户明确**不想要**的"预构建"形态,但它的调研文档非常有价值(`mcpp-index/.agents/docs/2026-07-08-opencv-*.md`):单包 vs 多包的论证、离线化 CMake 选项全集(`-DWITH_ADE=OFF` 杀掉唯一 configure 期下载等)、ABI 匹配要求。**opencv-m 与它是两个生态位:compat.opencv = 预构建二进制 + 头文件;opencv-m = 全源码 + 模块层**(关系定位见 §5 决策点 6)。
- **imgui-m 是仓库形态模板**(Form-A `-m` 仓库):`src/*.cppm` 封装层、examples/、tests/、templates/、`.agents/docs` 设计文档;发布时 index 里只登记 tag tarball(descriptor 无 `mcpp` 字段,走仓库内 mcpp.toml)。差异:imgui-m 上游源码来自 compat 包,**opencv-m 按需求直接 vendor**。
- **mcpp 能力对齐**:glob + `!` 排除、混合 C/C++(`c_standard` 独立)、`include_dirs` 传播、features(可 gate sources 与 defines)、workspace、`import std`(gcc 经 libstdc++ `bits/std.cc`)。**已知缺口**:无 per-source flags(→SIMD dispatch 受限,§5-4)、单包内无 per-target sources(→无法一包多 lib,但我们单包单 lib 不受影响)、glob 不跟软链接。
- mcpp-style-ref:模块命名 `目录.文件` 层级、`.cppm` 接口+实现、少用宏(`inline constexpr` 替代)——本设计全部遵循。

### 4.2 Rust 及其他生态(用户点名调研)

- **Rust `opencv` crate(twistedfall/opencv-rust)不从源码构建**:要求系统/vcpkg 已装 OpenCV + libclang,build.rs 探测(env → pkg-config → cmake → vcpkg),再用 libclang 现场解析头文件生成 C ABI 垫层和 Rust 绑定。**最大用户痛点恰恰是"自备 OpenCV + libclang"**——反向印证了本项目"包内源码构建"的价值。
- Rust 从源码构建大型 C/C++ 库的两种成熟模式:(a) build.rs 调上游构建系统(`cmake` crate、openssl-src);(b) **build.rs 用 `cc` crate 编译人工整理的源码清单**(旗舰例子 librocksdb-sys:~150 万行 C++ 照样列文件表直编)。对生成配置头,业界做法就是**按平台快照提交**或直接 `define()` 硬编码——与本文 §1 的 gen/ 方案一致。
- **无 CMake 构建 OpenCV 的既有先例:mjbots 的 Bazel 移植**(blog + `opencv.bzl`):手工模板 `cvconfig.h`/`cv_cpu_config.h`/`opencv_modules.hpp`、`version_string.inc` 置空、OpenCL 用桩(我们比它更进一步:cl2cpp 独立可跑,能真嵌内核)。证明该路线长期可维护。
- xmake/xrepo、conan、vcpkg 全部是"跑上游 CMake"派;xrepo 的 opencv 包的 feature→`WITH_*` 映射表可作为我们 features 设计的参考词表。
- Zig 生态无可用移植(zig cc + CMake 卡在链接器参数)。

## 5. 核心设计与待 review 决策点

### 仓库布局(提议)

```text
opencv-m/
├── mcpp.toml                  # 单包: name = "opencv"
├── third_party/opencv/        # OpenCV 5.0.0 源码, 原封不动 (git subtree 或直接 vendor)
├── gen/                       # 快照的生成文件 (纯文本, 提交进 git)
│   ├── opencv2/opencv_modules.hpp、cvconfig.h ...
│   └── <module>/ *.simd_declarations.hpp、opencl_kernels_<m>.{hpp,cpp}
├── src/
│   ├── cv.cppm                # export module opencv.cv; export import opencv.core; ... (汇总)
│   ├── core.cppm              # export module opencv.core; (GMF include + 生成的导出清单)
│   ├── core/exports.inc       # tools/gen_exports.py 产物 (被 core.cppm include)
│   ├── imgproc.cppm ...
├── include/opencv-m/macros.hpp  # CV_Assert 等函数式宏的伴随文本头 (可选使用)
├── tools/
│   ├── gen_exports.py         # 基于 third_party/opencv/modules/python/src2/hdr_parser.py
│   └── gen_config.sh          # 开发期一次性: cmake configure + cl2cpp 快照 gen/ (消费者不需要)
├── patches/                   # 目前为空; 若必须改 OpenCV 源码, 补丁 + 理由放这里
├── examples/  tests/  docs/  .agents/docs/
```

### 请 review 的 6 个核心决策点

1. **模块命名**:`opencv.core` / `opencv.imgproc` / … 每个 OpenCV module 一个 C++ module,加汇总模块 **`opencv.cv`**(= `export import` 全部启用模块,对标 `#include <opencv2/opencv.hpp>` 和 `import cv2`)。疑问:汇总名用 `opencv.cv` 还是裸 `opencv`?(mcpp lib-root 惯例是 `src/opencv.cppm` ↔ 包名尾段;两者可并存,`opencv` 作为 lib-root 再 `export import opencv.cv`。)
2. **v0.1 范围**(2026-07-17 review 更新):`core + imgproc + imgcodecs(先 0 codec 或仅 png/jpeg)+ highgui` 四件套 + **videoio**。用户明确要求视频能力优先、且 **FFmpeg 先行单独封装**(独立 mcpp 包 `ffmpeg-m`,同样走"源码 vendor + 配置快照 + 模块层"路线,见 §6 F 阶段);videoio 的 V4L2/图像序列后端零外部依赖可先行,FFmpeg 后端在 ffmpeg-m 就绪后接入。dnn 仍后置。imgcodecs 的编解码器走 mcpp `[features]`(feature gate sources + defines,词表参考 xrepo)。
3. **宏策略**:常量宏 → 模块内 `inline constexpr`(命名沿用原宏名:`cv::CV_8UC1` 这种"命名空间里的原名"vs 我 POC 里的 `cv::TYPE_8UC1`,**需要定夺**);`CV_Assert`/`CV_Error` → 伴随头 `opencv-m/macros.hpp`。
4. **SIMD 策略(性能 vs 机制)**:mcpp 无 per-source flags,v0.x 关闭运行时 dispatch,用**包全局固定 baseline**(建议 x86-64 上 `CPU_BASELINE=SSE4_2` 起步,可选 feature 提到 AVX2);把"per-source flags"作为需求反馈给 mcpp(或未来用 workspace 把各 ISA 档拆成成员包)。**接受 v0.x 比官方构建慢一档的事实,先把机制跑通。**
5. **消费者混用策略**:文档主推"全 import";`#include` 与 `import` 混用在同一程序标记为"自担风险"(上游 #27899 实锤)。是否要提供 Boost 式"兼容头"(`#include <opencv-m/core.hpp>` 内部转 import)?建议 v1 不做(YAGNI)。
6. **与 mcpp-index / compat.opencv 的关系**:opencv-m 作为独立 `-m` 仓库开发,稳定后按 Form-A 登记进 index(与 compat.opencv 并存,分别服务"要模块化+源码构建"和"要快速二进制"两类用户)。vendor 方式建议 git subtree(保留可升级路径)而非 submodule(index tarball 不含 submodule)。

### 风险与未决问题

- **gcc modules 仍为 experimental**:POC 导出面小;导出面放大到全 core API(数百 using)后可能踩 GCC 模板/TU-local 诊断坑。缓解:分阶段扩导出面 + 每步真实编译验证;clang 作为第二验证工具链;必要时对个别符号降级(不导出,文档注明用伴随头)。
- **BMI/构建放大**:GMF 吞下 `core.hpp` 全家桶后 BMI 体积与消费侧编译时间需实测(gcc 无 reduced-BMI;clang 有 `-fmodules-reduced-bmi`)。
- **hdr_parser 的覆盖面**:它只见 `CV_EXPORTS_W` 标注 API;纯 C++ 面(如 `cv::hal`、迭代器、`InputArray` 族)需人工白名单补充——生成器输出必须可叠加人工 curated 段。
- **多平台 gen/ 快照**:cvconfig.h / cv_cpu_config.h 按 (OS, arch) 矩阵各一份,`[target.'cfg(...)']` 选择;v0.1 只做 linux-x86_64,矩阵后补。
- 全模块(含 dnn)后冷构建时间会显著上升(dnn 27 万 LOC + protobuf);dnn 永远是可选 feature。

## 6. 分阶段路线图

| 阶段 | 交付物 | 验证 |
|---|---|---|
| P0 仓库骨架 | vendor `third_party/opencv`(5.0.0 tag);`tools/gen_config.sh` 产出并提交 `gen/`(linux-x86_64);mcpp.toml 雏形 | `mcpp build` 空包过 |
| P1 core 打通 | `src/core.cppm`(手工最小导出面,≈POC)+ zlib feature 化;tests(gtest)跑 Mat 基本用例 | `mcpp test`;冷构建 < 30s |
| P2 导出生成器 | `tools/gen_exports.py`(hdr_parser 驱动)→ `src/core/exports.inc` 全量 core 绑定面 + 常量 constexpr + `macros.hpp` | 编译期全量导出面过 gcc16 + 一个真实小程序(读写 Mat、FileStorage) |
| P3 四件套 | `opencv.imgproc` / `opencv.imgcodecs`(png/jpeg features)/ `opencv.highgui`(GTK 后续,先 headless) | examples: 灰度化+resize+imwrite 端到端 |
| P4 汇总与 UX | `opencv.cv` 汇总模块、examples/、docs/、mcpp templates(`mcpp new --template opencv`) | 新工程 3 行代码跑通 imread→imshow |
| P5 扩展 | clang 工具链验证、macOS、更多模块(video/features/objdetect)、SIMD baseline feature、登记 mcpp-index | CI 矩阵绿 |

**F 系列:ffmpeg-m(独立包,用户 2026-07-17 指定优先做;与 P0-P2 可并行)**

| 阶段 | 交付物 | 验证 |
|---|---|---|
| F0 可行性 POC | 照 opencv POC 方法:`./configure` 一次性快照 config.h/config_components.h,mcpp 直接编译 libavutil + libavcodec(h264/mjpeg)+ libavformat(mp4/mkv) 精简源列表,`--disable-x86asm` 起步 | mcpp 编译出的 demo 解码一个 mp4 首帧 |
| F1 ffmpeg-m 包 | 独立仓库/包:vendor FFmpeg 源码 + `gen/`(按 OS/arch)+ 模块层(`import ffmpeg.avcodec;` 等,C 库封装 = mcpp-style-ref §2.7 lua 模式)+ codec 集合 features | `mcpp test`;LGPL 合规(默认不启用 GPL 部件) |
| F2 接入 videoio | opencv-m 依赖 ffmpeg-m,启用 `cap_ffmpeg` 后端(HAVE_FFMPEG) | VideoCapture 打开 mp4 端到端 |

ffmpeg-m 关键事实(调研):FFmpeg 纯 C(无 C++ ABI/BMI 问题,链接与封装大幅简化);构建系统是自制 `./configure`+make(非 CMake),配置产物同样可快照;源列表由 Makefile 片段的 `OBJS-$(CONFIG_X)` 驱动,需脚本按冻结配置导出;先例:[allyourcodebase/ffmpeg](https://github.com/andrewrk/ffmpeg)(build.zig 完全替换 FFmpeg 构建系统、零系统依赖、单静态库)、[dmorn/ffmpeg.zig](https://github.com/dmorn/ffmpeg.zig)、Rust `ffmpeg-sys-next` 的 vendored build。

**x86 NASM 汇编:用 `build.mcpp` 驱动(2026-07-17 review 确定,已实测特性可用)**。mcpp 自 v0.0.81 提供 `build.mcpp`(Cargo `build.rs` 对应物,docs/07),在主构建前于宿主机运行、可发射 `link-lib`/`link-search`/`cfg`/`generated` 指令——正是 Cargo 生态 build.rs 调 nasm 的同款模式:

```cpp
// ffmpeg-m/build.mcpp (示意)
import mcpp;
int main() {
    if (host_has("nasm")) {                       // 探测宿主 nasm ([xlings] deps 供给)
        for (auto& f : asm_list) run_nasm(f);     // .asm → obj/*.o
        make_archive("vendor/lib/libffmpeg_asm.a");
        mcpp::link_search("vendor/lib");
        mcpp::link_lib("ffmpeg_asm");
        mcpp::define("HAVE_X86ASM=1");
    } else {
        mcpp::define("HAVE_X86ASM=0");            // 优雅降级 = --disable-x86asm 纯 C 路径
    }
    for (auto& f : asm_list) mcpp::rerun_if_changed(f);
}
```

注意事项(2026-07-17 复核更新):

- ① nasm 经 `[xlings] deps` 供给,xlings 生态自包含、不依赖 host——所以"宿主没 nasm"**不是**降级场景:nasm 缺失应当硬失败(供给 bug),而非静默出慢一档的产物(可复现性优先)。xlings 当前未打包 nasm,需先加包(自家生态,成本低)。
- ② `build.mcpp` 在 cross build(`--target`)下被跳过(mcpp docs/07 文档化限制,host-toolchain-for-cross 是计划项)→ cross 场景暂时落纯 C 默认态。
- ③ **实测发现的真正缺口(2026-07-17,mcpp 0.0.93)**:**mcpp 不运行"依赖包"的 `build.mcpp`**——path 依赖实验:依赖包单独 `mcpp build` 时 `build.mcpp compiling/running` 正常,但作为依赖被消费时静默不执行(连警告都没有)。而 ffmpeg-m 的主要形态恰恰是"被 opencv-m 依赖"。**这是需要反馈给 mcpp 的特性需求**(Cargo 语义:运行依赖的 build.rs,flags 作用于依赖自身 TU,link 指令传播到最终链接)。在 mcpp 支持前,ffmpeg-m 作为依赖只有纯 C 路径;作为根项目可用 build.mcpp 挂汇编。
- ④ 纯 C 默认态(`HAVE_X86ASM 0` 快照)仍是设计基石:它是 ②③ 及非 x86 平台的公共兜底;`build.mcpp` 只做"锦上添花",不承担正确性。
- ⑤ **同一招反过来救 OpenCV 的 SIMD dispatch**:`build.mcpp` 可对 `.opt.cpp` 存根逐档加 `-mavx2` 等编译成旁路静态库再 link-lib,绕开"mcpp 无 per-source flags"的缺口——同样受 ③ 制约(opencv-m 被消费时不执行),P5 前需 mcpp 先支持依赖 build.mcpp。

**给 mcpp 的特性需求清单(由本项目产生)**:详见独立文档 [`2026-07-17-mcpp-feature-requests.md`](2026-07-17-mcpp-feature-requests.md)(A1 依赖包 build.mcpp、A2 cross build.mcpp、A3 xlings 打包 nasm、B1 原生 .asm/.S 源、B2 per-source flags、C1-C3 体验项,含优先级×阶段映射)。

每阶段结束出一份 `.agents/docs` 增量设计记录(沿用 imgui-m 惯例)。P1 开始前,基于本文档 review 结论产出逐任务实施计划(superpowers:writing-plans 格式)。

---

## 附录 A:POC 复现(scratchpad `spike2/`,2026-07-17)

**生成文件快照命令**(开发期一次性;消费者不需要):

```bash
# 1) configure-only, 生成配置头 (不编译)
cmake -S third_party/opencv -B /tmp/ocv-cfg -DBUILD_LIST=core -DBUILD_SHARED_LIBS=OFF \
  -DWITH_OPENCL=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_OPENMP=OFF \
  -DCV_DISABLE_OPTIMIZATION=ON -DCPU_BASELINE= -DCPU_DISPATCH= \
  -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_ZLIB=ON \
  -DWITH_EIGEN=OFF -DWITH_LAPACK=OFF
# 2) opencl 内核嵌入 (独立脚本, 与 configure 无关)
cmake -DMODULE_NAME=core -DCL_DIR=third_party/opencv/modules/core/src/opencl \
  -DOUTPUT=gen/core/opencl_kernels_core.cpp -P third_party/opencv/cmake/cl2cpp.cmake
# 3) 拷贝快照: cvconfig.h cv_cpu_config.h custom_hal.hpp opencv_data_config.hpp
#    opencv2/{cvconfig.h,opencv_modules.hpp} modules/core/{*.simd_declarations.hpp,version_string.inc}
#    3rdparty/zlib/zconf.h
```

**POC mcpp.toml**(实测通过):

```toml
[package]
name    = "cvspike2"
version = "0.0.1"

[build]
sources = [
  "src/**/*.cppm", "src/main.cpp",
  "opencv/modules/core/src/**/*.cpp",
  "opencv/3rdparty/zlib/*.c",
  "gen/core/opencl_kernels_core.cpp",
]
include_dirs = [
  "gen", "gen/core",
  "opencv/modules/core/include",
  "opencv/modules/core/src",
  "opencv/3rdparty/zlib",
]
cxxflags = ["-D__OPENCV_BUILD=1", "-w"]
cflags   = ["-w", "-DZ_HAVE_UNISTD_H", "-DHAVE_UNISTD_H", "-D_POSIX_C_SOURCE=200809L"]
ldflags  = ["-lpthread", "-ldl"]

[targets.cvspike2]
kind = "bin"
main = "src/main.cpp"
```

(正式仓库中 `-w` 换成定向告警抑制,`__OPENCV_BUILD` 语义见 §1-7。)

## 附录 B:参考资料

- 本地:`mcpp/docs/05-mcpp-toml.md`、`imgui-m/docs/architecture.md` 与 `.agents/docs/*`、`mcpp-index/.agents/docs/2026-07-08-opencv-*.md`(compat.opencv 调研/实现)、`mcpp-style-ref/README.md`
- OpenCV:[5.0.0 release](https://github.com/opencv/opencv/releases/tag/5.0.0)、[4→5 迁移指南](https://github.com/opencv/opencv/wiki/OpenCV-4-to-5-migration)、[issue #27308 C++ modules 请求](https://github.com/opencv/opencv/issues/27308)、[issue #27899 include/import 混用 bug](https://github.com/opencv/opencv/issues/27899)
- 模块化先例:[Vulkan-Hpp vulkan.cppm](https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/vulkan/vulkan.cppm)、[fmt src/fmt.cc](https://github.com/fmtlib/fmt/blob/master/src/fmt.cc)、[Boost 模块化系列](https://anarthal.github.io/cppblog/modules)、[libc++ std module](https://libcxx.llvm.org/Modules.html)、[Clang modules 文档](https://clang.llvm.org/docs/StandardCPlusPlusModules.html)
- 无 CMake 构建先例:[mjbots Bazel-for-OpenCV](https://blog.mjbots.com/2018/12/28/bazel-for-opencv/)、[opencv.bzl](https://github.com/mjbots/bazel_deps/blob/master/tools/workspace/opencv/opencv.bzl)
- Rust:[opencv-rust](https://github.com/twistedfall/opencv-rust)(不从源码构建;痛点=自备 OpenCV+libclang)、[rust-sys-crate 模式](https://kornel.ski/rust-sys-crate)、[librocksdb-sys build.rs](https://github.com/rust-rocksdb/rust-rocksdb/blob/master/librocksdb-sys/build.rs)、[openssl-src 模式](https://crates.io/crates/openssl-src)
- 其他:[xmake-repo opencv 包](https://github.com/xmake-io/xmake-repo/blob/master/packages/o/opencv/xmake.lua)(feature 词表参考)
