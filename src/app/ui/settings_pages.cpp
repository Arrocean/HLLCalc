// SPDX-License-Identifier: GPL-3.0-only
// Hotkey, settings/update and help pages, plus the install confirmation modal.
#include "app/ui/pages.h"
#include "app/ui/widgets.h"
#include "i18n/i18n.h"
#include "input/hotkey.h"
#include "update/updater.h"
#include "version.h"
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace ui::detail {
namespace {
void SyncManualHotkey() {
    auto& h=hotkeys;
    if(h.keys.empty()) {
        h.keys.push_back({VK_OEM_5,"\\"});
        for(UINT key=VK_F1;key<=VK_F24;++key)if(key!=VK_F12)h.keys.push_back({key,"F"+std::to_string(key-VK_F1+1)});
        for(UINT key='A';key<='Z';++key)h.keys.push_back({key,std::string(1,static_cast<char>(key))});
        for(UINT key='0';key<='9';++key)h.keys.push_back({key,std::string(1,static_cast<char>(key))});
        for(UINT key=VK_NUMPAD0;key<=VK_NUMPAD9;++key)h.keys.push_back({key,"小键盘 "+std::to_string(key-VK_NUMPAD0)});
        const KeyChoice extra[]={{VK_OEM_MINUS,"-"},{VK_OEM_PLUS,"="},{VK_OEM_1,";"},{VK_OEM_2,"/"},{VK_OEM_3,"`"},{VK_OEM_4,"["},{VK_OEM_6,"]"},
            {VK_OEM_7,"'"},{VK_OEM_COMMA,","},{VK_OEM_PERIOD,"."},{VK_OEM_102,"ISO \\"},{VK_INSERT,"Insert"},{VK_DELETE,"Delete"},{VK_HOME,"Home"},
            {VK_END,"End"},{VK_PRIOR,"Page Up"},{VK_NEXT,"Page Down"},{VK_LEFT,"左方向键"},{VK_RIGHT,"右方向键"},{VK_UP,"上方向键"},{VK_DOWN,"下方向键"},
            {VK_BACK,"Backspace"},{VK_ADD,"小键盘 +"},{VK_SUBTRACT,"小键盘 -"},{VK_MULTIPLY,"小键盘 *"},{VK_DIVIDE,"小键盘 /"},{VK_DECIMAL,"小键盘 ."}};
        for(const auto& choice:extra)h.keys.push_back(choice);
    }
    h.loadedKey=hotkey::Key();h.loadedMods=hotkey::Modifiers();h.index=0;
    for(size_t i=0;i<h.keys.size();++i)if(h.keys[i].key==h.loadedKey)h.index=static_cast<int>(i);
    h.ctrl=(h.loadedMods&MOD_CONTROL)!=0;h.alt=(h.loadedMods&MOD_ALT)!=0;
    h.shift=(h.loadedMods&MOD_SHIFT)!=0;h.win=(h.loadedMods&MOD_WIN)!=0;
}
bool UpdateBusy(updater::Phase phase) {
    return phase==updater::Phase::Checking||phase==updater::Phase::Downloading||phase==updater::Phase::Launching;
}
const char* UpdatePhaseText(updater::Phase phase) {
    switch(phase) {
    case updater::Phase::Checking:return "正在检查更新…";
    case updater::Phase::UpToDate:return "已是最新版本";
    case updater::Phase::Available:return "发现新版本";
    case updater::Phase::Downloading:return "正在下载更新…";
    case updater::Phase::Ready:return "更新已准备就绪";
    case updater::Phase::Launching:return "正在启动更新…";
    case updater::Phase::Error:return "更新失败";
    case updater::Phase::Cancelled:return "更新已取消";
    default:return "尚未检查更新";
    }
}
// Small pulsing dot shown beside busy update phases.
void BusyDot() {
    const float t=MotionEnabled()?0.5f+0.5f*std::sin(static_cast<float>(ImGui::GetTime())*6.0f):1.0f;
    const auto pos=ImGui::GetCursorScreenPos();
    const float h=ImGui::GetTextLineHeight();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x+S(4),pos.y+h*0.5f),S(3.5f),Color(WithAlpha(kAccent,0.35f+0.65f*t)));
    ImGui::Dummy(ImVec2(S(12),h));
    ImGui::SameLine(0,S(4));
}
void OpenReleasePage(const std::string& url) {
    if(url.rfind("https://github.com/",0)!=0){Notify("发布页面地址无效");return;}
    const auto wide=Wide(url.c_str());
    if(reinterpret_cast<INT_PTR>(ShellExecuteW(Owner(),L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)Notify("无法打开发布页面");
}
}

void HotkeyPage() {
    auto& h=hotkeys;
    if(h.keys.empty()||h.loadedKey!=hotkey::Key()||h.loadedMods!=hotkey::Modifiers())SyncManualHotkey();
    Title("快捷键设置","自定义计算器呼出 / 收起按键，准星始终独立显示。");
    BeginCard("hotkey-card",ImVec2(-1,std::max(S(500),ImGui::GetContentRegionAvail().y)));
    Heading("全局快捷键");
    Small("当前绑定");
    Font(24);ImGui::TextColored(kAccent,"%s",Utf8(hotkey::Label()).c_str());ImGui::PopFont();
    Space(8);
    const bool capturing=hotkey::IsCapturing();
    if(capturing) {
        // A breathing outline signals that the next key press will be recorded.
        const float t=MotionEnabled()?0.5f+0.5f*std::sin(static_cast<float>(ImGui::GetTime())*5.0f):1.0f;
        const auto pos=ImGui::GetCursorScreenPos();
        const float w=ImGui::GetContentRegionAvail().x,grow=S(2)*t;
        ImGui::GetWindowDrawList()->AddRect(ImVec2(pos.x-grow,pos.y-grow),ImVec2(pos.x+w+grow,pos.y+S(36)+grow),
            Color(WithAlpha(kAccent,0.25f+0.35f*t)),ImGui::GetStyle().FrameRounding+grow,0,S(1.5f));
    }
    if(Button(capturing?"正在录制：请按快捷键…":"点击录制新快捷键",ImVec2(-1,S(36)),true,"record-hotkey"))hotkey::BeginCapture();
    RecordPoint(TestRecord);
    Space(6);
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,hotkey::HasError()?kCaution:kMuted);
    ImGui::TextWrapped("%s",i18n::Translate(hotkey::Status()).c_str());
    ImGui::PopStyleColor();ImGui::PopFont();
    Space(8);
    const float half=HalfWidth();
    ImGui::BeginDisabled(!capturing);
    if(Button("取消录制",ImVec2(half,S(32))))hotkey::CancelCapture();
    RecordPoint(TestCancelRecord);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(Button("恢复默认  \\",ImVec2(half,S(32)))&&hotkey::ResetDefault())SyncManualHotkey();
    RecordPoint(TestDefaultHotkey);

    Divider(10,8);
    Small("也可手动选择（按键被系统拦截时使用）");
    Font(14);
    const float modGap=S(16);
    Check("Ctrl",h.ctrl);RecordPoint(TestCtrl);ImGui::SameLine(0,modGap);
    Check("Alt",h.alt);RecordPoint(TestAlt);ImGui::SameLine(0,modGap);
    Check("Shift",h.shift);ImGui::SameLine(0,modGap);
    Check("Win",h.win);
    ImGui::PopFont();
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##manual-hotkey",i18n::Translate(h.keys[static_cast<size_t>(h.index)].label).c_str(),ImGuiComboFlags_HeightLarge)) {
        for(size_t i=0;i<h.keys.size();++i) {
            const bool selected=h.index==static_cast<int>(i);
            if(ImGui::Selectable(StableLabel(h.keys[i].label.c_str()).c_str(),selected,0,ImVec2(0,S(30))))h.index=static_cast<int>(i);
            if(h.keys[i].key==VK_F9)RecordPoint(TestF9Option);
            if(selected)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    RecordPoint(TestManualKey);
    if(Button("应用所选快捷键",ImVec2(-1,S(32)),true)) {
        hotkey::CancelCapture();
        const UINT mods=(h.ctrl?MOD_CONTROL:0u)|(h.alt?MOD_ALT:0u)|(h.shift?MOD_SHIFT:0u)|(h.win?MOD_WIN:0u);
        hotkey::Set(h.keys[static_cast<size_t>(h.index)].key,mods);
    }
    RecordPoint(TestManualApply);
    Space(8);
    Font(14);
    if(ImGui::CollapsingHeader(StableLabel("快捷键使用说明").c_str())) {
        ImGui::TextWrapped("%s",Tr("录制时先按住修饰键，再按主键；单独按修饰键不会提交，Esc 取消。F12 和系统保留组合不可用。若系统拦截按键无法录制，请使用手动选择。"));
        Space(6);
        ImGui::TextWrapped("%s",Tr("设置立即生效并自动保存；冲突或保存失败保留原绑定。默认反斜杠 \\，录制用的那次按键不会收起窗口。请避免占用游戏及文本编辑常用键，仍可用托盘打开界面。"));
    }
    if(!hotkey::IsRegistered())ImGui::TextColored(kCaution,"%s",Tr("当前没有注册成功的快捷键，请选择其他按键。"));
    ImGui::PopFont();
    EndCard();
}

void SettingsPage() {
    Title("设置与更新","自动保存的界面选项。");
    BeginCard("settings-card",ImVec2(-1,std::max(S(500),ImGui::GetContentRegionAvail().y)));
    Heading("界面语言");
    int language=i18n::Current()==i18n::Language::English?1:0;
    const char* languages[]={"中文","English"};
    if(Combo("##language",language,languages,2,TestLanguageOption)&&
       !i18n::SetLanguage(language==1?i18n::Language::English:i18n::Language::Chinese))Notify("语言设置保存失败");
    RecordPoint(TestLanguage);
    Space(8);
    bool enabled=app.animations;
    if(Check("启用非线性动画",enabled,"animations")) {
        if(WritePrivateProfileStringW(L"Appearance",L"Animations",enabled?L"1":L"0",app.configPath.c_str())) {
            app.animations=enabled;app.motion.clear();shell.transitionStarted=ImGui::GetTime();
        } else Notify("界面设置保存失败");
    }
    RecordPoint(TestAnimations);
    Muted("页面和按钮动效可随时关闭，窗口动画使用同一设置。");

    Divider(12,10);
    Heading("应用更新");
    const auto snapshot=updater::GetSnapshot();
    const auto current=snapshot.currentVersion.empty()?std::string(HLLCALC_VERSION_STRING):snapshot.currentVersion;
    ImGui::Text("%s  %s",Tr("当前版本"),current.c_str());
    if(!snapshot.latestVersion.empty())ImGui::Text("%s  %s",Tr("最新版本"),snapshot.latestVersion.c_str());
    const bool busy=UpdateBusy(snapshot.phase);
    if(busy)BusyDot();
    ImGui::TextColored(snapshot.phase==updater::Phase::Error?kCaution:kMuted,"%s",Tr(UpdatePhaseText(snapshot.phase)));
    Space(8);
    ImGui::BeginDisabled(busy);
    if(Button("检查更新",ImVec2(-1,S(34)),true))updater::CheckAsync();
    RecordPoint(TestCheckUpdate);
    ImGui::EndDisabled();
    if(snapshot.phase==updater::Phase::Available) {
        if(Button("下载更新",ImVec2(-1,S(34)),true))updater::DownloadAsync();
        RecordPoint(TestDownload);
    }
    if(snapshot.phase==updater::Phase::Downloading) {
        const float fraction=snapshot.total?static_cast<float>(std::clamp(static_cast<double>(snapshot.downloaded)/static_cast<double>(snapshot.total),0.0,1.0)):0.0f;
        const auto count=std::to_string(snapshot.downloaded/1024)+" / "+(snapshot.total?std::to_string(snapshot.total/1024):std::string("?"))+" KB";
        Progress(fraction,count.c_str(),updates.progress,S(22));
    } else updates.progress.Reset();
    if(snapshot.phase==updater::Phase::Downloading||snapshot.phase==updater::Phase::Checking) {
        if(Button(snapshot.phase==updater::Phase::Checking?"取消检查":"取消下载",ImVec2(-1,S(32)),false,"cancel-update"))updater::Cancel();
        RecordPoint(TestCancelUpdate);
    }
    if(snapshot.phase==updater::Phase::Ready) {
        if(Button("安装并重启",ImVec2(-1,S(34)),true))installDialog.request=true;
        RecordPoint(TestInstall);
    }
    if(!snapshot.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text,kCaution);
        ImGui::TextWrapped("%s",i18n::Translate(snapshot.error).c_str());
        ImGui::PopStyleColor();
    }
    if(!snapshot.releaseUrl.empty()) {
        if(Button("打开发布页面",ImVec2(-1,S(32))))OpenReleasePage(snapshot.releaseUrl);
        RecordPoint(TestReleasePage);
    }
    Space(8);
    Muted("默认不自动下载或安装。更新完成后需确认安装并重启。");
    if(!snapshot.notes.empty()&&ImGui::CollapsingHeader(StableLabel("发行说明").c_str()))ImGui::TextWrapped("%s",snapshot.notes.c_str());
    EndCard();
}

void HelpPage() {
    Title("使用说明","专注于手动计算与独立准星，不读取、不注入、不修改游戏进程。");
    BeginCard("help-card",ImVec2(-1,S(515)));
    Heading("一键呼出，随时回到游戏");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::BulletText(Tr("当前快捷键：%s，呼出或收起计算器。"),Utf8(hotkey::Label()).c_str());
    ImGui::BulletText("%s",Tr("可在左侧“快捷键设置”录制新绑定，默认是反斜杠 \\。"));
    ImGui::BulletText("%s",Tr("呼出后直接输入距离；收起后归还原窗口焦点。"));
    ImGui::BulletText("%s",Tr("右上角最小化与关闭均为收起，准星不会消失。"));
    ImGui::BulletText("%s",Tr("彻底关闭请使用左下角“退出程序”。"));
    ImGui::PopTextWrapPos();
    Divider(10,10);
    Heading("弹道与单位");
    ImGui::TextWrapped("%s",Tr("支持德、美、苏、英四国。SPG 使用距离与车体倾角，STD 只使用距离。倾角沿用原公式的 MIL 单位，不是角度 °。负数可通过 ± 按钮或粘贴输入。"));
    Space(6);
    ImGui::TextWrapped("%s",Tr("常规火炮射程上限 1600 米；SPG 射角范围为 -89 至 466 MIL，英军上限 356 MIL。参考表按目标距离前后 15 米生成，每行 3 米。"));
    Divider(10,10);
    Heading("窗口模式与兼容性");
    ImGui::PushStyleColor(ImGuiCol_Text,kCaution);
    ImGui::TextWrapped("%s",Tr("建议使用无边框窗口模式。普通置顶窗口无法保证覆盖独占全屏，也不保证所有游戏输入捕获模式下的焦点切换。"));
    ImGui::PopStyleColor();
    Space(6);
    ImGui::TextWrapped("%s",Tr("准星为鼠标穿透的独立桌面窗口。请遵守游戏及服务器规则；本工具不承诺反作弊兼容性。参数沿用 AimerHLL v6，请自行核对游戏版本变化。"));
    Space(10);
    Small("HLLCalc " HLLCALC_VERSION_STRING " · C++17 / Dear ImGui / Win32 · GPL-3.0-only");
    EndCard();
}

void InstallDialog(Action& action) {
    const auto title=StableLabel("确认安装更新","install-update-modal");
    if(installDialog.cancel)installDialog.request=false;
    if(installDialog.request) {
        ImGui::OpenPopup(title.c_str());
        installDialog.request=false;
        installDialog.opened=ImGui::GetTime();
    }
    if(BeginModal(title.c_str(),installDialog.opened,500)) {
        const auto snapshot=updater::GetSnapshot();
        if(installDialog.cancel||snapshot.phase!=updater::Phase::Ready)ImGui::CloseCurrentPopup();
        else {
            ImGui::TextWrapped("%s",Tr("确认安装已下载的更新并重启？当前设置会先保存。"));
            Space(8);
            ImGui::Text("%s  %s",Tr("当前版本"),snapshot.currentVersion.c_str());
            ImGui::Text("%s  %s",Tr("最新版本"),snapshot.latestVersion.c_str());
            Space(12);
            const float half=HalfWidth();
            if(Button("确认安装并重启",ImVec2(half,S(36)),true)){action=Action::InstallUpdate;ImGui::CloseCurrentPopup();}
            RecordPoint(TestConfirmInstall);
            ImGui::SameLine();
            if(Button("取消",ImVec2(half,S(36))))ImGui::CloseCurrentPopup();
            RecordPoint(TestCancelInstall);
        }
        EndModal();
    }
    installDialog.cancel=false;
}
}
