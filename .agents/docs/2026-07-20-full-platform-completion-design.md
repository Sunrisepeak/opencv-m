# 全平台收尾设计方案 + 验证方法

> 状态基线(2026-07-20):`compat.opencv` 已三平台(linux/macOS 全 profile、windows core),
> `compat.ffmpeg` + `ffmpeg` 模块已三平台,`opencv` 模块(`import opencv.cv`)仍 linux-only。
> 本文覆盖剩余 4 项 + 一个构建告警修复,给出**设计方案**与**可本地执行的验证方法**。

## 0. 当前状态与目标

| 层 | linux | macOS | windows | 缺口 |
|---|---|---|---|---|
| compat.opencv(源码构建) | full | full | **core** | windows videoio(项 B)、windows jpeg-SIMD(项 C)、macOS dnn(项 D) |
| opencv 模块(`import opencv.cv`) | ✅ | ❌ | ❌ | clang 导出兼容层(项 A,关键) |
| compat.ffmpeg / ffmpeg 模块 | ✅ | ✅ | ✅ | — |

**总目标**:`import opencv.cv` 三平台可用 + `compat.opencv` 三平台 full profile。
**关键路径**:**项 A(clang 导出兼容层)是 opencv 模块上 macOS/windows 的唯一硬门槛**;
项 B 是「windows 模块 full」的前置(模块聚合 videoio);项 C/D 是 compat 层完整性,彼此独立。

依赖关系图:

```
项A (clang导出兼容层) ──→ opencv 模块 macOS 可用
        │
        └──(+ 项B windows videoio)──→ opencv 模块 windows 可用
项B (windows videoio) ──→ compat.opencv windows = full
项C (windows jpeg-SIMD) ── 独立,纯性能
项D (macOS dnn) ── 独立,opencv-dnn 模块 macOS
告警修复 ── 独立,任何时候可做(建议随项A/B一起)
```

---

## 项 A（关键,规模:大):opencv 模块 clang 导出兼容层

### A.1 问题(已本地复现)

macOS/windows 用 clang;`opencv-m` 的导出面是 `src/gen_exports/*.inc` 里成批的
`export using cv::NAME;`。**clang 拒绝导出「目标具有内部链接的 using 声明」**,只要 `cv::NAME`
存在**任一** `static inline` 重载即触发(`using` 对一个名字是全有或全无)。gcc 靠 COMDAT/
vague-linkage 容忍,所以 linux 一直绿 —— 这是 v0.0.2「static-inline saga」从**运算符**
扩展到**整个 core 导出面**。

**本地复现(clang 18.1.3,Ubuntu)** —— 无需 macOS CI:

```cpp
// m.cppm
module;
namespace cv { static inline int makePtr(int){return 1;} int makePtr(double); }
export module opencv.core;
export namespace cv { using cv::makePtr; }
```
```
$ clang++ -std=c++20 --precompile m.cppm
error: using declaration referring to 'makePtr' with internal linkage cannot be exported
```

**受影响集合(CI 观测 + 待用 A.4 权威枚举)**:core 面 ~20+:
`abs, alignPtr, alignSize, cubeRoot, determinant, divUp, format, getElemSize, makePtr,
max, min, norm, roundUp, swap, trace, transform, read, write, randu, print, …`;
imgproc:`initWideAngleProjMap, morphologyDefaultBorderValue`(v0.0.5 已 prune);
imgcodecs:`imwritemulti`(v0.0.5 已 prune)。
> 注意:`cv::abs/max/min` 等有**混合重载**(Mat 版 `CV_EXPORTS` 外部链接 + 标量版
> `static inline` 内部链接);`using` 无法只导出外部那部分,所以整名失败。

### A.2 方案:替换面(replacement surface),不是 prune

prune 会丢功能(makePtr/format/abs/max 是核心 API),**不可接受**。采用 R1/R2 已验证的
「替换面」模式,从运算符扩展到函数:对每个「因内部链接重载而不可 `export using`」的名字,
在模块内提供一个**外部链接的重实现/转发**,放进 `cv::inline namespace opencv_m_repl`,
导出它;纯 importer 经 ADL 只看到这个外部版本。

**本地已验证该机制可行(clang 18.1.3)**:

```cpp
// m.cppm
module;
namespace cv { static inline int val(int x){return x+1;} }   // 上游:内部链接,不可导出
export module opencv.core;
export namespace cv { inline namespace opencv_m_repl {
  inline int val(int x){return x+1;}                          // 外部链接(inline),导出 + ADL 可见
} }
```
```cpp
// main.cpp
import opencv.core;
int main(){ return cv::val(41)==42 ? 0 : 1; }   // ✅ 编译链接运行通过(退出 0)
```

### A.3 重实现分类(工作量在这里)

按签名复杂度分档,逐档处理:

| 档 | 名字(示例) | 策略 |
|---|---|---|
| **平凡内联** | `alignPtr, alignSize, roundUp, divUp, getElemSize` | 直接照抄 header 的一行实现(自包含、无内部依赖),`inline`(外部链接)导出 |
| **模板工厂** | `makePtr` | `template<class T,class...A> Ptr<T> makePtr(A&&...a){ return Ptr<T>(new T(std::forward<A>(a)...)); }`(自包含) |
| **数学模板** | `abs, max, min, cubeRoot, norm, determinant, trace, transform` | 部分 R1 已在 `src/matx_ops.inc` 做过(determinant/trace/norm for Matx);对 `abs/max/min` 只重实现**标量/Vec/Matx 的内部链接重载**,Mat 版本(外部链接)仍走原 `using`(拆名:`abs` 整名冲突时,改为「不 `using cv::abs`,而是替换面提供全部需要的重载」——见 A.5 冲突处理) |
| **可变参** | `format` | `String format(const char* fmt, ...)` 其实是 `CV_EXPORTS`(外部);若报错是因另一个 inline 重载,则替换面提供该 inline 重载 |
| **不该导出** | `read, write, print, swap`(若目标是内部 helper) | 判断是否面向用户;非用户 API 直接 prune(加 `*.prune.txt`) |

> 判定「该替换 vs 该 prune」的准则:**用户 API(有文档、常用)→ 替换**;
> **内部 helper / 仅头文件实现细节 → prune**。A.4 的 harness 会对每个名字给出其重载来源
> 头文件与链接种类,据此归档。

### A.4 权威枚举(harness,本地 clang)

不靠肉眼 grep(会误判混合重载)。写 `tools/clang_export_probe.sh`:

1. 取 `src/gen_exports/<mod>.inc` 里每个 `using cv::NAME;`。
2. 对每个 NAME 生成最小 `.cppm`:GMF `#include <opencv2/<mod>.hpp>`(+ 合成最小
   `cvconfig.h`/`cv_cpu_config.h`,或复用一次 compat 构建产物),purview `export using cv::NAME;`。
3. `clang++ -std=c++23 --precompile` —— 记录 pass/fail + clang 给出的「target of using
   declaration」指向的头文件行(即那个 `static inline` 定义)。
4. 输出:每 mod 的「必须替换/prune 的名字」清单 + 来源。

这把「整个 core 导出面」变成一份**确定的、可复现的**清单,驱动 A.3 的逐名处理。

### A.5 混合 TU 冲突处理(沿用 R2)

同一 TU 同时**文本包含** header(内部链接版)+ `import`(替换版)时会重载歧义。R2 已给出
赢法:替换版用**带平凡真约束的模板**(`template<class T> requires opencv_m_pick<T>`),
C++20 subsumption 让「更受约束」的模块实体在混合 TU 里确定性胜出;纯 import 只见替换版。
非模板内部链接目标(如某些标量 helper)则相反 —— 替换版做成受约束模板,让上游非模板版
在混合 TU 胜出。**A.4 的每名清单里标注 template/non-template,套用对应 R2 策略。**

### A.6 验证方法(全本地,clang = macOS/windows 代理)

- **单元**:A.4 harness 对每个 mod 的接口单元 `clang++ --precompile` 全绿(0 个
  internal-linkage 错误)。
- **集成**:`clang++ -std=c++23` 编译 `opencv.cv` 全部接口单元 + 一个 `import opencv.cv;`
  的 importer(调用 makePtr/format/abs/resize 等),链接运行。
- **混合 TU**:一个 TU 同时 `#include <opencv2/core.hpp>` + `import opencv.core;`,验证 R2
  无歧义(gcc + clang 双编译)。
- **回归**:linux gcc 仍绿(现状不退)。
- **终验**:mcpp-index 加 `opencv-module` 的 `cfg(macos)`(+ 后续 windows)成员,CI macOS 腿
  真机绿 —— 但**迭代全在本地 clang 完成**,CI 只做最终确认。

### A.7 产出

`opencv-m`:`src/gen_exports/*.inc`(移除不可导出名)+ `src/*_repl.inc`(替换面,扩展现有
`core_ops.inc`/`matx_ops.inc`)+ `tools/clang_export_probe.sh` + `tools/gen_exports.py`
(集成 probe,回归防止再退)→ 发 `opencv-m v0.0.6` → mcpp-index re-point + 放宽
`opencv-module` cfg(macos)。

---

## 项 B（规模:中):windows videoio + FFmpeg

### B.1 问题
windows `compat.opencv` 现为 core(无 videoio)。full 需 cmake 配置时探测到 FFmpeg 头/库以
编译 `cap_ffmpeg.cpp`。消费端仍从 `compat.ffmpeg`(已三平台)取库;快照只为让 cmake 生成
videoio 配置。

### B.2 方案
1. windows 快照 workflow(`snapshot-windows-opencv.yml`)增加一步:MSYS2 里
   `ffmpeg configure --toolchain=msvc --enable-shared --disable-static` + `make install` 到
   `ffprefix`(参照现有 `snapshot-windows-ffmpeg.yml` 已证明的 configure 路径 + macOS 全 profile
   快照的 shared-ffmpeg 前缀做法)。
2. cmake:`-DWITH_FFMPEG=ON` + 经 `pkg-config`(MSYS2 提供)指向 `ffprefix/lib/pkgconfig`。
   **风险点**:OpenCV windows 的 FFmpeg 探测历史上偏向预置 dll;需确认 5.0.0 的
   `OpenCVFindLibsVideo.cmake` 在 pkg-config 命中时是否直接用(而非下载 dll)。若不行,退路是
   `-DOPENCV_FFMPEG_USE_FIND_PACKAGE=ON` 或直接喂 `FFMPEG_INCLUDE_DIRS/FFMPEG_LIBRARIES`。
3. `BUILD_LIST += videoio`;去掉 core-only 限制;`gen_descriptor.py` 已支持 windows,
   videoio 源 + `HAVE_FFMPEG` define 会自动进快照;windows 块加 `deps = { compat.ffmpeg }`
   (per-OS deps,已支持)。

### B.3 验证方法
- 快照 workflow:`grep -q cap_ffmpeg bld/ninja-cmds.log`(参照 macOS 快照的探测门)。
- 构建 spike:windows 成员做 mp4 encode→decode 往返(即把 `opencv-win` 测试升级到含 videoio,
  与 linux/macOS `opencv` 成员一致)。
- 完成后 windows compat = full → **`opencv` 成员可直接放宽 cfg(windows)**(与项 A 合流后模块也可)。

---

## 项 C(规模:小-中):windows jpeg-turbo NASM SIMD

### C.1 问题
现 windows `ENABLE_LIBJPEG_TURBO_SIMD=OFF`(C 回退,功能完整仅非 SIMD)。开启时链接报 124 个
`jsimd_*`/`jconst_*` 未定义 —— **注意 compat.ffmpeg 的 159 个 win64 NASM 符号链接正常**,故非
mcpp 通用 win64-NASM 缺陷,而是 libjpeg-turbo `simd/nasm/jsimdext.inc` 的 win64 符号修饰特例。

### C.2 方案(调查优先)
1. 本地/CI 反汇编一个 jpeg `.asm` 的 win64 目标(`llvm-objdump -t jccolor-avx2.obj`)与
   `jsimd.c` 引用的符号名逐字对比 —— 判断是否 EXTN 前缀(下划线)不一致,或 `global` 修饰缺失。
2. 对比 ffmpeg NASM(链接正常)与 jpeg NASM 的 `global`/前缀差异。
3. 若是 EXTN 前缀问题:给 jpeg NASM 传对的定义(`-DPREFIX` 或 `-D__x86_64__` 之外的 win64
   符号宏),或在 gen_descriptor 为 jpeg-asm 组补 asmflags。
4. 若确属 mcpp 汇编/链接侧(例如 mcpp 未把某类 win64 nasm .obj 传给链接器)→ 复现最小例
   **再** 报 mcpp issue(避免 #251 式误报:先本地反汇编确认符号确实在 .obj 里)。

### C.3 验证
windows 快照去掉 `ENABLE_LIBJPEG_TURBO_SIMD=OFF` → 构建 spike 链接通过 + jpeg 往返一致。
纯性能项,不阻塞功能。

---

## 项 D(规模:中):macOS dnn

### D.1 问题
`compat.opencv` 的 `dnn` feature 现为 linux-x86(AVX kernels + protobuf + mlas)。macOS 快照
当年 `BUILD_LIST` 未含 dnn,故 `features.dnn` 在 macosx 为空。opencv-dnn 模块因此留 linux。

### D.2 方案
1. macOS 快照 workflow `BUILD_LIST += dnn` + `-DWITH_PROTOBUF=ON -DWITH_FLATBUFFERS=ON`
   (对齐 linux dnn 参考构建的 R5 做法),NEON 下 dnn 的 SIMD 走 universal-intrinsics(无 x86 AVX
   kernel);mlas 在 arm 的可用子集需确认(可能需 `MlasHGemmSupported` 之外更多 curated stub)。
2. `gen_descriptor.py`(已支持 aarch64)生成 macosx 的 `features.dnn` 源 —— 但 **features 目前是
   顶层中性键**;dnn 源 x86/arm 不同,需把 **`features.dnn` 也做成 per-OS**(mcpp 是否支持
   per-OS features 待 probe;若不支持,退路:dnn feature 的源用 `cfg`-无关的公共集 + per-OS
   SIMD 覆盖,或把 dnn 拆成 `compat.opencv-dnn` 独立 3-平台 xpm 包)。
3. `merge_opencv.lua`:若 per-OS features 可行,把 dnn 从中性移到 per-OS。

### D.3 验证
macOS 快照 dnn spike 构建 + `blobFromImage`/`Net` 断言(对齐 linux dnn 成员测试);
mcpp-index `opencv-module-dnn` 放宽 cfg(macos)(**依赖项 A** —— dnn 模块接口也走 `import`)。

---

## 告警修复(独立,建议随项 A/B 提交)

现象(消费 compat.opencv 时):
```
warning: [build].flags glob '**/3rdparty/mlas/**' matched no source file
warning: [build].flags glob '**/clsrc/opencl_kernels_core.cpp' matched no source file
warning: [build].flags glob '**/clsrc/opencl_kernels_{geometry,imgproc}.cpp' matched no source file
```

### 根因
- `**/3rdparty/mlas/**`:mlas 源**只在 dnn feature**里;base(无 dnn)构建下该 flags-glob 匹配 0 源。
- `**/clsrc/opencl_kernels_*.cpp`:这些 .cpp 由 **build.mcpp 合成**(`cl2cpp`),其编译 flags 随
  build.mcpp 的 `mcpp:generated=` 指令附带;描述符里再挂一份 flags-glob 是**死条目**,base 构建
  时匹配 0 源(合成时机/来源不在描述符声明源集)。

### 方案(`tools/compat-opencv/gen_descriptor.py`)
1. **mlas glob** → 移入 `features.dnn`(与 mlas 源同处);base 不再产生该 glob。
2. **opencl_kernels globs** → 不再作为独立 flags-glob 发出(其 flags 已由 build.mcpp 合成 kernel
   时携带);或加「仅当该 glob 在声明源集有匹配才发出」的守卫。
3. 重生成三平台 `compat.opencv.lua`(经 `merge_opencv.lua`),`mcpp xpkg parse --all-os` +
   一次消费构建确认**零 "matched no source file" 告警**。

> 这些告警当前**无害**(死 flags 不影响产物),但 mcpp 生态用户不该看到噪音 —— 属「架构好 +
> 简单使用」范畴,优先做。

---

## 建议排期

1. **告警修复**(半天,gen_descriptor,立即提升观感)。
2. **项 A clang 导出兼容层**(关键,大;全本地 clang 迭代)→ opencv 模块 macOS 上线。
3. **项 B windows videoio**(中)→ compat.opencv windows = full → `opencv` 成员放宽 cfg(windows)。
4. **项 A + B 合流** → opencv 模块 windows 上线(真正「import opencv.cv 三平台」)。
5. **项 D macOS dnn** / **项 C windows jpeg-SIMD**(独立,按需)。

## 验证方法总纲

| 项 | 本地可验? | 方法 |
|---|---|---|
| A | ✅ 全本地 | clang++ probe harness(单名)+ import 集成 + 混合 TU 双编译 + gcc 回归 |
| B | 部分 | 快照 workflow `grep cap_ffmpeg` + windows 成员 mp4 往返(CI） |
| C | 部分 | `llvm-objdump -t` 本地符号对比 + windows spike 链接（CI) |
| D | 部分 | macOS 快照 dnn 构建 + blobFromImage 断言（CI) |
| 告警 | ✅ 本地 | `xpkg parse --all-os` + 消费构建 grep 无告警 |
