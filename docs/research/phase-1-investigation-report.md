# 阶段 1：仓库和环境调查报告

## 结论摘要

阶段 1 已确认目标二进制、S4ModApi 2.x 公共 ABI、PDB 可用程度和
Settlers United 的静态接入方式。没有安装 Hook、注入 DLL 或部署 ASI。

最重要的结论：

- 全部目标二进制是 PE32/i386；游戏文件版本为 2.50.1516.0，S4ModApi
  为 2.3.2.0。
- Settlers United、Logger、HD Patch 和 WarriorsLib 静态导入
  `S4ModApi.dll!_S4CreateInterface@8`。
- S4ModApi 可观察页面、GUI 元素绘制、地图初始化和鼠标输入，也可在主
  菜单创建自定义 UI。
- S4ModApi 没有专用胜利/结束事件，也没有当前地图文件、多人、随机地图
  或战役按钮枚举的直接接口。
- PDB 公开符号包含胜利页面、地图文件、随机地图、多人类型和各战役页面
  的候选函数，且可取得 CodeView section:offset；这些函数的语义尚未经过
  运行时验证。
- 游戏在主菜单连续观察 120 秒仍存活。当前外部采集进程即使按用户反馈
  以管理员方式运行，`OpenProcess(PROCESS_QUERY_INFORMATION |
  PROCESS_VM_READ)` 仍返回错误 5，因此完整动态模块表为 `Unknown`。

## 证据与版本身份

| 输入 | 已确认结果 | 证据 |
|---|---|---|
| `S4_Main.exe` | PE32/i386；2.50.1516.0；SHA-256 `3b561269…d3816` | `research/evidence/manifest.sha256`, `pe-metadata.txt` |
| `S4_MainR.pdb` | MSVC PDB 7；SHA-256 `702df42e…ae6` | `manifest.sha256`, `pdb-symbols.txt` |
| `S4ModApi.dll` | PE32/i386；2.3.2.0；SHA-256 `ed111491…26e8` | `manifest.sha256`, `pe-metadata.txt` |
| S4ModApi 源码 | `nyfrk/S4ModApi` 提交 `bb5d40ead1bb2a111ec0f787a6da5c15e130684e`，标签 `v2.3` | `s4modapi-source.txt` |
| Settlers United ASI | PE32；静态导入 S4ModApi | `pe-metadata.txt` |

完整哈希而非缩写值是兼容性判断依据。任何 EXE/PDB 哈希变化都必须让
PDB 候选探针安全禁用并重新调查。

## S4ModApi capabilities

| 调查问题 | 结果 | 可信度 | 证据 | 阶段 2 动作 |
|---|---|---|---|---|
| 能否在主菜单/战役页面创建 UI？ | `CreateCustomUiElement` 接受 `S4_GUI_ENUM`；官方 HelloWorld 示例使用 `S4_SCREEN_MAINMENU`。所有战役页面是否实际可绘制仍需验证。 | 主菜单 `Confirmed`；全部战役页面 `Candidate` | `s4modapi-source.txt` 中声明与 HelloWorld 示例 | 诊断版不绘制，只记录各页面 UI-frame 回调。 |
| 自定义 UI 是否仅限游戏地图内？ | 不是；官方示例明确在主菜单创建 UI。 | `Confirmed` | HelloWorld `S4_SCREEN_MAINMENU` | 后续只在已验证页面启用覆盖层。 |
| 是否存在胜利或游戏结束监听器？ | 公共头文件没有 `OnVictory` 或 `OnGameEnd`。`GameDefaultGameEndCheck` 是脚本调用，不是事件订阅。 | 公共 API 缺失为 `Confirmed` | 六项完整头文件 absence check | 诊断版只观察 PDB 候选，不自动记录。 |
| 能否通过公共 API 读取当前地图文件？ | 没有 `GetCurrentMapName` 或等价地图路径 getter。 | `Confirmed` 缺失 | absence check | 关联 `AddMapInitListener` 与 `CMapFile` 候选。 |
| 能否直接读取多人、随机地图和本地胜者？ | 没有 `IsMultiplayer`、`IsRandomMap` 或本地胜者 getter。`GetLocalPlayer`、`HasPlayerLost` 只是部分信号。 | 直接接口缺失 `Confirmed`；部分信号 `Candidate` | 公共声明和 absence check | 每个字段独立记录来源与 `Unknown` 状态。 |
| 能否观察菜单控件或绘制？ | 有 UI-frame、GUI element blit、GUI clear、hovering element 和页面枚举；元素数据含矩形、关联值、效果与文字。 | 声明 `Confirmed`；具体页面行为 `Candidate` | `AddUIFrameListener`, `AddGuiElementBltListener`, `S4GuiElementBltParams` | 对各菜单输出限流控件快照。 |

可直接使用的主要接口：

- `AddUIFrameListener(LPS4FRAMECALLBACK, S4_GUI_ENUM)`
- `AddMapInitListener(LPS4MAPINITCALLBACK)`
- `AddMouseListener(LPS4MOUSECALLBACK)`
- `AddTickListener(LPS4TICKCALLBACK)`
- `AddGuiBltListener(LPS4GUIBLTCALLBACK)`
- `AddGuiElementBltListener(LPS4GUIDRAWCALLBACK)`
- `AddGuiClearListener(LPS4GUICLEARCALLBACK)`
- `GetHoveringUiElement(LPS4UIELEMENT)`
- `IsCurrentlyOnScreen(S4_GUI_ENUM)`
- `CreateCustomUiElement(LPCS4CUSTOMUIELEMENT)`
- `GetLocalPlayer()`、`HasPlayerLost(DWORD)` 和
  `GameDefaultGameEndCheck()`，但后三项不能单独证明胜利。

## PDB candidate probes

LLVM 14.0.6 能读取 Publics、Globals、模块、节头和节贡献。本 PDB 的 TPI
类型流显示零条可用类型记录，因此不能从 PDB 自动恢复完整类布局。地址以
CodeView `section:offset` 保存，必须结合已提交节头转换为本哈希版本的 RVA。

| 需求 | 候选符号 | PDB 地址 | 可能用途 | 必须怎样确认 | 误读风险 / Hook 所有权 |
|---|---|---|---|---|---|
| 胜利候选 | `CStateVictoryScreen::sub_127FE0` | `0001:1208288` | 胜利页面状态进入或处理 | 对胜利、失败、退出分别记录调用与对象状态 | 页面出现不等于本地玩家正式获胜；S4ModApi 无同名 Hook。 |
| 胜利候选 | `CStateVictoryScreen::sub_128180`, `sub_1281C0` | `0001:1208704`, `0001:1208768` | 胜利状态生命周期辅助信号 | 与 `sub_127FE0` 调用顺序及页面枚举交叉验证 | 可能只是绘制/析构；不得作为唯一判据。 |
| 随机地图参数 | `CStateMDRandomMapParameters::sub_125F80`, `sub_1265A0`, `sub_126F70` | `0001:1200000`, `0001:1201568`, `0001:1204080` | 随机地图菜单状态或参数提交 | 固定地图与随机地图菜单路径逐项对比 | 菜单状态不保证当前已加载地图来源。 |
| 随机地图核心 | `CRandomMaps::sub_10B0A0` 至 `sub_10BBC0` | `0001:1089696` 起 | 随机生成器生命周期/模式来源 | 只在随机生成时出现且固定地图不出现 | 函数数量多且名称未知；阶段 2 不批量 Hook。 |
| 多人类型 | `CStateLobbyMultiplayerType::sub_121B50`, `sub_121C10`, `sub_121F20` | `0001:1182544`, `0001:1182736`, `0001:1183520` | 多人大厅类型选择 | 单人、多人大厅与实际进图三阶段对比 | 大厅类型不是运行中会话模式；不能单独排除多人。 |
| 地图身份 | `CMapFile@S4` RTTI/vtable 公开符号 | vtable `0002:1149592`；RTTI 多处 | 定位地图文件对象类型 | 在 `AddMapInitListener` 前后读取经结构不变量验证的路径 | 当前只有 RTTI，没有字段布局；禁止直接猜成员偏移。 |
| Dark Tribe 页面 | `CStateCampaignDark::sub_111540`, `sub_1116F0`, `sub_1117F0` | `0001:1115456`, `0001:1115888`, `0001:1116144` | 页面匹配或任务控件创建候选 | 进入/离开页面并与 S4ModApi GUI 快照关联 | 优先使用 S4ModApi，避免直接 Hook 页面函数。 |
| 主菜单 | `CStateMainMenu::sub_1224B0`, `sub_122B70`, `sub_122EF0` | `0001:1184944`, `0001:1186672`, `0001:1187568` | 页面状态基准 | 与 `S4_SCREEN_MAINMENU` 同步验证 | S4ModApi 已提供页面回调，不应先 Hook。 |

PDB 中还确认了 AO、Mission CD、New World、Trojans、Roman、Mayan、Viking
等多类页面状态符号。阶段 2 应先记录 S4ModApi 页面和 GUI 事件；只有明确
缺失的数据才选择单个内部探针。

## Loader observations

静态加载证据：

- `SettlersUnited.asi`、`SettlersUnitedLogger.asi`、`S4HD-Patch.asi` 和
  `S4WarriorsLib.asi` 均导入 `S4ModApi.dll!_S4CreateInterface@8`。
- `ExtraZoom.asi` 不静态导入 S4ModApi；它导入 `GetProcAddress` 和
  `LoadLibraryExW`，其运行时行为不能由导入表单独确定。
- `ddraw.dll`、所有现有 ASI、游戏 EXE 和 S4ModApi 均为 i386。

运行时观察：

- 用户通过 Settlers United 启动游戏并停留主菜单。
- `S4_Main.exe` PID 36956 被连续轮询 12 次，每次间隔 10 秒，120 秒内未
  退出。这只证明未加载本插件时的基线主菜单稳定性。
- 外部采集能看到 PID 和父 PID，但无法读取路径、命令行或模块。
- 只读 Win32 `OpenProcess(0x410)` 返回 `win32_error=5`；因此
  `module_count=0` 表示访问受限，不表示任何模块缺失。
- 游戏自身在本次启动中更新了 Video、WarningTypes、Hotkeys、HD Patch
  和 Sentry 文件。这些是已获允许的应用自身正常写入，本项目没有改写它们。

完整结果见 `research/evidence/loader-state.txt`。阶段 2 的最小诊断 ASI 必须
首先从进程内部调用 `GetModuleHandle`/模块快照并记录加载链；在完成这一步
前不安装游戏内部 Hook。

## 风险与安全决策

- **版本绑定：** 所有 PDB 地址仅适用于当前 EXE/PDB 哈希。
- **符号语义：** `sub_` 名称只证明函数归属候选类，不证明具体行为。
- **重复 Hook：** S4ModApi 已管理 GUI、地图初始化、帧和输入 Hook；插件
  必须优先注册监听器，不能重复 Hook 对应入口。
- **胜利误判：** 胜利页面只能作为候选信号，必须另行确认本地玩家、正式
  结束、非多人、非随机和稳定地图身份。
- **随机地图误判：** 菜单入口和随机生成器活动都不能独立证明当前会话来源。
- **加载链未知：** 外部进程访问限制要求阶段 2 先做进程内模块清单，禁止
  直接进入 Hook 实验。

## Phase 2 acceptance criteria

`CampaignCompletionDebug.asi` 必须满足以下条件，且不写
`completed_maps.json`、不绘制勾选：

1. 生成 Win32/PE32 ASI，并在 `DllMain` 之外延迟初始化。
2. 记录 EXE、PDB（若读取）和 S4ModApi 的完整版本与 SHA-256。
3. 首先记录 `GetModuleHandle`/模块快照结果，确认 Settlers United、Logger、
   HD Patch、WarriorsLib、ExtraZoom 和 S4ModApi 的实际加载状态。
4. 模块清单完成前不安装内部 Hook；检测到未知版本时安全禁用。
5. 通过 S4ModApi 记录所有目标页面切换和限流 GUI 元素快照。
6. 记录地图会话开始、地图身份候选及其来源/可信度。
7. 记录本地玩家、游戏结束、胜利、多人和随机地图候选；未知值必须明确
   输出 `Unknown`。
8. 分别测试固定单人、失败、主动退出、随机地图、多人和载入存档。
9. 任何 PDB 候选探针必须单独开关、逐个验证并带签名检查。
10. 初始化或探针失败时撤销本插件已注册内容，游戏继续运行。

## 阶段 1 门禁决定

结论：**NO-GO for internal hooks；GO for a hook-free phase 2 bootstrap**。

原因：公共 ABI、文件身份和 PDB 候选均已取得，但外部完整动态加载链仍为
`Unknown`，不满足安装内部 Hook 的门槛。最小安全下一步是单独规划一个
无内部 Hook 的诊断启动版本，只验证 ASI 加载、延迟初始化、进程内模块清单
和 S4ModApi 监听器注册。它成功后才能为单个胜利/模式探针制定后续计划。
