# mcpp 通用能力需求清单 — 由 ffmpeg-m / opencv-m 项目驱动

> 日期: 2026-07-17(v2,按"通用机制"视角重构)
> 来源: opencv-m 调研与实测 POC(见 `2026-07-17-opencv-m-research-and-plan.md`)
> 环境: mcpp 0.0.93, gcc@16.1.0, linux x86_64
> 原则: 每条都是**领域无关的通用机制**(媒体/加密/数值计算类库全都受益),不是为 ffmpeg/opencv 开的特例——符合 mcpp "closed syntax, open vocabulary" 的 schema 所有权原则。

## 先回答核心问题:mcpp 要支持 nasm 构建吗?

**建议:要,而且作为一等公民(G1),而不是只靠 build.mcpp 绕。** 理由:

1. **通用性**:手写汇编是整个媒体/加密/数值生态的常态——FFmpeg、dav1d、x264/x265、OpenSSL、BLAS 类库全有 `.asm`(NASM)或 `.S`(GAS)。mcpp 想承接"大型 C/C++ 库源码构建"这个定位,汇编 TU 迟早绕不开;为每个库各写一遍 build.mcpp 调 nasm 是重复劳动。
2. **声明式 > 命令式**:`.asm` 直接进 `[build].sources`,和 `.c` 一样参与指纹/增量/并行,不需要包作者管缓存与重跑逻辑。
3. **cross 天然成立**:nasm 本身就是交叉汇编器(`-f elf64/macho64/win64` 由目标三元组决定),原生支持后交叉构建不需要任何特判——而 build.mcpp 路线在 cross 下还要先解决 A2。
4. **对照**:Cargo 没有原生汇编(逼出了 cc crate 生态),Zig `build.zig` 有 `addAssemblyFile` ——mcpp 作为 C/C++ 原生工具,应该站 Zig 这边。

build.mcpp(G2/G3)仍然需要——它解决的是"探测/代码生成/条件逻辑"这类真正动态的事,和 G1 正交。

---

## G1. 原生汇编源支持(`.S` / `.asm` 进 `sources`)★核心建议

| 后缀 | 语法 | 驱动 | 备注 |
|---|---|---|---|
| `.S` | GAS(带 C 预处理) | 现有 cc 驱动即可(gcc/clang 原生认) | 覆盖 FFmpeg 全部 ARM/AArch64 汇编;实现成本≈把后缀加进 glob 并路由到 CC |
| `.asm` | NASM | nasm(经 xlings 供给,见 G7) | 覆盖 FFmpeg/x264/OpenSSL 的 x86 汇编;`-f` 输出格式由解析出的目标三元组推导;`-D` 宏从包 `cflags`/`defines` 映射 |

验收:`sources = ["libavcodec/x86/*.asm"]` 无需其他配置即编译、进指纹、增量正确、`--target` 交叉可用。

## G2. 运行依赖包的 `build.mcpp`(Cargo 语义)

**实测缺口(0.0.93)**:依赖包的 `build.mcpp` 被静默跳过(无警告)。复现:`mylib` 带 `build.mcpp` 发射 define,单独构建生效;被 `app` path-dep 消费时不执行。

指令作用域(照 Cargo):`cxxflag/cflag/cfg/generated` → 仅作用于**该包自身 TU**;`link-lib/link-search` → 传播到最终链接;`rerun-if-*` → 按包独立缓存。信任模型可加 `--no-build-scripts` 开关,默认可用。

**这是"包携带构建期逻辑"的基础设施**:没有它,build.mcpp 只对根项目有意义,任何库包都不能依赖它。

## G3. `build.mcpp` 的环境契约(cross + features + 目标信息)

- **cross 下运行**(docs/07 已标 planned):宿主工具链编译运行,不再跳过;
- **暴露构建上下文**(对应 Cargo 的 `TARGET`/`HOST`/`CARGO_FEATURE_*`/`PROFILE`/`OUT_DIR` 环境变量):目标三元组、宿主三元组、**活跃 features 集合**、profile、建议输出目录。没有 features 暴露,build.mcpp 无法按 feature 决定生成什么(例如 ffmpeg-m 按启用的 codec 集合筛 `.asm` 列表);
- 跳过时(若保留任何跳过路径)至少升级为显式警告 + 文档化默认态语义。

## G4. per-glob 编译 flags

```toml
[build.flags."third_party/opencv/**/*.avx2.cpp"]
cxxflags = ["-mavx2", "-mfma"]
[build.flags."third_party/**"]
cxxflags = ["-w"]                 # 三方代码静默, 一方代码保持告警
```

现状只有包全局与 target-entry 两级(mcpp-index opencv 调研文档亦列为 generic gap)。两大用例:**SIMD 多档 dispatch TU**(OpenCV `.opt.cpp`、任何数值库)与 **vendored 代码的告警隔离**(现在只能全包 `-w`,连自己的封装层也被静默——POC 里已亲历)。

## G5. mcpp.toml 的 `[features]` 支持 gate sources

```toml
[features]
png = { sources = ["third_party/libpng/*.c"], defines = ["HAVE_PNG"] }
```

现状:index 描述符的 features **能** gate sources(compat.gtest 实例),mcpp.toml 的 `[features]` 表单形式只有 `defines/implies/requires`——**两边不对称**。Form-A 仓库(imgui-m/ffmpeg-m/opencv-m 这类)没法按 feature 增删编译单元,而这恰是 vendored 大库最高频的需求:imgcodecs 的 17 种编解码器、highgui 的 GUI 后端、ffmpeg 的 codec 集合,全是"feature = 一组源文件 + 一个 define"。

## G6. `generated_files` 进 mcpp.toml(与索引描述符对齐,小项)

index 描述符已有 `generated_files`(compat.opengl 用它合成锚文件);mcpp.toml 无对应。快照小文件(如 `opencv_modules.hpp` 这类几行的生成头)可声明式合成,免去提交琐碎文件或依赖 build.mcpp。优先级低(checked-in 快照已可行)。

## G7. xlings 打包 `nasm`(供给设施)

版本 ≥2.16(FFmpeg 要求)。xlings 生态自包含、不依赖宿主;工具缺失 = 供给错误 → **硬失败**,不做静默降级(可复现性优先;用户确认的原则)。G1 与 build.mcpp-调-nasm 两条路线都以此为前提。

## G8. 修缮项(POC 实测发现的一致性问题)

- **G8a** `sources` glob 不跟随目录软链接(`include_dirs` 却穿透)→ 跟随,或文档明示 + "glob 经过 symlink 目录被忽略"警告;
- **G8b** `[build].cxxflags` 中相对 `-I` 对 `.cppm` 单元不生效(普通 TU 生效;换 `include_dirs` 正常)→ module TU 与普通 TU 的 flag 路径解析基准统一到项目根,或文档明确"相对 -I 未定义"。

## G9. 构建产物缓存(生态级,远期)

OpenCV 全模块 + FFmpeg 级别的 vendored 依赖冷构建分钟级;同 (源版本, 配置, 工具链指纹) 的对象/静态库跨项目共享(参考 vcpkg binary caching / sccache)。compat.opencv 的实现文档亦提过(install() 构建无 binary cache)。**注意这与"消费者侧源码构建"不矛盾**:缓存键含工具链指纹,命中即等价重编。

---

## 需求 × 项目里程碑映射

| 里程碑 | 硬依赖 | 显著受益 |
|---|---|---|
| F0 ffmpeg 纯 C POC / F1 ffmpeg-m 成包(纯 C) | —(现有能力足够) | G4(告警隔离)、G5(codec features) |
| F1+ ffmpeg-m 汇编加速 | **G7 + (G1 或 G2+G3)**;G1 路线最短 | |
| F2 opencv-m videoio 接 ffmpeg 后端 | — | G5 |
| P0–P4 opencv-m 四件套 | — | G4、G5、G8a/b(已绕开) |
| P5 OpenCV SIMD dispatch | **G4 或 (G2+G3)** | |
| cross 发布(musl/macos/win) | G1 自动覆盖汇编;若走 build.mcpp 路线则需 G3 | G9 |

**排期建议**:G7 随手做 → **G1(nasm/GAS 一等公民,本清单最高杠杆)** → G5、G4(声明式三连,vendored 大库形态就此完整)→ G2+G3(build.mcpp 补全,通用构建期逻辑)→ G8 顺手 → G6/G9 远期。

**再次强调**:F0/F1(纯 C)与 P0–P4 **零阻塞**,不必等任何 G 项;纯 C/固定 baseline 是正确性基线,G 项落地只叠性能与工程体验。
