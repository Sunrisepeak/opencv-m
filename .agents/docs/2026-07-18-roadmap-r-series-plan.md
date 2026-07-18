# R 系列初步方案(v0.0.2 发布后的 review 结论)

日期:2026-07-18 · 背景:双链路(ffmpeg/opencv)已闭环;用户 review 指示:macOS profile
与 OpenCV 上游 de-static 提案**先不做**,其余全做。本文为逐项初步方案,按建议执行顺序排列。

## R1 Matx/Vec 算子替换层(消掉已知边界 1)

- 范围:matx.inl.hpp 48 个 + operations.hpp 11 个 static-inline 算子
  (Matx ==/!=、+/-、标量乘除、Matx×Matx 矩阵乘、Matx×Vec、Vec 全套)。
- 方式:core_ops.inc 同模式——自含重实现(逐元素循环照抄上游公式,不得调用
  上游 TU-local 助手);矩阵乘等语义逐条对照上游实现。
- 测试:api_surface 加 MatxOps 用例(Matx33f*Matx33f、Matx*Vec、Vec 算术、
  与 cv::Mat(Matx) 互转结果对照)。

## R2 混用 TU 歧义(彻底解决已知边界 2)

- 核心技巧:替换算子**故意做成更不特化的模板**,利用重载排序双向选边:
  - 值类型:`template<class A, class B> requires std::same_as<A,B>` 双参形式
    (上游单参模板更特化 → 混用 TU 上游胜出,用头文件里的定义,正确;
    纯 import TU 只有我们 → 命中替换层);
  - Mat/MatExpr 委托:非模板改 `template<class M> requires std::same_as<
    std::remove_cvref_t<M>, Mat>`(上游非模板 → 混用 TU 上游胜出)。
- 风险:gcc16 的偏特化排序 × 模块可见性交互需实证;若有坑,退路 = 保持现状
  + macros_mix 文档约定(成员形式)。
- 测试:macros_mix_test 补值类型算子断言(现在特意避开的场景转为覆盖)。

## R3 F2:videoio↔ffmpeg 后端打通(视频能力主线)

- compat.opencv5 换代:维护期把 compat.ffmpeg 8.1.2 本地装成 CMake 可发现形态
  (头 + 静态库 + pkg-config stub)→ `WITH_FFMPEG=ON` 重跑参考构建 →
  新快照(cvconfig.h 带 HAVE_FFMPEG,cap_ffmpeg*.cpp 进 manifest)。
- 描述符 mcpp 段加 `dependencies = { ["compat.ffmpeg"] = "8.1.2" }`
  (mcpp 段依赖表支持度需实证——首个 compat 依赖 compat 的案例;
  行不通则提 mcpp issue,临时退路:consumer 同时声明两个 dep)。
- 风险:OpenCV 5 cap_ffmpeg 对 FFmpeg 8.x API 的兼容窗口(上游 CI 矩阵确认);
  avdevice/swresample 是否被 cap_ffmpeg 引用(链接闭包)。
- 验收(双包联动 smoke):ffmpeg 模块包编码合成 mp4 → opencv `VideoCapture`
  解回逐帧断言;opencv-m 加 examples/video_frames。
- 版本策略:compat.opencv5 描述符是重生成(5.0.0 版本号不变、内容变)——
  需 bump?compat 包内容变更 = 新 descriptor 修订;与 index 维护者惯例对齐
  (ffmpeg 先例:描述符可再生成,version 键跟上游版本走)。

## R4 unifont feature(putText 中文渲染)

- 字体(WenQuanYiMicroHei.ttf.gz,13MB)是 configure 期额外下载 → 违背单源
  封闭 → 单独打 xpm 包(官方 OpenCV CDN url + mcpp-res CN 镜像,sha256 钉)。
- compat.opencv5 加 feature `unifont`:build.mcpp 见 `MCPP_FEATURE_UNIFONT=1`
  时对字体包内容跑 blob2hdr 并 `mcpp:cfg=HAVE_UNIFONT`。
- 开放点:feature-conditional dependency mcpp 是否支持;不支持则字体包常驻
  依赖(feature 只控嵌入,多一次 13MB 下载)或提 mcpp 增强。
- 需核对 HAVE_UNIFONT 在快照头里的落点(imgproc CMake set INTERNAL cache)。

## R5 dnn 可选档(最大件,最后做)

- 前置:BUILD_LIST 加 dnn 重跑参考构建(内置 3rdparty libprotobuf 直编 +
  flatbuffers header-only;TU 规模预计 +700 级)。
- 形态倾向:**同包 feature "dnn"**(mcpp features 门控 sources;manifest 加
  `!feature dnn` 分段行,build.mcpp 按 env 过滤生成),而非独立包——
  避免两个 compat.opencv5 变体的快照漂移。
- opencv-m 加 `opencv.dnn` 模块接口(hdr_parser 扫 dnn.hpp,dnn API 基本全
  CV_EXPORTS_W,生成面友好)。

## R6 smoke 驱动的根治(默认命名空间 → workspace 成员)

- 现状必要性:默认命名空间包无法作 workspace 成员指向本地 path index,
  smoke 脚本是**合并前验证的唯一通道**(否则模块包盲飞到 merge 后)。
- 根治:给 mcpp 提 feature request——workspace 成员支持
  `[indices] "" = { path = ... }`(或等价机制);落地后 imgui/ffmpeg/opencv
  三个 smoke 脚本转普通成员,删掉 shell 例外通道。

## 不做(本轮 review 明确)

- macOS/Windows profile:被 mcpp#229(依赖包 cfg 条件 sources 不展开)挡住,
  修复后再启动(届时 = 每平台一份参考构建快照 + cfg 选择)。
- OpenCV 上游 de-static 提案:暂缓。

## 附:关于"更多文件挪进 build.mcpp 生成"(review 问题 3)

- **不能挪**:cvconfig.h / cv_cpu_config.h / simd_declarations / 3rdparty 配置头
  和 tu_manifest.txt 的内容——它们是 CMake configure 探测决策的**冻结数据本身**,
  消费端无法推导(除非重跑 CMake,违背零 CMake 原则)。这正是"配置快照"形态的
  本质与代价。
- **可挪但收益小**:57 个 dispatch stub .cpp(可由 simd_declarations 推导,省
  ~10KB);当前内联总量仅 59KB(描述符 111KB,不到 compat.ffmpeg 的 1/3),
  列为低优先级美化。

## 执行顺序

R1+R2(纯 opencv-m 仓,合一个 PR,发 v0.0.3)→ R3(index+opencv-m 双仓)→
R4 → R6(issue 先行)→ R5。
