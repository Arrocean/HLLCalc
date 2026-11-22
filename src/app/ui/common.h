// SPDX-License-Identifier: GPL-3.0-only
// Internal to the UI module: shared state, palette, IDs and small helpers.
// Nothing here is part of the public ui:: interface declared in app/ui.h.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "app/motion.h"
#include "app/ui.h"
#include "crosshair/crosshair.h"
#include "imgui.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui::detail {

// ---- Palette: b-launcher style dark shell with a blue accent -------------
inline const ImVec4 kText(0.98f,0.98f,0.98f,1.0f);
inline const ImVec4 kMuted(0.63f,0.63f,0.65f,1.0f);
inline const ImVec4 kAccent(0.39f,0.56f,1.0f,1.0f);
inline const ImVec4 kPurple(0.65f,0.48f,1.0f,1.0f);
inline const ImVec4 kSuccess(0.36f,0.79f,0.54f,1.0f);
inline const ImVec4 kCaution(0.91f,0.66f,0.29f,1.0f);
inline const ImVec4 kSurface(0.09f,0.09f,0.09f,1.0f);        // cards and modals
inline const ImVec4 kPrimary(0.25f,0.38f,0.76f,1.0f);
inline const ImVec4 kPrimaryHover(0.29f,0.42f,0.81f,1.0f);
inline const ImVec4 kPrimaryPressed(0.22f,0.32f,0.66f,1.0f);
inline const ImVec4 kNavSelected(0.13f,0.17f,0.25f,1.0f);

// ---- Layout constants (unscaled pixels) ----------------------------------
inline constexpr float kSidebarWidth=208.0f;
inline constexpr float kTitlebarHeight=48.0f;
inline constexpr double kStatusSeconds=3.0;

enum Page { PageCalculator, PageCrosshair, PageHelp, PageHotkey, PageSettings, PageCount };

// Desktop-test widget IDs (see ui::TestWidgetPoint). Values are a stable
// contract with tests/desktop/ui_tests.cpp; never renumber them.
enum TestId {
    TestNavCalculator=0, TestNavCrosshair=1, TestNavHelp=2,
    TestEnableCrosshair=3, TestSaveConfig=4, TestClose=5,
    TestDistance=6, TestInclinationSign=7, TestHide=8,
    TestNation=9, TestNationOption=10, // 10..13
    TestCsImport=14, TestCsCode=15, TestCsPaste=16, TestCsApply=17, TestCsCancel=18,
    TestNavHotkey=19, TestRecord=20, TestCancelRecord=21, TestDefaultHotkey=22,
    TestManualApply=23, TestCtrl=24, TestAlt=25, TestF9Option=26, TestManualKey=28,
    TestNavSettings=29, TestVersion=30, TestLanguage=31, TestAnimations=32,
    TestCheckUpdate=33, TestDownload=34, TestInstall=35, TestCancelUpdate=36,
    TestReleasePage=37, TestConfirmInstall=38, TestCancelInstall=39,
    TestLanguageOption=40, // 40..41
    TestIdCount=42
};

// Bits reported by ui::LayoutIssues().
enum LayoutIssue {
    IssueFieldAlign=1, IssueInputOverflow=2, IssueTablePadding=4, IssueTableOverflow=8,
    IssueMilBaseline=16, IssueSidebarMargins=32, IssuePopupMargins=64
};

// ---- Motion primitives ----------------------------------------------------
// Hover/press feedback for one widget, keyed by ImGuiID.
struct MotionState { motion::Spring hover,press; int lastFrame=-1; };
// A spring that snaps on first use and whenever motion is disabled.
struct Glide {
    motion::Spring spring;
    bool ready=false;
    float Update(float target,float response=24.0f);
    void Reset() {ready=false;}
};

// ---- State ----------------------------------------------------------------
struct TestPoint { int x=0,y=0; bool valid=false; };
struct AppState {
    float scale=1.0f;
    std::wstring configPath;
    bool initialized=false;
    bool animations=true,suppressed=false;
    int layoutIssues=0;
    TestPoint testPoints[TestIdCount];
    std::unordered_map<ImGuiID,MotionState> motion;
};
struct ShellState {
    int page=PageCalculator,transitionPage=-1;
    double transitionStarted=0.0;
    Glide navPosition,navSelection[PageCount];
    std::string status;
    double statusUntil=0.0;
};
struct CalculatorState {
    int nation=0,mode=0;
    char distance[24]="500",inclination[24]="0";
    bool focusDistance=true,distanceActive=false;
    Glide modeIndicator;
    std::string lastResult;
    double resultChanged=-10.0;
};
struct CrosshairState {
    crosshair::Config config;
    std::vector<std::string> monitors;
    char ruler[4096]{};
    Glide previewVisible;
};
struct CsDialogState {
    char code[160]{};
    std::string error;
    bool request=false,cancel=false,visible=false;
    double opened=0.0;
};
struct InstallDialogState { bool request=false,cancel=false; double opened=0.0; };
struct KeyChoice { UINT key; std::string label; };
struct HotkeyState {
    std::vector<KeyChoice> keys;
    int index=0;
    bool ctrl=false,alt=false,shift=false,win=false;
    UINT loadedKey=0,loadedMods=0;
};
struct UpdateState { Glide progress; };

inline AppState app;
inline ShellState shell;
inline CalculatorState calc;
inline CrosshairState xhair;
inline CsDialogState csDialog;
inline InstallDialogState installDialog;
inline HotkeyState hotkeys;
inline UpdateState updates;

// ---- Helpers --------------------------------------------------------------
float S(float n);                       // scale unscaled pixels to the DPI scale
const char* Tr(const char* key);        // dictionary lookup (Chinese source keys)
std::string VisibleLabel(const char* source);
std::string StableLabel(const char* source,const char* id=nullptr);
bool MotionEnabled();
ImVec4 Mix(ImVec4 a,ImVec4 b,float t);
ImVec4 WithAlpha(ImVec4 c,float alpha);
ImU32 Color(ImVec4 c);                  // honours the current style alpha
ImU32 Tint(ImU32 c);
// EaseOutCubic of the time elapsed since `started`; 1 when motion is disabled.
float EaseSince(double started,double seconds);
std::string Utf8(const std::wstring& value);
std::wstring Wide(const char* value);
void Copy(char* output,size_t capacity,const std::string& value);
bool Parse(const char* value,int& result,int low,int high,bool emptyZero=false);
void RecordPoint(int id);
void Flag(int issue);
void Notify(const char* message);
void RefreshMonitors();
void SyncConfig();
HWND Owner();
void Navigate(int page);
}
