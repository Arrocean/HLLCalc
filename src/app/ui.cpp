// SPDX-License-Identifier: GPL-3.0-only
// Standalone Dear ImGui 1.92.4 UI. Visual reference: b-launcher dark shell;
// no launcher assets, runtime, widgets or dependencies are included.
//
// Module layout (src/app/ui/):
//   common.*        shared state, palette, test IDs, helpers
//   widgets.*       animated controls, cards and modals
//   shell.cpp       titlebar, sidebar navigation, status toast
//   *_page.cpp      one file per screen
// This file only implements the public interface in app/ui.h.
#include "app/ui.h"
#include "app/ui/pages.h"
#include "app/ui/widgets.h"
#include <algorithm>

namespace ui {
using namespace detail;
namespace {
void ApplyStyle(float scale) {
    // Start from an unscaled style every time; never multiply a scaled style.
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.WindowPadding=ImVec2(16,14);style.FramePadding=ImVec2(10,6);
    style.ItemSpacing=ImVec2(8,4);style.ItemInnerSpacing=ImVec2(8,5);
    style.CellPadding=ImVec2(12,7);
    style.SelectableTextAlign=ImVec2(0,0.5f);style.ButtonTextAlign=ImVec2(0.5f,0.5f);
    style.WindowRounding=12;style.ChildRounding=10;style.FrameRounding=6;style.PopupRounding=8;style.ScrollbarRounding=8;
    style.WindowBorderSize=0;style.ChildBorderSize=1;style.FrameBorderSize=0;style.PopupBorderSize=1;
    style.ScrollbarSize=9;style.GrabMinSize=12;
    auto* c=style.Colors;
    c[ImGuiCol_Text]=kText;c[ImGuiCol_TextDisabled]=kMuted;
    c[ImGuiCol_WindowBg]=ImVec4(0.039f,0.039f,0.039f,1);
    c[ImGuiCol_ChildBg]=ImVec4(0,0,0,0);c[ImGuiCol_PopupBg]=ImVec4(0.10f,0.10f,0.10f,1);
    c[ImGuiCol_NavCursor]=WithAlpha(kAccent,0.65f);
    c[ImGuiCol_Border]=ImVec4(0.16f,0.16f,0.16f,1);c[ImGuiCol_Separator]=c[ImGuiCol_Border];
    c[ImGuiCol_FrameBg]=ImVec4(0.15f,0.15f,0.15f,1);c[ImGuiCol_FrameBgHovered]=ImVec4(0.20f,0.20f,0.22f,1);c[ImGuiCol_FrameBgActive]=ImVec4(0.20f,0.23f,0.31f,1);
    c[ImGuiCol_Button]=ImVec4(0.15f,0.15f,0.15f,1);c[ImGuiCol_ButtonHovered]=ImVec4(0.19f,0.20f,0.22f,1);c[ImGuiCol_ButtonActive]=ImVec4(0.17f,0.20f,0.27f,1);
    c[ImGuiCol_Header]=ImVec4(0.19f,0.25f,0.40f,1);c[ImGuiCol_HeaderHovered]=ImVec4(0.23f,0.30f,0.47f,1);c[ImGuiCol_HeaderActive]=ImVec4(0.28f,0.36f,0.55f,1);
    c[ImGuiCol_CheckMark]=kAccent;c[ImGuiCol_SliderGrab]=kAccent;c[ImGuiCol_SliderGrabActive]=kPurple;
    c[ImGuiCol_TextSelectedBg]=ImVec4(0.31f,0.43f,0.74f,0.5f);
    c[ImGuiCol_TableHeaderBg]=ImVec4(0.12f,0.12f,0.13f,1);
    style.ScaleAllSizes(scale);
    ImGui::GetStyle()=style;
}
void ResetMotion() {
    app.motion.clear();
    shell.transitionPage=-1;
    shell.navPosition.Reset();
    for(auto& item:shell.navSelection)item.Reset();
    calc.modeIndicator.Reset();
    xhair.previewVisible.Reset();
    updates.progress.Reset();
}
void PageContent(float side,float width,float height,Action& action) {
    if(shell.page!=shell.transitionPage){shell.transitionPage=shell.page;shell.transitionStarted=ImGui::GetTime();}
    const float eased=EaseSince(shell.transitionStarted,0.22);
    // The new page rises a few pixels while fading in and settles exactly at rest.
    const float rise=S(8)*(1.0f-eased);
    const float bar=S(kTitlebarHeight);
    ImGui::SetCursorPos(ImVec2(side,bar+rise));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*(0.6f+0.4f*eased));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(16),S(16)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
    ImGui::BeginChild("page-content",ImVec2(std::max(S(50),width-side),std::max(S(50),height-bar-rise)),
        ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_NavFlattened,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(); // cards use their own rounding
    if(calc.focusDistance&&shell.page==PageCalculator)ImGui::SetScrollY(0);
    switch(shell.page) {
    case PageCalculator:CalculatorPage(action);break;
    case PageCrosshair:CrosshairPage();break;
    case PageHelp:HelpPage();break;
    case PageHotkey:HotkeyPage();break;
    default:SettingsPage();break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
}

bool Initialize(const std::wstring& iniPath,float uiScale) {
    if(!ImGui::GetCurrentContext()||iniPath.empty())return false;
    app.configPath=iniPath;
    app.scale=std::clamp(uiScale,0.5f,4.0f);
    shell.page=PageCalculator;
    calc.focusDistance=true;calc.distanceActive=false;
    const auto* ini=app.configPath.c_str();
    calc.nation=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Calculator",L"Nation",0,ini)),0,3);
    calc.mode=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Calculator",L"Mode",0,ini)),0,1);
    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"Calculator",L"Distance",L"500",buffer,64,ini);
    Copy(calc.distance,sizeof(calc.distance),Utf8(buffer));
    GetPrivateProfileStringW(L"Calculator",L"Inclination",L"0",buffer,64,ini);
    Copy(calc.inclination,sizeof(calc.inclination),Utf8(buffer));
    SyncConfig();
    app.animations=GetPrivateProfileIntW(L"Appearance",L"Animations",1,ini)!=0;
    ResetMotion();
    ApplyStyle(app.scale);
    app.initialized=true;
    return true;
}
bool Save() {
    if(!app.initialized)return false;
    bool ok=true;
    const auto* ini=app.configPath.c_str();
    auto put=[&](const wchar_t* key,const std::wstring& value){if(!WritePrivateProfileStringW(L"Calculator",key,value.c_str(),ini))ok=false;};
    put(L"Nation",std::to_wstring(calc.nation));
    put(L"Mode",std::to_wstring(calc.mode));
    put(L"Distance",Wide(calc.distance));
    put(L"Inclination",Wide(calc.inclination));
    if(!crosshair::SaveConfig())ok=false;
    if(!WritePrivateProfileStringW(L"Appearance",L"Animations",app.animations?L"1":L"0",ini))ok=false;
    return ok;
}
void Shutdown() {
    if(app.initialized)Save();
    app.initialized=false;
    calc.distanceActive=false;
    xhair.monitors.clear();
    app.motion.clear();
    app.configPath.clear();
}
void FocusDistance() {
    shell.page=PageCalculator;
    calc.focusDistance=true;calc.distanceActive=false;
    csDialog.cancel=true;installDialog.cancel=true;
}
void OpenCrosshair() {Navigate(PageCrosshair);SyncConfig();}
int DistanceValue() {int value=0;return Parse(calc.distance,value,1,99999)?value:0;}
bool DistanceFocused() {return calc.distanceActive;}
int CurrentPage() {return shell.page;}
int NationValue() {return calc.nation;}
bool CsDialogVisible() {return csDialog.visible;}
bool CsImportFailed() {return !csDialog.error.empty();}
int LayoutIssues() {return app.layoutIssues;}
bool AnimationsEnabled() {return MotionEnabled();}
void SetAnimationSuppressed(bool value) {
    app.suppressed=value;
    if(value){app.motion.clear();shell.transitionPage=shell.page;}
}
bool TestWidgetPoint(int id,int& x,int& y) {
    if(id<0||id>=TestIdCount||!app.testPoints[id].valid)return false;
    x=app.testPoints[id].x;y=app.testPoints[id].y;
    return true;
}
Action Frame() {
    if(!app.initialized)return Action::None;
    Action action=Action::None;
    calc.distanceActive=false;
    app.layoutIssues=0;
    for(auto& point:app.testPoints)point.valid=false;
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    const float width=std::max(display.x,1.0f),height=std::max(display.y,1.0f),side=S(kSidebarWidth);
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(width,height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("HLLCalc shell",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    CsCodeDialog();
    InstallDialog(action);
    Titlebar(width,action);
    Sidebar(side,height,action);
    PageContent(side,width,height,action);
    StatusToast(side,width,height);
    ImGui::End();
    ImGui::PopStyleVar();
    return action;
}
}
