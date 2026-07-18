# mcpp 0.0.97 适配方案:包瘦身 + 仓库完整性 + index CI 机制升级

日期:2026-07-18 · 背景:mcpp v0.0.97 一次性落地了本项目提的全部诉求——
R6 语法(`[indices] default = { path = ... }`,亦接受 `""` 键)+ 修复
#226/#229/#232/#233/#234/#235 + 增强 #227/#228 + `mcpp run -p` + 扫描性能(#225)+
workspace 根配置继承(#224)。0.0.96 已修 #230(windows)。
本文是据此的具体执行方案,覆盖三条线:index CI 机制、包/描述符瘦身、仓库功能完整性(R 系列)。

**用户决策(2026-07-18 定稿,覆盖原文对应开放点):**
1. **compat.opencv 只保留一份**:老 install() 形态删除,`compat.opencv` 这个名字
   直接采用 opencv5 源码直编形态(即现 compat.opencv5 的内容迁名);
   compat.opencv5 经过渡期后删除。
2. **compat.ffmpeg 与 compat.opencv 的简洁性优化都做**(B4 不再"挂起")。
3. **ffmpeg-m / opencv-m 走全功能 + feature 机制**:opencv-m 含 R3 视频、
   R4 unifont、R5 dnn(feature 门控);ffmpeg-m 落地此前预留的 features
   机制(profile 变体)。
4. **mcpp-index 只保留 workspace 测试**:imgui/ffmpeg/opencv/tinyhttps 等
   模块专属 CI job 与 smoke shell 全部删除,统一转 workspace 成员;
   README/贡献文档重写。

## 现状盘点(0.0.97 后变成技术债的东西)

| 位置 | 现状 | 对应修复 |
|---|---|---|
| index validate.yml | `MCPP_VERSION: 0.0.96`(3 处 matrix 同步) | 升 0.0.97 |
| index validate.yml | `mcpp index update` 前置步骤(nasm 自举 #232 workaround) | #232 已修,删 |
| index tests/ | 4 个 smoke shell(tinyhttps/imgui/ffmpeg/opencv)+ 3 个专属 CI job | R6 语法已落地,全部转 workspace 成员 |
| compat.opencv5 | 457 TU 全量走 gen/tu/ 转发 stub 层(为 #233 对象路径冲突 + #234 空格 defines) | 两者已修,stub 层可缩到只剩 jpeg12/16 |
| compat.opencv5 build.mcpp | prelude 机制(#234 workaround)+ 全量 stub 合成 | 可删大半 |
| ffmpeg-m ci.yml | apt 装 nasm(#232 workaround) | 删,交回 mcpp 自举 |
| ffmpeg-m .xlings.json | mcpp 0.0.95 | 升 0.0.97 |
| opencv-m .xlings.json | mcpp 0.0.96 | 升 0.0.97 |
| opencv-m 开发流程 | regen 后需 `touch src/*.cppm`(#235 stale build) | #235 已修,流程说明删掉 |
| macOS profile | 被 #229 挡住而列为"不做" | #229 已修,阻塞消除(仍按用户指示暂缓,但已可随时启动) |

## V0 实证 spike(先行,半天)

所有重构动手前,用本项目已有的 repro 逐条验证 0.0.97 的修复真实生效
(此前每个 mcpp 版本都实证过再依赖,不破例):

1. `[indices] default = { path = ... }`:玩具默认命名空间包 + workspace 成员,确认
   `mcpp test --workspace` 从本地 checkout 解析裸名依赖;同时验 `""` 键拼写和
   #224 的根级 `[indices]` 相对路径按 workspace 根解析。
2. #233:已有 2 文件 repro(不同目录同名源)直接跑。
3. #234:`defines = ["T=long long"]` 直传,不再需要 prelude。
4. #235:改 gen_exports/*.inc 后 `mcpp build` 必须重编(不 touch)。
5. #232:临时 MCPP_HOME 冷环境编 .asm(无 apt nasm、无 `index update`)。
6. #229:玩具依赖包带 `[target.'cfg(linux)'.build].sources` 被消费时展开。
7. #227/#228:`[[build.flags]]` 与 `{a,b}` glob 各一个最小用例。

任何一条不符 → 回报 mcpp issue,该项对应的重构搁置,其余照做。

## Track A:mcpp-index CI 机制升级(R6 落地)——一个 PR

**A1 工具链与 workaround 清理**
- `MCPP_VERSION` 0.0.96 → 0.0.97(env + 3 处 matrix 注释同步)。
- 删 "Refresh sandbox package index (nasm bootstrap, mcpp#232)" 步骤及注释。
- 版本注释块更新(0.0.97 动机:R6 语法 + #232/#233/#234/#235 修复)。

**A2 smoke shell → workspace 成员(核心)**
- 新增 4 个普通成员:`tests/examples/{tinyhttps,imgui,ffmpeg,opencv}-module/`,
  每个 = 最小消费工程(main.cpp 沿用各 smoke 里的消费源码 + 断言)+
  `[dependencies] <裸名> = "<已发布版本>"`。
- 索引重定向:利用 #224,在 **workspace 根** mcpp.toml 声明一次
  `[indices] default = { path = "." }`(按 workspace 根解析),成员不再各写
  `../../..`;既有 compat 成员的 `compat = { path = ... }` 一并上收(单独 commit,
  行为等价验证)。
- 平台门控:ffmpeg/opencv 成员依赖走 `[target.'cfg(linux)'.dependencies]`
  (与 tests/examples/opencv5 同模式)——macos/windows workspace 腿自然跳过。
- 删除:`tests/smoke_{tinyhttps,imgui,ffmpeg,opencv}_module.sh`(4 个)、
  validate.yml 的 `imgui-module`/`ffmpeg-module`/`opencv-module` 三个专属 job、
  workspace job 里的 tinyhttps smoke 步骤。
- 与选择性成员测试机制天然契合:PR 只动某包时只跑其成员(此前 smoke job 靠
  paths 过滤粗粒度触发);nightly 全量回归照旧兜底。
- 时间账:linux workspace 现 ~19m,吸收 4 成员后预计 ~30m(ffmpeg 6m + opencv
  5m + imgui 2m 是 smoke 全新环境的数,成员共享 workspace 缓存应更短);
  若实测超 35m,备选 = workspace job 内按成员分片(`mcpp test -p` 列表并行 matrix)。
- 语义差异说明(接受):smoke 曾额外覆盖"物理重播默认索引"机制本身;成员形态
  覆盖的是我们真正关心的**合并前描述符正确性**,且用的仍是真实已发布 tarball
  (Form A 描述符里的下载源不变)。

**A3 收尾**
- README「贡献一个模块包」段落更新:删 smoke 脚本要求,改为"加一个
  tests/examples/<name>-module 成员"。
- 老注释("默认命名空间无法本地重定向,故走 smoke")全文清除。

## Track B:包/描述符瘦身

**B0 compat.opencv 合一(用户决策 1)**
- 迁名:`tools/compat-opencv5/` → `tools/compat-opencv/`,生成目标改
  `pkgs/c/compat.opencv.lua`(源码直编形态,版本键 5.0.0);老 install() 形态
  的 compat.opencv.lua 内容删除。
- 过渡期:`compat.opencv5` 描述符暂留(已发布 opencv 0.0.2 tarball 的
  mcpp.toml 依赖它)→ opencv-m v0.0.3 把依赖改为 `compat.opencv = "5.0.0"`
  并发版、index 升 opencv 0.0.3 后,删 compat.opencv5.lua。
- 老 4.x install() 版本条目移除属破坏性变更:index 尚年轻 + 用户反馈老形态
  功能残缺(视频关闭),按决策接受;README 迁移说明一句话。

**B1 compat.opencv v2 重生成(与 R3 合并为同一次 regen,只重跑一次参考构建)**
- de-stub:#233/#234 已修 →
  - sources 直接列真实路径(`modules/{core,imgproc,...}/src/**` + ISA 后缀 glob
    照旧),457 个转发 stub 缩到只剩 **jpeg12/jpeg16**(同一 .c 按
    BITS_IN_JSAMPLE=12/16 重复编译是构建语义,mcpp 一个源只能出现一次,
    stub 仍是正解;8-bit 走真实源)。
  - 空格 defines 直接进 flags(#234 修复带 shell 引号保护),删 prelude 机制。
  - build_mcpp_template.cpp 删 stub 合成 + prelude 段(保留 blob2hdr/cl2cpp/
    jpeg12/16 stub);tu_manifest.txt 缩到 jpeg12/16 行。
  - flags 声明用 #228 花括号 glob 压缩(`modules/{core,imgproc}/...`),
    描述符体积预计 111KB → 明显下降。
- R3 合流:同一次 regen 打开 `WITH_FFMPEG=ON`(compat.ffmpeg 8.1.2 本地装成
  CMake 可发现形态 → cvconfig.h 带 HAVE_FFMPEG,cap_ffmpeg*.cpp 进源列表),
  描述符 mcpp 段加 `dependencies = { ["compat.ffmpeg"] = "8.1.2" }`
  (compat 依赖 compat 首例,V0 后补一条玩具实证;不支持则提 issue + 临时退路
  = consumer 双声明)。
- 版本策略:descriptor 版本键跟上游走(仍 5.0.0),内容为修订;对既有消费者
  (opencv 0.0.2 模块包)透明——头文件面不变,gen_descriptor 重生成后跑
  opencv-module 成员即回归。
- 验收:重生成描述符 → 本地 `mcpp test --workspace`(新成员形态)+ 双包联动
  smoke(ffmpeg 编码 mp4 → opencv VideoCapture 逐帧断言,做成
  `tests/examples/video-bridge` 成员,cfg(linux))。

**B2 ffmpeg-m 仓**
- `.xlings.json` mcpp 0.0.95 → 0.0.97;ci.yml 删 apt nasm 步骤(V0 第 5 条过了再删)。
- 检查 gen 工具里 #226(-iquote)时代的换写是否可回退为自然写法(低优先)。
- 无包内容变化 → 不发新版本,CI-only PR。

**B3 opencv-m 仓**
- `.xlings.json` 0.0.96 → 0.0.97;文档中 `touch src/*.cppm` 流程说明删除(#235)。
- 随 R1+R2 一起进 v0.0.3(见 Track C),不单独发版。

**B4 compat.ffmpeg 描述符瘦身**(用户决策 2,必做)
- gen_descriptor.py 输出改用 #228 花括号 glob 压缩源/flags 声明、#234 修复后
  的自然 defines 写法;byte-diff 审查后 regen 提交(版本键 8.1.2 不变)。
- 与 ffmpeg-m features 机制(Track C)联动的 profile 分段若引入,一并进此次 regen。

## Track C:仓库功能完整性(R 系列,顺序按 0.0.97 微调)

- **R1+R2**(opencv-m v0.0.3):Matx/Vec 算子替换层 + 混用 TU 歧义"更不特化模板"
  方案,内容照 2026-07-18-roadmap-r-series-plan.md,不受 0.0.97 影响,可立即做。
  发版后 index 升 `opencv = "0.0.3"`(此时已是成员形态,改一行版本号 + CI 即回归)。
- **R3**:并入 B1(同一次 regen),见上。opencv-m 加 examples/video_frames。
- **R4 unifont**:照原方案(字体独立 xpm 包 + feature 门控 blob2hdr);
  feature-conditional dependency 支持度在 V0 后顺带实证。
- **R5 dnn**:照原方案,最后做(feature "dnn" 门控)。
- **ffmpeg-m features 机制**(用户决策 3 新增):落地此前预留的 profile 变体——
  默认 full 不变;加 feature 门控的裁剪档(如 `decode-min`:F0 已验证的最小
  解码 profile 源列表 + config 快照分支),`[features]` 门 sources/defines;
  产出 ffmpeg-m v0.0.2 + compat.ffmpeg regen 的 feature 分段。具体分档在
  动手时按 F0 数据定(decode-min 已有现成源列表)。
- **macOS profile**:#229 修复后唯一硬阻塞消除;仍按用户指示暂缓,但列为
  "已解锁,随时可启动"(启动时 = macOS 参考构建快照 + cfg 分支选择)。

## 执行顺序与 PR 切分

1. **V0** spike(本地,无 PR)。
2. **PR-A**(mcpp-index):Track A 全部——0.0.97 + 删 workaround + 成员转换
   + 模块专属 job 全删(只留 workspace/lint/mirror)+ README 重写。
   这是后续一切的验证基座(R6 端到端首用)。
3. **PR-B2**(ffmpeg-m)+ **opencv-m 工具链 bump**:小 PR,可与 2 并行。
4. **PR-B0/B1/R3**(mcpp-index):compat.opencv 合一 + v2 regen(de-stub +
   FFmpeg 后端 + 花括号压缩)+ video-bridge 联动成员;同批 **B4** compat.ffmpeg
   瘦身 regen。
5. **PR-C1**(opencv-m):R1+R2 + 依赖切到 `compat.opencv` → v0.0.3;
   随后 index PR 升 opencv 0.0.3 + 删 compat.opencv5。
6. **R4 unifont** → **ffmpeg-m features(v0.0.2)+ compat.ffmpeg feature 分段**
   → **R5 dnn**。

(4 与 5 有交叉:B0 的过渡期设计允许先落 compat.opencv 新描述符、
compat.opencv5 暂留,再由 5 完成消费端切换后删除。)

风险集中点:A2 的 workspace 时长(备选分片)、B1 的 compat-依赖-compat 支持度
(V0 实证)、R2 的 gcc16 重载排序实证(原方案已有退路)、B0 删老 compat.opencv
的破坏性(用户已拍板接受)。
