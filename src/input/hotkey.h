// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
namespace hotkey {
// UI-thread only. The previous registration stays alive until both the new
// registration and its single-value INI write succeed.
bool Initialize(HWND owner,const std::wstring& iniPath);
void Shutdown();
bool Set(UINT key,UINT modifiers);
bool ResetDefault();
void BeginCapture();
void CancelCapture();
bool IsCapturing();
// Call before ImGui and normal WM_HOTKEY handling, for all window messages.
bool HandleCaptureMessage(UINT message,WPARAM key,LPARAM data);
bool IsTrigger(WPARAM id,LPARAM packed);
bool IsRegistered();
UINT Key();
UINT Modifiers();
std::wstring Label();
std::string Status();
bool HasError();
}
