# opencv-m v0.0.1 实现记录(O2 模块层)

日期:2026-07-18 · 前置:compat.opencv5 已提 mcpp-index PR #76(O1,全源码直编 +
build.mcpp 消费端合成形态);O0/O1 记录见 2026-07-18-opencv-m-implementation-plan.md
与 mcpp-index .agents/docs/2026-07-18-add-compat-opencv5-plan.md。

## 结构

- 8 个模块接口:`opencv.{core,imgproc,imgcodecs,videoio,highgui,flann,geometry}`
  + 汇总 `opencv.cv`(export import 全部)+ lib-root `opencv`。
- 导出面:`tools/gen_exports.py` 复用 OpenCV 自带 `modules/python/src2/hdr_parser.py`
  (preprocessor_definitions 需 CV_VERSION_MAJOR/MINOR + OPENCV_ABI_COMPATIBILITY=500)
  扫全部公共 *.hpp,叠加 `tools/curated/<mod>.txt` 人工白名单(模板类型族
  Mat_/Point2i/Vec3b/Matx33f、InputArray 家族等 wrapper 视角外的名字),再经
  `tools/prune_loop.py` 编译验证自动剪枝(`tools/curated/<mod>.prune.txt`,
  原因入注释)。最终 ~2000 名字,skip 报告在 src/gen_exports/*.skipped.txt。
- 常量宏:cv:: constexpr 原名重安置(`cv::CV_8UC3`、`cv::CV_PI`、
  `cv::CV_MAKETYPE()`),实现用「捕获值 → #undef → 再定义」三步(注意
  **CV_MAKETYPE 也要 #undef**,否则函数式宏吃掉 constexpr 函数定义,
  整 TU 崩且剪枝循环级联误伤 600+ 名字)。
- 全局作用域名(enum CpuFeatures/CPU_AVX2 —— cvdef.h 里 cv 命名空间之外):
  ffmpeg 式 `export using ::name;` 手工段。
- 宏伴随头 `include/opencv-m/macros.hpp`(CV_Assert 族、原始类型宏、版本宏),
  **必须在 import 之前 include**。混用 TU 里用裸宏拼写(活动宏会碾碎
  `cv::CV_8UC3` 拼写——见 tests/macros_mix_test.cpp)。

## 关键设计决断:模块间不互相 import

gcc16 下「文本包含 + import 合并」同一全局模块实体(带默认实参的成员函数,如
cv::SVD::compute)会报 *conflicting default argument*:依赖模块 TU 的 GMF 文本
包含 core.hpp,再 `export import opencv.core;` 就撞。**方案:各模块自包含 GMF、
只导出自家名字、互不 import;跨模块聚合只发生在 opencv.cv(纯 import 合并无冲突,
实测)。** 代价:单独 `import opencv.imgproc;` 拿不到 cv::Mat —— 文档主推
`import opencv.cv;`。

## 验证

- `mcpp build` 绿(模块层 ~2s;compat.opencv5 冷构建 33s)。
- `mcpp test` 3/3:api_surface(纯 import TU:类型/常量/imgproc/videoio)、
  macros_mix(伴随头共存)、roundtrip(PNG 逐像素 + JPEG q95 均差 < 12,
  实测锐边合成图 ≈ 6.8)。
- examples:probe(版本/SIMD/后端)、gray_pipeline(合成图 → resize →
  灰度 → imwrite PNG)。template:imgproc。

## 踩坑(过程记录)

- mcpp#233/#234(O1 已录)之外新发现 **mcpp#235**:模块 purview 内文本
  `#include`(gen_exports/*.inc 模式)改动不触发重编(stale build)——
  ffmpeg-m 同模式同样暴露;开发期改 .inc 后需 touch .cppm。
- hdr_parser 点分名歧义:小写类名(softfloat/softdouble)与嵌套类
  (ogl.Buffer.*)会被误判为子命名空间 → 两遍扫描(先收类名表)。
- `import std;` 的 TU 用 `std::println(stderr, …)` 需先 `#include <cstdio>`
  (stderr 是宏)。

## Addendum — v0.0.2:static-inline 算子替换层(核心质量修复)

v0.0.1 发布后 O3 冒烟暴露深层问题:OpenCV 把值类型的命名空间级算子模板声明为
**`static inline`(内部链接)**(types.hpp 122 个 + matx.inl.hpp 48 个 +
operations.hpp 11 个)——内部链接实体从根上过不了模块边界:导入端要么绑到不存在
的符号(链接 undefined reference,如 `Size_ operator!=`,此前全靠 compat 库对象
里恰好实例化过的 COMDAT 撑着),要么把 TU-local 实体暴露(gcc -Wexpose-…)。

修复(src/core_ops.inc,手工维护):
1. **自含重实现替换层**,置于 `cv` 的 **inline namespace opencv_m_repl**
   (与上游同名实体在模块 TU 内不再是重定义;导入端 ADL/限定查找照常命中):
   - `saturate_cast` 全家(clamp 语义,float→int 用 lrint 就近取偶 ≈ cvRound);
   - Point_/Point3_/Size_/Rect_/Range/Scalar_/Complex 的 ==/!=/± 算术/交并集。
   - **函数体必须自含**(不得调用同名算子或上游 TU-local 助手):模板体在导入端
     实例化,gcc16 会把 GMF 内部声明也计入候选 → 体内嵌套调用歧义。逐字段直算。
2. **Mat/MatExpr 64 个 CV_EXPORTS 外部算子改「函数指针捕获 + 委托」**:
   `using cv::operator+;` 会把整个重载集(含内部件)拖到导入端与替换层撞;
   改为逐签名 `static_cast<R(*)(Args)>(&cv::operator+)` 捕获(取址消歧,调用歧义
   但取址唯一)再以同名内联算子委托。curated 里的 operator using 行全部移除。

已知边界(README Notes 已记):Matx/Vec 矩阵代数尚无替换层(纯模块 TU 不可用);
混用 TU 中值类型算子两套皆可见 → 歧义,用成员/字段形式。上游根治 = OpenCV 去
static(可作为 opencv 上游提案,F 系列跟进)。
