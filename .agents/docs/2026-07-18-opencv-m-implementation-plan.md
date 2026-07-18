# opencv-m 实施计划(O 系列)——基于 O0 POC 实测结论

日期:2026-07-18 · 前置:ffmpeg 链路全部落地(compat.ffmpeg + ffmpeg 模块包均已进 index,v0.0.1 released)
上承:`2026-07-17-opencv-m-research-and-plan.md`(P 系列规划),本文按 ffmpeg 实战确立的
compat-split 架构修订并给出实测依据。

## 对原规划的三处架构修订

1. **不 vendor**(用户 ffmpeg review 决定的推广):OpenCV 5.0.0 源码经 mcpp-index 新包
   `compat.opencv5`(官方 tarball + CN 镜像,全源码直编形态)到达消费者;opencv-m 仓库只是
   模块层(ffmpeg-m 同款终态)。
2. **SIMD runtime dispatch 保留**(原计划"固定 baseline"作废):mcpp 0.0.95 per-glob flags
   实测可精确表达 OpenCV 的 per-ISA 编译面(见 O0)。
3. **构建期生成物不全量内联**:17MB 生成物(字体 hex 头 + OpenCL kernel 内嵌 + 转发 stub)
   由 **build.mcpp 消费端合成**(0.0.95 已支持依赖包 build.mcpp 执行);描述符只内联小体量
   config 快照。

## O0 POC(2026-07-18,已全绿,scratchpad `spike3/` + `gen_spike3.py`)

范围:`BUILD_LIST=core,imgproc,imgcodecs,highgui,videoio`(闭包再拉入 flann、geometry),
hermetic profile:外部探测全 OFF、内置 3rdparty 全 ON(zlib、libpng、libjpeg-turbo 含 27 个
NASM `.asm`)、headless highgui、V4L2 videoio、`WITH_UNIFONT=OFF`。

结果:**mcpp 0.0.95 + gcc16 直编 457 TU,冷构建 ~73s/32 核,消费程序全绿**
(getVersionString=5.0.0、resize/cvtColor、PNG+JPEG 编解码 roundtrip、videoio 8 后端、
AVX2 dispatch 生效)。

方法(全部脚本化于 gen_spike3.py,是 O1 描述符生成器的内核):

- 参考构建:`cmake -G Ninja`(一次,维护期)→ **`ninja -t commands`** 取全量编译命令
  (⚠️ 不能用 `ninja -n`:构建过的目录只列过期目标)。需真跑一次 `ninja`(-k 容错)让
  build 期生成器(cl2cpp、blob2hdr)落盘。
- flag 面分解:每命令 flags = 基础 ∪ 目录组(9 组:7 模块 + zlib/png/jpeg×3档)∪ ISA 后缀组
  (`**/*.{sse4_1,sse4_2,avx,avx2,avx512_skx}.cpp`)∪ 逐文件特例(alloc/system/parallel 3 个)。
  生成器做**精确重构校验**(重组差集必须为空)。
- 生成物快照:内嵌绝对路径改写为项目相对(`-I ocv`/`-I gen` 兜底 quoted include)。
- **转发 stub 层**(一石三鸟):每个 C/C++ TU 经 `gen/tu/<组>/<路径__mangle>` 唯一名 stub
  `#include` 原文件 —— ① 绕开 mcpp#233(对象路径按父目录名折叠,`modules/*/src/` 同名必撞);
  ② 吸收 libjpeg-turbo 同源三编(BITS_IN_JSAMPLE=8/12/16,43 个重复 TU);
  ③ 承载含空格 define(mcpp#234,`OPENCV_ALLOCATOR_STATS_COUNTER_TYPE=long long` 改注入
  stub 顶部 `#define`)。
- 宿主 gcc13 编 arithm.avx2(bfloat)会 ICE——仅参考构建受影响(`-k` 跳过),gcc16 无恙。

生成物体量账:fonts(sans+italic)1.7MB + opencl_kernels ~0.6MB + tu-stubs 1.8MB +
config 快照残余 <1MB。前三类都是官方 tarball 内文件的机械变换 → build.mcpp 合成;
唯 unifont(13MB CJK 字体)是 configure 期**额外下载**,v0.1 关闭,后续做 feature。

## 阶段划分

| 阶段 | 交付物 | 验证 |
|---|---|---|
| O1 compat.opencv5 | mcpp-index `pkgs/c/compat.opencv5.lua` + `tools/compat-opencv5/`(fetch/gen_config/gen_descriptor,gen_spike3.py 演化)+ 包内 build.mcpp(blob2hdr/cl2cpp/tu-stub 合成)+ CN 镜像(gtc,官方 tarball 逐字节)| 消费工程零 override 直编运行;workspace 三平台 CI |
| O2 模块层 | opencv-m 仓库:`src/{core,imgproc,imgcodecs,highgui,videoio,flann,geometry}.cppm` + 汇总 `opencv.cv` + lib-root `opencv`;`tools/gen_exports.py`(hdr_parser.py 驱动 + 人工白名单段);`include/opencv-m/macros.hpp`;gtest tests;examples;template;CI(ffmpeg-m 克隆)| `import opencv.cv;` 消费程序;mcpp test 绿;PR + linux CI 绿 |
| O3 发布登记 | opencv-m v0.0.1 tag + release CI;mcpp-index `pkgs/o/opencv.lua`(Form A,linux-only)+ smoke + CN 镜像 | index PR 全绿合并;`[dependencies] opencv="0.0.1"` 即用 |

模块命名(研究文档决策点 1,按推荐执行):per-module `opencv.core` 等 + 汇总 `opencv.cv` +
lib-root `opencv`(`export import opencv.cv;`)。宏策略(决策点 3):常量宏 → 模块内
`inline constexpr` 原名;`CV_Assert` 族 → macros.hpp 伴随头(import 前 include)。

## 风险登记

- build.mcpp 依赖执行是 0.0.95 新面(G 清单刚落地),O1 首个真实用户;若有坑 → 降级路线:
  生成物打进 CN/GLOBAL 双镜像的补充 tarball(损失"官方单源",记录再议)。
- gcc16 modules 吞 `opencv2/opencv.hpp` 全家桶的 BMI/导出面规模是 O2 主风险(P 系列已列);
  分模块渐进 + 每步实编验证。
- mcpp#233/#234 修复后可去 stub 层(保留生成器开关)。
