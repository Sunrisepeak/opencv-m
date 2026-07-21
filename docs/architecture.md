# opencv-m 架构设计说明

**中文** · [English](architecture.en.md)

本文说明 opencv-m 的整体结构、各层职责与关键设计决策的依据。阅读对象是需要修改本仓库、
或需要理解"C++23 模块封装一个大型 C++ 头文件库"这一问题的开发者。

---

## 1. 设计目标与约束

opencv-m 要同时满足三项要求,而这三项在既有方案中相互冲突:

1. **消费端以模块方式使用 OpenCV**:`import opencv.cv;` 取代 `#include <opencv2/opencv.hpp>`,
   同时 API 拼写保持不变(仍是 `cv::Mat`、`cv::cvtColor`)。
2. **消费端不需要安装 OpenCV,也不需要执行 CMake**:依赖声明后由 mcpp 完成全部构建。
3. **功能不缩水**:imgcodecs(PNG/JPEG)、videoio(FFmpeg 后端)、dnn 在 Linux、macOS、
   Windows 三平台均可用,且保留运行时 SIMD dispatch。

约束条件有二。其一,OpenCV 的构建配置由 CMake 生成:`cvconfig.h`、`cv_cpu_config.h`、
各模块的 SIMD dispatch 派生 TU 均是构建期产物,消费端不可能在没有 CMake 的前提下得到它们。
其二,C++ 模块的导出语义比头文件严格:宏无法跨模块边界,`static inline`(内部链接)实体
无法被 `export using` 重新导出——clang 直接拒绝,gcc 只是宽容。

本仓库的架构即是对这两项约束的系统性回应。

## 2. 分层结构

```
消费者项目
  └─ import opencv.cv;                     ← 模块接口层
        └─ src/*.cppm  +  src/*.inc        ← 导出面(生成 + 手写替换层)
              └─ third_party/opencv-5.0.0  ← vendored 上游源码(零补丁)
                   +  gen/                 ← 冻结的构建配置快照(替代 CMake)
                   +  mcpp.toml / build.mcpp ← 构建描述(替代 CMakeLists)
                   +  compat.ffmpeg        ← 唯一外部依赖(videoio 后端)
```

各层职责如下。

### 2.1 vendored 上游源码 —— `third_party/opencv-5.0.0/`

OpenCV 5.0.0 官方 tag tarball 的裁剪导入,sha256 锁定,**不含任何补丁**。保留 8 个模块
(core、imgproc、imgcodecs、highgui、videoio、dnn、flann、geometry)与 7 个第三方组件
(libjpeg-turbo、libpng、zlib、protobuf、mlas、flatbuffers、dlpack);test/perf 树与语言绑定
目录被裁除,但 `modules/python/src2` 予以保留,因为其中的 `hdr_parser.py` 是导出面生成器的输入。
裁剪后为 1969 个文件、48 MB。

零补丁是刻意的约束:上游版本升级时,`tools/vendor/import_opencv.sh` 重新导入即可,
无需重放补丁集合。所有平台差异都被推到 `gen/` 与 `mcpp.toml` 中表达。

### 2.2 冻结配置快照 —— `gen/`

CMake 在配置阶段生成的头文件与派生 TU,在本仓库中以**已生成的真实文件**形式提交:

| 目录 | 内容 | 规模 |
|---|---|---|
| `gen/common/` | 三平台逐字节相同的产物(SIMD dispatch 包装 TU、`opencv_modules.hpp` 等)与 `gen/common/synth/`(字体、OpenCL kernel 等合成资源) | 257 文件 |
| `gen/linux/`、`gen/macosx/`、`gen/windows/` | 平台相关的 `cvconfig.h`、`cv_cpu_config.h` 等,附 `INCLUDE_DIRS.txt` 与 `DNN_SOURCES.txt` | 各 43–45 文件 |

这些快照由 `tools/vendor/port_descriptor.py` 一次性移植自已退役的 `compat.opencv` 描述符,
移植过程包含跨平台去重、冲突检测与逐字节校验。提交生成物而非在构建期运行 CMake,是使
"消费端不跑 CMake"成为可能的前提;代价是上游升级时需要重新执行一次移植流程。

### 2.3 构建描述 —— `mcpp.toml` 与 `build.mcpp`

`mcpp.toml`(1449 行)承载可声明的部分:源文件 glob、公共 include 目录、51 组 `[[build.flags]]`
逐 glob 编译选项、`[target.'cfg(os)'.build]` 的平台条件源集合、`[features]` 定义、以及唯一的外部
依赖 `compat.ffmpeg`。

`build.mcpp` 只承担清单语法**无法表达**的三件事,不多做:

1. 按目标平台读取 `gen/<os>/INCLUDE_DIRS.txt`,发出私有 include 目录(清单没有平台条件的
   `include_dirs` 键);
2. `dnn` 特性开启时,注入 `gen/<os>/DNN_SOURCES.txt` 列出的平台相关源文件(清单没有
   "平台 × 特性"的交叉维度);
3. `unifont` 特性开启时,将 `assets/WenQuanYiMicroHei.ttf.gz` 以十六进制嵌入生成头文件
   (等价于 CMake 的 `ocv_blob2hdr`)。

该文件采用 modules-first 风格,通过 `import mcpp;` 使用类型化 API。需要注意:`import std;`
在 build.mcpp 的编译上下文中不可用(经验证仅 `mcpp` 模块被接入),因此文本 include 保留,
且必须位于 `import` 之前。

### 2.4 模块接口层 —— `src/`

每个 `opencv.<mod>` 是一个自包含模块:在全局模块片段(GMF)中 `#include` 对应上游头文件,
在 purview 中通过 `export using cv::name;` 逐名导出。模块之间不互相 `import`,`opencv.cv`
是唯一的聚合入口(`export import` 各子模块)。

导出面由两部分合成:

- **生成部分** `src/gen_exports/*.inc`(2267 行):`tools/gen_exports.py` 调用上游
  `hdr_parser.py` 枚举 `CV_EXPORTS_W` 表面,叠加 `tools/curated/<mod>.txt` 中人工补充的名字
  (生成器看不见的模板 typedef 家族 `Point2i`/`Vec3b`/`Matx33f`、`InputArray` 家族、traits 等)。
  名字跨模块去重,每个实体只由一个归属模块导出。
- **人工替换部分** `src/*_ops.inc` / `src/core_fns.inc`:见 §3.1。

`tools/prune_loop.py` 构成生成器的闭环:编译失败时解析指向 `gen_exports/*.inc` 的错误,
将不可导出的名字连同原因追加到 `tools/curated/<mod>.prune.txt`,重新生成并重试,直至构建通过。
当前累计裁剪 431 行。

## 3. 关键设计决策

### 3.1 内部链接实体的替换层

OpenCV 的运算符与部分命名自由函数以 `static inline` 定义在头文件中,属内部链接,无法被
`export using` 重导出。本仓库以等价的 `inline`(外部链接)定义替换该表面,分三个文件:

| 文件 | 覆盖范围 |
|---|---|
| `src/core_ops.inc` | `saturate_cast`、Point/Size/Rect/Range/Scalar、Mat/MatExpr 运算符 |
| `src/matx_ops.inc` | Matx/Vec 代数 |
| `src/core_fns.inc` | 命名自由函数(`cv::format`、`cv::print` 等按上游语义重实现) |

替换层是 `import opencv.cv;` 能在 clang(macOS/Windows)与 gcc 上同时编译的直接原因。
混合 TU(既 include 头文件又 import 模块)中,上游的精确匹配 `static inline` 版本重载
更特化,因此语义不变。

### 3.2 模板参数的推导形式(clang BMI 约束)

在 clang 20 与 22 上存在一项实测约束:模板参数未被**第一个实参**完全绑定的函数/运算符模板,
经模块 BMI 序列化后,导入方一旦使用该名字即触发前端崩溃(clang 18 无此问题,属 18→20 回归)。
受影响的形态包括 4 个 int 参数的 `Matx × Matx` 乘法、`Matx<_Tp,m,m>` 的 `determinant`、
双 typename 的 `+=`/`-=`、以及逗号初始化 `<<`。

`src/matx_ops.inc` 因此改用**整类型推导**:以 `template<typename _MA, typename _MB>` 声明,
再用 `__is_same(_MA, typename _MA::mat_type)` 之类的自校验约束回绑,使全部模板参数由实参类型
本身确定。该形式在 gcc 上语义等价并已回归验证。相关最小复现与分析见 mcpp#256。

### 3.3 宏的处理

模块不能导出宏。对象式常量宏(`CV_8U`、`CV_PI`、`CV_MAKETYPE` 等)以保持原拼写的
`cv::` constexpr 值/函数形式导出;函数式宏(`CV_Assert`、`CV_Error`、版本宏)无法如此处理,
需要原始拼写的 TU 应在 `import` 之前 `#include <opencv-m/macros.hpp>`。

### 3.4 平台条件编译与 Windows 的临时层

mcpp 0.0.101 的清单语法中,`sources` 可按平台条件化,`flags` 不可——`[[build.flags]]`
是全局表。Linux 与 macOS 的差异是纯增量且定义名互不重叠,故通过将平台相关定义提升为
per-OS 全局 `cflags` 解决(提升前逐名 grep 验证其仅被本代码组读取)。

Windows 的差异同时包含**增加与删除**(例如 zlib 组在 unix 下定义 `HAVE_UNISTD_H=1`,
Windows 必须不定义),而删除无法通过无条件的全局表表达。当前采用**桩命名空间**过渡方案:
每个 Windows TU 对应 `gen/windows/tu/w*/` 下的一行 `#include` 桩文件(共 703 个),使
Windows 的 flag 表得以按 Windows 独有的路径 glob 命中。

该层是临时的。mcpp 支持 `[target.'cfg(os)'.build].flags` 后(mcpp#258),703 个桩文件与
mcpp.toml 中 32 条对应 glob 条目将一并删除,构建期的 23 条 dead-glob 警告亦随之消失。

### 3.5 特性划分

| 特性 | 内容 | 理由 |
|---|---|---|
| `dnn` | 追加 `import opencv.dnn;` 接口与底层 dnn 源(含 vendored protobuf) | 底层多出 300 余个 TU,仅对显式开启者构建;`opencv.cv` umbrella 默认不含 dnn |
| `unifont` | 内嵌 WenQuanYi Micro Hei 字体,支持 `FontFace("uni")` 的 Unicode/CJK `putText` | 字体资源体积与渲染路径仅对需要者有意义 |

`dnn` 的 gemm 后端按平台选择:Linux 与 macOS 使用 mlas(x86 AVX/AVX2/AVX512、arm NEON);
Windows 使用 OpenCV 内置的 `fast_gemm`——上游 mlas 的 x86 汇编为 GAS/ELF 语法,clang-cl
无法产出 COFF,故沿用上游"无 ASM 即回退 fast_gemm"的既定路径。此平台差异依赖 mcpp#253
引入的 per-OS 特性语义,故要求 mcpp ≥ 0.0.101。

### 3.6 依赖边界

外部依赖只有一个:`compat.ffmpeg`,为 videoio 的 FFmpeg 后端(`cap_ffmpeg`)提供实现。
其余全部内联于本仓库。包索引侧仅保留一个指向本仓库 Release 的薄 `opencv.lua`,不再直连
上游 opencv.org 的分发物。

## 4. 验证策略

CI 为平台 × 工具链矩阵,五条腿全部为必需:

| 腿 | 平台与工具链 |
|---|---|
| linux-gcc | Ubuntu / gcc 16.1.0(附加 `dnn,unifont` 特性构建与模板占位符检查) |
| linux-llvm-22 | Ubuntu / llvm 22.1.8 |
| linux-musl-static | Ubuntu / gcc 16.1.0 `--target x86_64-linux-musl`,静态链接 |
| macos-llvm | macOS 15 / llvm(arm64) |
| windows-llvm | Windows / llvm |

每条腿冷构建约 2700 个目标文件;`~/.mcpp/registry` 缓存跨运行保留工具链与已构建的
`compat.ffmpeg`。测试为 6 个可执行文件,覆盖导出面完整性(`api_surface_test`)、
读写往返含 FFmpeg mp4(`roundtrip_test`)、模块与头文件混合 TU(`full_mix_test`、
`macros_mix_test`)、以及两项特性接口(`dnn_module_test`、`unifont_module_test`)。

示例工程不在 CI 中执行:每个示例是 path 依赖消费者,会在自身 `target/` 中冷构建整套库
(4 核 runner 约 40 分钟),而其覆盖面已被 `mcpp test` 包含。

## 5. 上游升级流程

1. `tools/vendor/import_opencv.sh` 导入新版本 tarball(脚本自行下载并校验,不依赖宿主机环境)。
2. 重新生成 `gen/` 快照,核对跨平台差异与冲突。
3. `python3 tools/gen_exports.py` 重生成导出面,`python3 tools/prune_loop.py` 收敛至构建通过。
4. 五条 CI 腿全绿后发布 Release;包索引侧的薄 `opencv.lua` 更新版本指向。

## 6. 已知的临时结构

| 项 | 移除条件 |
|---|---|
| `gen/windows/tu/w*/` 的 703 个桩文件与 32 条 glob 条目 | mcpp#258(`[target.'cfg(os)'.build].flags`)落地 |
| 构建期 23 条 dead-glob 警告 | 同上 |
| llvm CI 腿先安装 gcc 的变通 | mcpp#259(`toolchain install llvm` 未拉取其 glibc 运行时)修复 |
| `src/matx_ops.inc` 的整类型推导写法 | clang BMI 回归修复后可考虑回退,但当前形式本身无害 |
