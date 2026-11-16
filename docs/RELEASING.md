# 构建与发布

## 自动工作流

- `.github/workflows/ci.yml`：普通分支 push、Pull Request 和手动触发时，在 `windows-2022` 上构建 x64 Debug / Release，并运行四组标准 CTest。只读仓库权限。
- `.github/workflows/release.yml`：推送 `v*` tag 时触发；先严格验证版本，再构建 Release、运行标准测试、打包，最后在独立的最小写权限作业中发布 GitHub Release。
- CI **不构建或运行交互式 desktop UI 测试**：配置固定使用 `HLLCALC_BUILD_UI_TESTS=OFF`。标准 hotkey 测试只创建隐藏测试窗口，不生成可见界面或模拟键鼠输入；测试环境需具备普通 Windows 用户会话能力。

## 发布前准备

1. 将源码、vendored `third_party`、`resources`、`licenses`、测试、文档和工作流一起提交到 GitHub。
2. 确认主许可证 `LICENSE` 保持 GPLv3，第三方 MIT 等声明在 `THIRD_PARTY_NOTICES.md` 和 `licenses/` 中完整保留。
3. 在仓库 **Settings → Actions → General** 允许 GitHub Actions 与官方 Actions 使用。组织策略也必须允许工作流申请 `contents: write`。
4. 使用 GitHub 自动提供的 `GITHUB_TOKEN` 即可；不需要创建个人访问令牌，也不要把 token 写进源码或打包文件。构建作业仅有 `contents: read`，发布作业单独申请 `contents: write`。
5. 确认 CI 通过、工作区干净，且对应源码已提交。打包会拒绝未提交修改，以免 EXE 与 `git archive HEAD` 对应源码不一致。

根项目默认版本为 `1.3.0`。Release 工作流不会依赖硬编码版本：它从 tag 提取核心版本与后缀，传给 `-DHLLCALC_VERSION=...` 和 `-DHLLCALC_VERSION_SUFFIX=...`，用于编译程序版本资源。

## tag 格式

接受：

```text
v1.3.0
v1.3.1
v1.4.0-rc.1
v1.4.0-beta.2
```

必须为 `vMAJOR.MINOR.PATCH` 或 `vMAJOR.MINOR.PATCH-prerelease`：

- 三个数字分量均为 `0..65535`，不允许多余前导零。
- 预发布后缀只允许 ASCII 字母、数字、连字符与点分隔标识符；纯数字标识符不允许前导零。
- 不接受 `latest`、`v1.3`、`v01.3.0`、`v1.3.0+metadata` 等格式。
- 带后缀的 tag 自动标记为 GitHub **Pre-release**。

例如，维护者审核完提交后，可自行执行以下命令；此文档和本地准备过程**不会替你推送或实际创建线上 Release**：

```bash
git tag -a v1.3.0 -m "HLLCalc 1.3.0"
git push origin v1.3.0
```

预发布示例：

```bash
git tag -a v1.4.0-rc.1 -m "HLLCalc 1.4.0 release candidate 1"
git push origin v1.4.0-rc.1
```

发布 tag 应保持不可变，不要通过移动已有 tag 来替换另一份源码。相同 tag 的工作流采用串行 concurrency；重跑时若 Release 已存在，只重新上传该 tag 的三个附件（`--clobber`），不创建重复 Release。已有 Release 的说明不会被重跑覆盖。

## 发布产物

每个版本生成：

```text
HLLCalc-<Version>-windows-x64.zip
HLLCalc-<Version>-source.zip
SHA256SUMS.txt
```

- Windows ZIP 内为可直接运行的 `HLLCalc.exe`，同时带完整主 `LICENSE`、`README.md`、`THIRD_PARTY_NOTICES.md`、第三方 `licenses/` 与 `docs/`。
- Source ZIP 正式发布时使用 **`git archive HEAD`**，包含对应 tag 的源码、完整 vendored 第三方依赖、测试、资源、构建与发布脚本。GitHub 自动生成的 Source code 链接不是唯一的对应源码交付方式，另附上述明确命名的源码 ZIP。
- `SHA256SUMS.txt` 为两个 ZIP 的 SHA-256，采用 UTF-8 无 BOM 和 LF。发布作业上传前再次校验哈希。
- 构建作业会先上传内部 Actions artifact，只有构建、测试、打包全部成功才进入发布作业。

下载后可在 PowerShell 中使用 `Get-FileHash -Algorithm SHA256 <zip路径>`，与校验文件逐项比较；在 Linux/macOS 上可使用 `sha256sum --check SHA256SUMS.txt`（macOS 可能需另装对应工具）。

## 本地打包

在安装了 C++ 桌面开发组件和 Windows SDK 的 Windows 上，使用 PowerShell 7，从仓库根运行：

```powershell
cmake -S . -B build -A x64 `
  -DHLLCALC_VERSION=1.3.0 -DHLLCALC_VERSION_SUFFIX= `
  -DBUILD_TESTING=ON -DHLLCALC_BUILD_UI_TESTS=OFF
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./scripts/package.ps1 -BuildDirectory build -Configuration Release `
  -Version 1.3.0 -OutputDirectory dist
```

预发布对应：

```powershell
cmake -S . -B build -A x64 `
  -DHLLCALC_VERSION=1.4.0 -DHLLCALC_VERSION_SUFFIX=rc.1 `
  -DBUILD_TESTING=ON -DHLLCALC_BUILD_UI_TESTS=OFF
cmake --build build --config Release --parallel
./scripts/package.ps1 -BuildDirectory build -Configuration Release `
  -Version 1.4.0-rc.1 -OutputDirectory dist
```

`package.ps1` 通过 `cmake --install` 安装到输出目录内的唯一临时 stage，然后制作 ZIP；脚本会核对版本与 CMake cache 一致、必需运行文件及法律声明完整。版本不合法、安装不完整、命令失败都会终止，不会继续发布。相同输出目录下同版本的两个 ZIP 和校验文件会被替换；其他用户目录不会被删除。

### 当前目录没有 `.git` 时

正式发布默认明确报错，不会冒充 `git archive HEAD`。为了本地验证，可显式使用：

```powershell
./scripts/package.ps1 -BuildDirectory build -Configuration Release `
  -Version 1.3.0 -OutputDirectory dist-local -AllowUntrackedSource
```

这会从固定项目文件和源码目录白名单创建快照，并在源码 ZIP 中加入 **`UNTRACKED-SNAPSHOT.txt`**。该文件说明快照没有 Git 跟踪保证，不是已验证 tag 的正式 Release。链接、junction、未知文件类型、生成物或潜在凭据文件会导致失败，需维护者审核；不要把此选项用于 GitHub 正式发布工作流。

脚本会拒绝明显的构建目录、`.vs`、`.git`、环境凭据文件、私钥及编译产物进入源码包；这不能替代维护者对已提交内容的安全审核。请在推送前检查没有把 token、密码或其他私密数据写进普通源码。

## 已验证与仍需线上验证

这些工作流文件是供仓库公开后使用的配置；本地脚本与构建可在本机验证。**未在你的 GitHub 仓库实际触发 Actions，未推送 tag，也未创建线上 Release。** 首次推送版本 tag 后，请检查 Actions 日志、Release 附件、解压后的可执行程序与许可证是否齐全。

如果发布作业报权限错误，检查组织/仓库 Actions 策略和 `GITHUB_TOKEN` 的 `contents: write` 是否被允许；不要通过把个人令牌硬编码进脚本来绕过策略。
