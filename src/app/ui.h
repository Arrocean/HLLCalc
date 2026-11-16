// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
namespace ui {
enum class Action { None, Hide, Quit, InstallUpdate };
// Requires an existing Dear ImGui context and initialized crosshair subsystem.
// The host supplies backend initialization and a Chinese-capable default font.
bool Initialize(const std::wstring& iniPath, float scale);
void Shutdown();
bool Save();
Action Frame();
void FocusDistance();
void OpenCrosshair();
bool AnimationsEnabled();
void SetAnimationSuppressed(bool suppressed);
int DistanceValue();
bool DistanceFocused();
int CurrentPage();
int NationValue();
bool CsDialogVisible();
bool CsImportFailed();
// Read-only constraints: bit 0 field alignment, 1 input overflow, 2 table padding,
// 3 table overflow, 4 MIL baseline, 5 sidebar margins, 6 popup margins.
int LayoutIssues();
// Read-only desktop-test geometry. Host exposes it only in isolated test mode.
// IDs: navigation 0..2, enable crosshair 3, save 4, titlebar close 5,
// distance 6, inclination sign 7, hide 8, nation combo 9, nation options 10..13,
// CS import 14, code input 15, clipboard paste 16, apply 17, cancel 18,
// hotkey page 19, record 20, cancel 21, default 22, manual apply 23,
// Ctrl 24, Alt 25, F9 option 26, reserved 27, manual key combo 28.
// Settings page 29, version 30, language 31, animations 32, check 33,
// download 34, install request 35, cancel update 36, release page 37,
// confirm installation 38, cancel installation 39, Chinese option 40, English option 41.
bool TestWidgetPoint(int id, int& x, int& y);
}
