# 第三方及来源声明

本项目发行源码使用 **GPL-3.0-only**，完整主项目许可证为 [LICENSE](<LICENSE>)。本文件记录参考来源、实际随仓库提供的组件和原始许可位置；它不将第三方作品统一改成主项目许可证，也不替代各自的许可全文。

## 1. AimerHLL v6 — 功能与经验参数参考

- 项目/作者：[AimerHLL v6 / AimerSo](https://github.com/AimerSo/AimerHLL)。
- 范围：火炮计算功能、部分经验参数、射角/射程行为，以及准星配置功能的参考。
- 核对的来源许可：GPL v3；本项目保留 GPL-3.0-only 源码标识及主项目发行许可。
- 本项目是原生 C++/Win32 工程，没有将原项目的整套 Python 程序、地图、旗帜、网页应用或图标资源作为运行依赖打包。

这里保留实际参考记录，不表示上游整套源码被直接复制，也不把数学公式、通用算法、事实参数或功能概念本身描述为该项目独占的代码。主项目选择 GPL-3.0-only 发行，不因非盈利或公开源码而删除来源记录。

相关本地模块见 [弹道计算](<src/ballistics/ballistics.h>) 与 [准星接口](<src/crosshair/crosshair.h>)。构建本项目无需安装或取得参考项目。

## 2. B Launcher — MIT

- 作者声明：`Copyright (c) 2026 cubk`。
- 范围：深色主题、侧栏/卡片界面结构等视觉参考；Dear ImGui 的 vendored 源码及 Win32 后端最初取自该参考工程。
- 未包含其 Minecraft 启动器业务、账户、下载功能或图像资源。
- 完整原始许可：[third_party/B-LAUNCHER-LICENSE](<third_party/B-LAUNCHER-LICENSE>)。

保留该 MIT 通知，不把“参考外观”夸大为复用其整个启动器实现。

## 3. Dear ImGui 1.92.4 — MIT

- 上游：[ocornut/imgui](https://github.com/ocornut/imgui/tree/v1.92.4)。
- 作者声明：`Copyright (c) 2014-2025 Omar Cornut`。
- 路径：[third_party/imgui](<third_party/imgui/>)。
- 范围：核心界面源码、所需头文件、Win32 后端和 DirectX 11 后端。DX11 后端取自上游同版本源码。
- 完整许可：[third_party/imgui/LICENSE.txt](<third_party/imgui/LICENSE.txt>)。

其源码内部还含以下独立来源声明，不能仅用 ImGui 的顶层许可覆盖或删除。

### 3.1 内嵌 STB — MIT / Public Domain 双许可

ImGui 随附的修改版 STB 头文件：

- [imstb_rectpack.h](<third_party/imgui/imstb_rectpack.h>)。
- [imstb_textedit.h](<third_party/imgui/imstb_textedit.h>)。
- [imstb_truetype.h](<third_party/imgui/imstb_truetype.h>)。

三者原文均提供 **MIT（Alternative A）或 Public Domain / Unlicense（Alternative B）** 二选一，MIT 作者声明为 `Copyright (c) 2017 Sean Barrett`。原始双许可全文保留在各文件末尾；本项目发布时同时携带 MIT 通知副本 [third_party/licenses/stb-MIT.txt](<third_party/licenses/stb-MIT.txt>)，不删除或重写原文件中的双许可选项。

### 3.2 ProggyClean 默认字体 — MIT

- 作者声明：`Copyright (c) 2004, 2005 Tristan Grimmer`。
- ImGui 的 [imgui_draw.cpp](<third_party/imgui/imgui_draw.cpp>) 内含 ProggyClean.ttf 默认字体数据及其来源声明。
- 单独保留的许可：[third_party/licenses/ProggyClean-MIT.txt](<third_party/licenses/ProggyClean-MIT.txt>)。
- 字体来源信息：[Proggy Fonts](https://www.proggyfonts.net/)。

HLLCalc 界面另从用户系统加载中文字体；系统字体不作为本仓库资产或 Release 的外置字体重新分发，也不在此改写其许可。

## 4. CS 分享码协议参考与测试 fixtures — MIT

- 来源：[AkiVer / csgo-sharecode](https://github.com/akiver/csgo-sharecode)。
- 固定参考 revision：`139b9e284756110b44a894331f285b0ddabdf6ef`。
- 作者声明：`Copyright (c) 2017-present AkiVer`。
- [格式文档](https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/doc/code_layout.md)。
- [公开测试向量](https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/src/index.test.ts)。

本项目的 [分享码解码器](<src/sharecode/cs_crosshair.cpp>) 为独立 C++17 实现，不打包或执行该项目的 TypeScript 运行库。部分公开测试 fixtures 和预期数据参考上游；[对应测试源文件](<tests/unit/cs_crosshair_tests.cpp>) 顶部保留单独的 MIT 通知，另附全文副本 [third_party/licenses/csgo-sharecode-MIT.txt](<third_party/licenses/csgo-sharecode-MIT.txt>)。

这些 fixtures 的 MIT 声明与本项目测试逻辑的 GPL-3.0-only 标识分别保留。

## 5. 图标素材

HLL Calc 图标由用户提供。原始图片为 [resources/app.png](<resources/app.png>)，多尺寸 Windows 图标为 [resources/app.ico](<resources/app.ico>)。这里只作格式/尺寸转换并嵌入程序，不声明为其他参考项目的图标，也不另行更改原作者署名或素材来源。

维护者公开发布或替换素材时，应保留适用的来源及授权记录。

## 6. 发布包中的声明

运行包应携带：

- 主项目完整 [GPLv3 许可证](<LICENSE>)。
- 本文件。
- `licenses/` 中的 Dear ImGui、B Launcher、STB、ProggyClean 和分享码 fixtures 的许可通知副本。

对应源码包应保留相同版本的项目源码、构建脚本、资源及 `third_party/` 内的原始源码/许可文件。不要删除源码中已有的独立作者、版权或许可通知。

Windows、DirectX、系统字体、MSVC 和 Windows SDK 是平台或构建环境的一部分；本文件不将这些系统/工具链组件重新许可为 GPL 或 MIT。

以上是仓库的来源和许可记录，不是对所有使用、组合或再分发情形的法律结论。新增依赖、替换素材或改变分发方式时，请重新核对其声明及打包内容。
