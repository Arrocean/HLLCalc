# 构建与开发

[返回首页](<../README.md>) · [使用指南](<USAGE.zh-CN.md>) · [发布指南](<RELEASING.md>)

## 环境要求

- Windows 10/11 x64。
- Visual Studio 2022/2026 或 Build Tools，安装“使用 C++ 的桌面开发”，包含 MSVC x64 和 Windows SDK。
- CMake 3.20 或更新版本。
- PowerShell；Git 用于克隆、版本管理和发布。

应用使用 C++17、Win32、DirectX 11 与 GDI，MSVC 使用静态 CRT。Dear ImGui 等所需第三方源码已经放在仓库中，普通构建不需要额外下载依赖。

Python 仅用于可选的开发对照脚本；**不是构建或运行依赖**。不需要取得任何参考项目才能构建、运行单元测试或发布本项目。

## 推荐：构建脚本

在仓库根目录运行：

```powershell
.\scripts\build.ps1
```

默认使用 `build/cmake`，构建 Release 并执行七组非交互 CTest。输出：

- 正式构建输出：`build/cmake/bin/Release/HLLCalc.exe`。
- 便捷副本：`build/HLLCalc.exe`。

常用参数：

```powershell
.\scripts\build.ps1 -Configuration Debug
.\scripts\build.ps1 -Configuration Release -BuildDirectory build/local
```

根目录 `build.cmd` 是包装入口，可用于双击或从命令提示符调用。

## 直接使用 CMake

```powershell
cmake -S . -B build/cmake -A x64 -DBUILD_TESTING=ON
cmake --build build/cmake --config Release --parallel
ctest --test-dir build/cmake -C Release --output-on-failure
```

上述路径对应 Visual Studio 多配置生成器。Debug 输出在 `bin/Debug`；使用其他生成器时请以 CMake 的实际输出为准。

### 版本

默认基础版本为 `1.3.0`。可通过 CMake 参数设置发布版本：

```powershell
cmake -S . -B build/cmake -A x64 -DBUILD_TESTING=ON `
  -DHLLCALC_VERSION=1.3.0 -DHLLCALC_VERSION_SUFFIX=rc.1
```

`HLLCALC_VERSION` 使用三段数值版本，`HLLCALC_VERSION_SUFFIX` 用于预发布后缀；正式版留空。版本模板位于 `cmake/`，Windows 资源模板位于 `resources/`。不要分别手改多个生成后的版本文件。

## 测试

### 默认自动测试

默认构建并运行七组非交互测试：

| 组别 | 主要检查 |
| --- | --- |
| 弹道 | 四国、两模式、舍入、射程/射角、输入范围、11 行与 3 米步进 |
| CS 分享码解码 | 真实向量、编码往返、格式/版本/校验/溢出、数值边界、失败不修改输出 |
| CS 静态适配 | 新旧单位、颜色、透明度、几何转换、未知参数拒绝 |
| 全局快捷键 | 注册、冲突、持久化、取消、恢复默认和保存失败回退 |
| 非线性动画 | 缓动端点、单调性、时间推进、中途反向不跳变 |
| i18n | 中英字典键与 printf 占位符、切换持久化、未知文本回退 |
| 更新安全 | 版本比较、GitHub 元数据、校验和、ZIP 路径/尺寸/格式边界，不联网、不安装 |

快捷键测试创建自己的隐藏窗口和临时配置，但仍会短暂注册真实全局按键。测试前请退出正在运行的 HLLCalc，并避免其他程序占用测试用的键。

### 桌面交互测试：必须显式启用

桌面测试会**移动鼠标、发送键盘事件、切换前台窗口并创建程序实例**。请保存其他工作，测试期间不要操作键盘鼠标，也不要在共享桌面或普通 CI 中运行。

```powershell
.\scripts\build.ps1 -DesktopTests
```

直接 CMake 方式：

```powershell
cmake -S . -B build/cmake -A x64 -DBUILD_TESTING=ON `
  -DHLLCALC_BUILD_UI_TESTS=ON
cmake --build build/cmake --config Release --parallel
ctest --test-dir build/cmake -C Release --output-on-failure
```

`HLLCALC_BUILD_UI_TESTS` 默认关闭。GitHub Actions 应保持其关闭，仅执行默认自动测试。

桌面检查覆盖真实快捷键、输入焦点、鼠标归位/穿透、界面布局、CS 导入、图标资源、快捷键录制和冲突处理，并实际退出重启验证绑定保存。它不是实际游戏、反作弊或独占全屏兼容性测试。

### 可选参数对照

`scripts/verify_reference.py` 是开发辅助工具，可将测试程序导出的计算数据与维护者自行取得的参考输入比较。查看脚本用法后再运行；它不应成为正常构建、CI 或发布前置条件，仓库不要求存在其他项目的本机路径。

## 本地打包

先完成 Release 构建和测试，再运行：

```powershell
.\scripts\package.ps1 -BuildDirectory build/cmake `
  -Configuration Release -Version 1.3.0 -OutputDirectory dist
```

打包生成运行 ZIP、对应源码 ZIP 与 `SHA256SUMS`。运行包附主项目完整许可证、第三方声明及依赖许可文件；源码包应包含对应版本源码、构建脚本、资源和保留的第三方源码/许可。

正式 tag、预发布 tag、GitHub 权限和自动 Release 操作见 [发布指南](<RELEASING.md>)。

## 目录与修改边界

```text
src/app/           主窗口、渲染循环、界面
src/ballistics/    参数与计算
src/crosshair/     独立透明叠加层及配置
src/input/         全局快捷键
src/sharecode/     CS 码解码与静态适配
src/i18n/          嵌入式中英字典与语言偏好
src/update/        后台更新、校验与受控安装助手
tests/unit/        自动回归测试
tests/desktop/     显式启用的桌面测试
resources/         图标、资源模板和资源头
cmake/             CMake/版本模板
scripts/           构建、打包、开发辅助
third_party/       vendored 依赖及原始许可
```

构建产物放在 `build/`，发布产物放在 `dist/`；不要提交本机 IDE 状态、生成的工程文件、临时配置或二进制中间文件。修改第三方组件时保留其原始版权及许可声明，并同步 [第三方声明](<../THIRD_PARTY_NOTICES.md>)。

## 发布前自检

- 使用干净构建目录完成 Release 构建和默认 CTest。
- 必要时在独立桌面手动运行桌面测试；不要在 CI 中强开。
- 检查版本、图标及资源，不携带个人配置和凭据。
- 确认运行包和对应源码包版本一致，校验和覆盖最终 ZIP。
- 检查 [LICENSE](<../LICENSE>)、[THIRD_PARTY_NOTICES.md](<../THIRD_PARTY_NOTICES.md>) 和第三方许可文件随包保留。

本页说明构建与验证入口，不表示所有 Windows、显卡、键盘布局或游戏输入模式都已逐一验证。
