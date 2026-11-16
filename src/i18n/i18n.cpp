// SPDX-License-Identifier: GPL-3.0-only
#include "i18n/i18n.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace i18n {
namespace {
struct Entry { const char* chinese; const char* english; };
constexpr Entry entries[] = {
    // Navigation, shell and general actions.
    {"火炮计算", "Artillery"},
    {"自定义准星", "Crosshair"},
    {"使用说明", "Help"},
    {"快捷键设置", "Hotkeys"},
    {"设置与更新", "Settings & updates"},
    {"设置", "Settings"},
    {"退出程序", "Exit application"},
    {"战术工具 / TACTICAL", "TACTICAL UTILITY"},
    {"TACTICAL UTILITY  /  原生桌面工具", "TACTICAL UTILITY  /  NATIVE DESKTOP"},
    {"隐藏计算器", "Hide calculator"},
    {"显示计算器", "Show calculator"},
    {"收起计算器", "Hide calculator"},
    {"显示 / 隐藏", "Show / hide"},
    {" 显示 / 隐藏", " Show / hide"},
    {"[%s] 呼出 / 收起", "[%s] Show / hide"},
    {"呼出 / 收起", "Show / hide"},
    {"●  准星运行中", "●  Crosshair active"},
    {"○  准星已暂停", "○  Crosshair paused"},
    {"准星运行中", "Crosshair active"},
    {"准星已暂停", "Crosshair paused"},
    {"准星叠加层", "Crosshair overlay"},
    {"主显示器", "Primary display"},
    {"（主）", " (Primary)"},
    {"HLLCalc — 火炮计算与准星", "HLLCalc — Artillery & Crosshair"},
    {"取消", "Cancel"},
    {"保存", "Save"},
    {"应用", "Apply"},
    {"确认", "Confirm"},
    {"关闭", "Close"},
    {"恢复默认", "Reset defaults"},
    {"恢复默认  \\", "Reset default  \\"},
    {"保存配置", "Save settings"},
    {"配置已保存", "Settings saved"},
    {"保存失败，请检查配置目录写入权限", "Save failed. Check write access to the settings directory."},
    // Calculator.
    {"手动输入目标距离，快速获取射击参数与邻近距离修正。", "Enter the target range to calculate elevation and nearby range corrections."},
    {"射击参数", "Firing parameters"},
    {"ARTILLERY / 手动计算", "ARTILLERY / MANUAL INPUT"},
    {"阵营", "Faction"},
    {"德军 / GER", "Germany / GER"},
    {"美军 / US", "United States / US"},
    {"苏军 / USSR", "Soviet Union / USSR"},
    {"英军 / UK", "United Kingdom / UK"},
    {"德军  /  GER", "Germany  /  GER"},
    {"美军  /  US", "United States  /  US"},
    {"苏军  /  USSR", "Soviet Union  /  USSR"},
    {"英军  /  UK", "United Kingdom  /  UK"},
    {"火炮类型", "Artillery type"},
    {"自行火炮 SPG", "Self-propelled SPG"},
    {"常规火炮 STD", "Standard STD"},
    {"目标距离 / 米", "Target range / m"},
    {"目标距离", "Target range"},
    {"车体倾角 / MIL", "Hull inclination / MIL"},
    {"车体倾角", "Hull inclination"},
    {"减号当前用作快捷键，负值请用 ± 或粘贴。", "Minus is the current hotkey. Use ± or paste a negative value."},
    {"负值可直接输入，也可使用 ± 按钮。", "Type negative values directly, or use the ± button."},
    {"建议射击参数", "Recommended elevation"},
    {"超过最大射程 · 常规火炮上限 1600 米", "Out of range · Standard artillery limit: 1600 m"},
    {"超出射角限制 · 请调整距离或车体倾角", "Elevation limit exceeded · Adjust range or hull inclination"},
    {"请输入有效的距离及整数倾角", "Enter a valid range and an integer inclination"},
    {"参数在允许范围内", "Within the permitted limits"},
    {"距离修正表", "Range correction table"},
    {"11 个参考点 · 每行相差 3 米", "11 reference points · 3 m increments"},
    {"距离 / m", "Range / m"},
    {"射击参数 / MIL", "Elevation / MIL"},
    {"%d  ·  目标", "%d  ·  Target"},
    {"高亮行为当前目标，请以实际游戏版本及炮位为准。", "The highlighted row is your target. Verify the game version and gun position."},
    // Crosshair controls and presets.
    {"调整即刻应用到独立叠加层；收起计算器后，准星仍持续显示。", "Changes apply immediately to the independent overlay, which stays visible when the calculator is hidden."},
    {"外观与形状", "Appearance & shape"},
    {"启用准星", "Enable crosshair"},
    {"中心点", "Center dot"},
    {"圆环", "Ring"},
    {"点形状", "Dot shape"},
    {"方形", "Square"},
    {"圆形", "Circle"},
    {"菱形", "Diamond"},
    {"十字类型", "Cross type"},
    {"完整十字", "Full cross"},
    {"T 向下", "T down"},
    {"T 向上", "T up"},
    {"仅左右", "Horizontal only"},
    {"仅上下", "Vertical only"},
    {"准星颜色", "Crosshair color"},
    {"线长度", "Line length"},
    {"线粗细", "Line thickness"},
    {"中心间隙", "Center gap"},
    {"中心点尺寸", "Dot size"},
    {"圆环半径", "Ring radius"},
    {"圆环粗细", "Ring thickness"},
    {"黑色描边", "Black outline"},
    {"描边宽度", "Outline thickness"},
    {"不透明度", "Opacity"},
    {"旋转角度", "Rotation"},
    {"全局缩放", "Overall scale"},
    {"位置与标尺", "Position & ruler"},
    {"水平偏移 / px", "Horizontal offset / px"},
    {"垂直偏移 / px", "Vertical offset / px"},
    {"显示器", "Display"},
    {"启用下落标尺", "Enable drop ruler"},
    {"标签:像素，英文逗号分隔", "Label:pixels, separated by commas"},
    {"例如 100:30,200:70；正数向下。", "Example: 100:30,200:70; positive values go down."},
    {"准星预览", "Crosshair preview"},
    {"居中展示 · 偏移仅作用于屏幕准星", "Centered preview · Offsets affect only the desktop overlay"},
    {"预设与保存", "Presets & saving"},
    {"导入预设", "Import preset"},
    {"导出预设", "Export preset"},
    {"预设已同步", "Preset synchronized"},
    {"已恢复默认准星", "Default crosshair restored"},
    {"CS 代码仅导入静态外观；文件预设为 INI。收起时自动保存。", "CS codes import static appearance only. File presets use INI. Settings save when hidden."},
    {"准星配置", "Crosshair settings"},
    {"INI 配置 (*.ini)", "INI configuration (*.ini)"},
    {"所有文件", "All files"},
    {"无法打开文件选择对话框。", "Unable to open the file selection dialog."},
    {"无法保存准星配置，请检查 INI 路径与写入权限。", "Unable to save crosshair settings. Check the INI path and write permissions."},
    {"无法读取所选准星配置。", "Unable to read the selected crosshair preset."},
    {"所选文件缺少有效的 [Crosshair] 配置，或配置过大。", "The selected file has no valid [Crosshair] section, or the configuration is too large."},
    {"无法保存导入的准星配置，当前准星保持不变。", "Unable to save the imported preset. The current crosshair was not changed."},
    {"导出准星配置失败，请检查目标文件权限。", "Unable to export the preset. Check destination file permissions."},
    // CS import dialog.
    {"导入 CS 准星代码", "Import CS crosshair code"},
    {"粘贴 CSGO-... 或新版 CS... 分享码，只导入基础静态外观。", "Paste a CSGO-... or newer CS... share code. Only basic static appearance is imported."},
    {"从剪贴板粘贴", "Paste from clipboard"},
    {"剪贴板内容过长，请只复制准星分享码。", "Clipboard text is too long. Copy only the crosshair share code."},
    {"保留颜色、线长、粗细、间隙、中心点、描边、透明度和 T 型。动态扩散、开火/移动响应、后坐力与武器相关参数均忽略。", "Imports color, length, thickness, gap, dot, outline, opacity and T shape. Dynamic spread, firing/movement response, recoil and weapon parameters are ignored."},
    {"样式统一为静态十字 / T 型，按当前显示器换算为整数像素。描边使用黑色，半描边近似为完整描边；不导入瞄准镜准星。与 CS 内可能略有差异，可导入后微调。若自定义快捷键占用了输入字符，可使用剪贴板粘贴按钮。", "Styles become a static cross or T, scaled to whole pixels for the current display. Outlines are black; half outlines are approximated as full. Scope crosshairs are excluded. Appearance may differ slightly from CS and can be adjusted after import. Use the paste button if your hotkey captures a code character."},
    {"导入失败", "Import failed"},
    {"导入静态准星", "Import static crosshair"},
    {"已导入 CS 静态准星，动态相关参数已忽略", "Static CS crosshair imported; dynamic parameters ignored"},
    {"静态准星已应用，但保存失败，请检查配置目录权限", "Static crosshair applied, but saving failed. Check settings directory permissions."},
    {"此代码使用了暂不支持的颜色或准星样式。", "This code uses a color or crosshair style that is not supported yet."},
    {"准星尺寸或参考分辨率无效。", "Invalid crosshair dimensions or reference resolution."},
    // Key labels, capture and runtime status messages.
    {"小键盘 ", "Numpad "},
    {"小键盘 +", "Numpad +"},
    {"小键盘 -", "Numpad -"},
    {"小键盘 *", "Numpad *"},
    {"小键盘 /", "Numpad /"},
    {"小键盘 .", "Numpad ."},
    {"左方向键", "Left arrow"},
    {"右方向键", "Right arrow"},
    {"上方向键", "Up arrow"},
    {"下方向键", "Down arrow"},
    {"自定义计算器呼出 / 收起按键，准星始终独立显示。", "Customize the calculator's global show/hide shortcut. The crosshair remains independent."},
    {"全局快捷键", "Global hotkey"},
    {"当前绑定", "Current binding"},
    {"正在录制：请按快捷键…", "Recording: press a shortcut…"},
    {"点击录制新快捷键", "Record a new shortcut"},
    {"取消录制", "Cancel recording"},
    {"也可手动选择（按键被系统拦截时使用）", "Or select manually (when Windows intercepts a key)"},
    {"应用所选快捷键", "Apply selected shortcut"},
    {"快捷键使用说明", "Hotkey instructions"},
    {"录制时先按住修饰键，再按主键；单独按修饰键不会提交，Esc 取消。F12 和系统保留组合不可用。若系统拦截按键无法录制，请使用手动选择。", "Hold modifiers, then press the main key. Modifiers alone do not commit; Esc cancels. F12 and reserved system shortcuts are unavailable. Use manual selection if Windows intercepts the shortcut."},
    {"设置立即生效并自动保存；冲突或保存失败保留原绑定。默认反斜杠 \\，录制用的那次按键不会收起窗口。请避免占用游戏及文本编辑常用键，仍可用托盘打开界面。", "Changes apply immediately and save automatically. Conflicts or save failures preserve the old binding. The default is backslash \\; the recording keystroke will not hide the window. Avoid common game/editing keys. The tray icon can still open the interface."},
    {"当前没有注册成功的快捷键，请选择其他按键。", "No hotkey is registered. Choose another shortcut."},
    {"快捷键已保存：", "Hotkey saved: "},
    {"当前快捷键：", "Current hotkey: "},
    {"已取消录制。当前快捷键：", "Recording canceled. Current hotkey: "},
    {"快捷键初始化失败。可从托盘重新打开设置。", "Unable to initialize the hotkey. Reopen settings from the tray icon."},
    {"快捷键注册失败，可能已被其他程序占用。请在快捷键设置中更换。", "Hotkey registration failed; another application may be using it. Choose another in Hotkeys."},
    {"原快捷键配置无效，本次已使用默认反斜杠键。", "The saved hotkey is invalid. This session uses the default backslash key."},
    {"不支持此按键或系统保留组合。可用字母、数字、标点、导航键或功能键（F12 除外）。", "Unsupported key or reserved system shortcut. Use a letter, digit, punctuation, navigation key or function key (except F12)."},
    {"快捷键尚未初始化。", "The hotkey has not been initialized."},
    {"该快捷键无法注册，可能已被占用。原快捷键保持不变，请重试其他按键。", "Unable to register that shortcut; it may be in use. The old binding is unchanged. Try another key."},
    {"快捷键保存失败，请检查配置目录权限。原快捷键保持不变。", "Unable to save the hotkey. Check settings directory permissions. The old binding is unchanged."},
    {"请按新的快捷键；可同时按住 Ctrl / Alt / Shift / Win，Esc 取消。", "Press a new shortcut; optionally hold Ctrl / Alt / Shift / Win. Esc cancels."},
    // Help and compatibility information.
    {"专注于手动计算与独立准星，不读取、不注入、不修改游戏进程。", "Manual calculations and an independent crosshair. No game-process reading, injection or modification."},
    {"一键呼出，随时回到游戏", "Open instantly, return to the game"},
    {"当前快捷键：%s，呼出或收起计算器。", "Current shortcut: %s, to show or hide the calculator."},
    {"可在左侧“快捷键设置”录制新绑定，默认是反斜杠 \\。", "Record a new binding in Hotkeys on the left. The default is backslash \\."},
    {"呼出后直接输入距离；收起后归还原窗口焦点。", "Enter the range immediately after opening. Hiding returns focus to the previous window."},
    {"右上角最小化与关闭均为收起，准星不会消失。", "The top-right minimize and close buttons only hide the calculator; the crosshair stays visible."},
    {"彻底关闭请使用左下角“退出程序”。", "Use Exit application at the lower left to quit completely."},
    {"弹道与单位", "Ballistics & units"},
    {"支持德、美、苏、英四国。SPG 使用距离与车体倾角，STD 只使用距离。倾角沿用原公式的 MIL 单位，不是角度 °。负数可通过 ± 按钮或粘贴输入。", "Supports Germany, the US, the USSR and the UK. SPG uses range and hull inclination; STD uses range only. Inclination is in MIL, not degrees. Use the ± button or paste a negative value."},
    {"常规火炮射程上限 1600 米；SPG 射角范围为 -89 至 466 MIL，英军上限 356 MIL。参考表按目标距离前后 15 米生成，每行 3 米。", "Standard artillery is limited to 1600 m. SPG elevation ranges from -89 to 466 MIL, or up to 356 MIL for the UK. The table spans ±15 m in 3 m steps."},
    {"窗口模式与兼容性", "Window mode & compatibility"},
    {"建议使用无边框窗口模式。普通置顶窗口无法保证覆盖独占全屏，也不保证所有游戏输入捕获模式下的焦点切换。", "Use borderless windowed mode. Ordinary topmost windows cannot guarantee coverage over exclusive fullscreen or focus switching with every game input mode."},
    {"准星为鼠标穿透的独立桌面窗口。请遵守游戏及服务器规则；本工具不承诺反作弊兼容性。参数沿用 AimerHLL v6，请自行核对游戏版本变化。", "The crosshair is an independent click-through desktop window. Follow game and server rules; anti-cheat compatibility is not guaranteed. Parameters follow AimerHLL v6; verify changes in your game version."},
    // Native startup / device messages.
    {"程序图标资源缺失，请重新构建。", "Application icon resources are missing. Rebuild the application."},
    {"无法初始化界面。请检查 DirectX 11 和系统中文字体。", "Unable to initialize the interface. Check DirectX 11 and the system's Chinese-capable fonts."},
    {"图形设备不可用，程序将保存配置并退出。", "The graphics device is unavailable. Settings will be saved before the application exits."},
    // Appearance, updates and new settings page.
    {"语言", "Language"},
    {"界面语言", "Interface language"},
    {"中文", "Chinese"},
    {"简体中文", "Simplified Chinese"},
    {"跟随系统", "System default"},
    {"语言切换失败，请检查配置目录写入权限。", "Unable to change language. Check write access to the settings directory."},
    {"界面动效", "Interface animations"},
    {"启用非线性动画", "Enable eased animations"},
    {"页面和按钮动效可随时关闭，窗口动画使用同一设置。", "Page and button animations can be disabled at any time. Window animations use the same setting."},
    {"自动保存的界面选项。", "Interface preferences save automatically."},
    {"外观与更新", "Appearance & updates"},
    {"当前版本", "Current version"},
    {"检查更新", "Check for updates"},
    {"下载更新", "Download update"},
    {"安装并重启", "Install & restart"},
    {"取消下载", "Cancel download"},
    {"更新状态", "Update status"},
    {"尚未检查更新", "Updates have not been checked"},
    {"正在检查更新…", "Checking for updates…"},
    {"已是最新版本", "You are up to date"},
    {"发现新版本", "New version available"},
    {"发现新版本：", "New version available: "},
    {"正在下载更新…", "Downloading update…"},
    {"更新已准备就绪", "Update is ready"},
    {"正在启动更新…", "Starting the update…"},
    {"更新失败", "Update failed"},
    {"更新已取消", "Update canceled"},
    {"下载进度", "Download progress"},
    {"发行说明", "Release notes"},
    {"打开发布页面", "Open release page"},
    {"默认不自动下载或安装。更新完成后需确认安装并重启。", "Updates are not downloaded or installed automatically. Confirm installation and restart after downloading."},
    {"侧栏/设置版本按钮", "Sidebar/settings version button"},
    {"版本", "Version"},
    {"关于", "About"},
    {"更新设置", "Update settings"},
    {"更新服务未配置", "Update service is not configured"},
    {"无法连接更新服务器", "Unable to connect to the update server"},
    {"下载失败", "Download failed"},
    {"校验失败", "Verification failed"},
    {"更新包校验失败", "Update package verification failed"},
    {"更新包已校验", "Update package verified"},
    {"确认安装更新并重启？", "Install the update and restart?"},
    {"重试", "Retry"},
    {"稍后", "Later"},
    {"正在下载：", "Downloading: "},
    {"更新失败：", "Update failed: "},
    {"下载失败：", "Download failed: "},
    {"当前版本：", "Current version: "},
    {"最新版本：", "Latest version: "},
    {"新版本：", "New version: "},
    {"确认安装更新", "Confirm update installation"},
    {"确认安装已下载的更新并重启？当前设置会先保存。", "Install the downloaded update and restart? Your current settings will be saved first."},
    {"最新版本", "Latest version"},
    {"确认安装并重启", "Confirm install & restart"},
    {"应用更新", "Application updates"},
    {"语言设置保存失败", "Unable to save the language preference"},
    {"界面设置保存失败", "Unable to save interface preferences"},
    {"无法打开发布页面", "Unable to open the release page"},
    {"发布页面地址无效", "The release page URL is invalid"},
    {"取消检查", "Cancel check"}
};

std::atomic<Language> language{Language::Chinese};
std::wstring iniPath;
using Map=std::unordered_map<std::string_view,const Entry*>;
const Map& Dictionary() {
    static const Map dictionary=[] {
        Map result;result.reserve(sizeof(entries)/sizeof(entries[0]));
        for(const auto& entry:entries)result.emplace(entry.chinese,&entry);
        return result;
    }();
    return dictionary;
}
const Entry* Find(std::string_view key) {
    const auto& dictionary=Dictionary();const auto found=dictionary.find(key);
    return found==dictionary.end()?nullptr:found->second;
}
Language SystemLanguage() {
    return PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_CHINESE?Language::Chinese:Language::English;
}
std::wstring AbsolutePath(const std::wstring& input) {
    if(input.empty())return {};
    const DWORD required=GetFullPathNameW(input.c_str(),0,nullptr,nullptr);
    if(!required||required>32768)return {};
    std::wstring absolute(required,L'\0');
    const DWORD length=GetFullPathNameW(input.c_str(),required,absolute.data(),nullptr);
    if(!length||length>=required)return {};
    absolute.resize(length);return absolute;
}
std::string Utf8(const wchar_t* text) {
    if(!text||!*text)return {};
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);
    if(size<=1)return {};
    std::string result(static_cast<std::size_t>(size),'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,result.data(),size,nullptr,nullptr))return {};
    result.resize(static_cast<std::size_t>(size-1));return result;
}
std::wstring Utf16(const std::string& text) {
    if(text.empty())return {};
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.c_str(),static_cast<int>(text.size()),nullptr,0);
    if(size<=0)return {};
    std::wstring result(static_cast<std::size_t>(size),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.c_str(),static_cast<int>(text.size()),result.data(),size))return {};
    return result;
}
bool IsDigit(char c) {return c>='0'&&c<='9';}
bool IsFlag(char c) {return c=='-'||c=='+'||c==' '||c=='#'||c=='0'||c=='\'';}
std::vector<std::string> PrintfTokens(std::string_view text) {
    std::vector<std::string> result;
    for(std::size_t at=0;at<text.size();++at) {
        if(text[at]!='%')continue;
        std::size_t p=at+1;if(p>=text.size())continue;
        if(text[p]=='%'){++at;continue;}
        // Optional positional index, then flags, width, precision and length.
        const auto afterPercent=p;while(p<text.size()&&IsDigit(text[p]))++p;
        if(p<text.size()&&text[p]=='$')++p;else p=afterPercent;
        while(p<text.size()&&IsFlag(text[p]))++p;
        if(p<text.size()&&text[p]=='*'){++p;while(p<text.size()&&IsDigit(text[p]))++p;if(p<text.size()&&text[p]=='$')++p;}
        else while(p<text.size()&&IsDigit(text[p]))++p;
        if(p<text.size()&&text[p]=='.'){++p;if(p<text.size()&&text[p]=='*')++p;else while(p<text.size()&&IsDigit(text[p]))++p;}
        if(p+2<text.size()&&text[p]=='I'&&((text[p+1]=='3'&&text[p+2]=='2')||(text[p+1]=='6'&&text[p+2]=='4')))p+=3;
        else if(p<text.size()&&std::strchr("hljztL",text[p])){const char first=text[p++];if(p<text.size()&&text[p]==first&&(first=='h'||first=='l'))++p;}
        if(p<text.size()&&std::strchr("diuoxXfFeEgGaAcspn",text[p])) {
            result.emplace_back(text.substr(at,p-at+1));at=p;
        }
    }
    return result;
}
}
void Initialize(const std::wstring& ini) {
    iniPath=AbsolutePath(ini);Language next=SystemLanguage();
    if(!iniPath.empty()) {
        wchar_t saved[32]{};GetPrivateProfileStringW(L"Appearance",L"Language",L"",saved,32,iniPath.c_str());
        if(_wcsicmp(saved,L"zh-CN")==0)next=Language::Chinese;
        else if(_wcsicmp(saved,L"en-US")==0)next=Language::English;
    }
    language.store(next,std::memory_order_relaxed);
}
Language Current(){return language.load(std::memory_order_relaxed);}
bool SetLanguage(Language next) {
    if((next!=Language::Chinese&&next!=Language::English)||iniPath.empty())return false;
    const wchar_t* code=next==Language::Chinese?L"zh-CN":L"en-US";
    if(!WritePrivateProfileStringW(L"Appearance",L"Language",code,iniPath.c_str()))return false;
    language.store(next,std::memory_order_relaxed);return true;
}
const char* Tr(const char* chinese) {
    if(!chinese)return "";
    if(Current()==Language::Chinese||std::strstr(chinese,"##"))return chinese;
    const auto* entry=Find(chinese);return entry?entry->english:chinese;
}
std::string Translate(const std::string& text) {
    if(Current()==Language::Chinese||text.find("##")!=std::string::npos)return text;
    if(const auto* exact=Find(text))return exact->english;
    constexpr const char* prefixes[]={"已取消录制。当前快捷键：","快捷键已保存：","当前快捷键：","小键盘 ","发现新版本：","正在下载：","更新失败：","下载失败：","当前版本：","最新版本：","新版本："};
    const Entry* best=nullptr;std::size_t bestSize=0;
    for(const char* prefix:prefixes) {
        const std::size_t length=std::strlen(prefix);
        if(length>bestSize&&text.compare(0,length,prefix)==0){best=Find(prefix);bestSize=length;}
    }
    if(best)return std::string(best->english)+text.substr(bestSize);
    constexpr const char* suffixes[]={"（主）"," 显示 / 隐藏"};
    for(const char* suffix:suffixes) {
        const std::size_t length=std::strlen(suffix);
        if(text.size()>=length&&text.compare(text.size()-length,length,suffix)==0) {
            if(const auto* entry=Find(suffix))return text.substr(0,text.size()-length)+entry->english;
        }
    }
    return text;
}
std::wstring Wide(const wchar_t* chinese) {
    if(!chinese)return {};
    if(Current()==Language::Chinese)return chinese;
    const std::string utf8=Utf8(chinese);
    if(utf8.empty()&&*chinese)return chinese;
    const auto translated=Translate(utf8);
    const auto result=Utf16(translated);
    return result.empty()&&!translated.empty()?std::wstring(chinese):result;
}
std::size_t EntryCount(){return sizeof(entries)/sizeof(entries[0]);}
bool ValidateDictionary(std::string& error) {
    std::unordered_set<std::string_view> keys;keys.reserve(EntryCount());
    for(const auto& entry:entries) {
        if(!entry.chinese||!*entry.chinese||!entry.english||!*entry.english){error="Empty localization entry";return false;}
        if(!keys.emplace(entry.chinese).second){error="Duplicate localization key: "+std::string(entry.chinese);return false;}
        if(PrintfTokens(entry.chinese)!=PrintfTokens(entry.english)){error="Format placeholders differ: "+std::string(entry.chinese);return false;}
        if(std::strstr(entry.chinese,"##")||std::strstr(entry.english,"##")){error="Internal ImGui IDs must not enter the dictionary";return false;}
    }
    error.clear();return true;
}
}
