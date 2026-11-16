// SPDX-License-Identifier: GPL-3.0-only
// UTF-8. Independent C++17/Win32 implementation; feature reference:
// AimerHLL/hud/crosshair_overlay.py (GPL). No Python/runtime dependency.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

namespace crosshair {
struct Config {
    int enabled = 1, dot = 1, shape = 0, cross = 0, ring = 0;
    int length = 20, thickness = 2, gap = 10, dotSize = 4, radius = 15;
    int ringThickness = 2, opacity = 255, outline = 1, outlineThickness = 1;
    int rotation = 0, scale = 100, offsetX = 0, offsetY = 0, monitor = 0;
    int red = 0, green = 255, blue = 0, ruler = 0;
    std::wstring rulerEntries = L"100:30,200:70";
};

// All calls belong to the application's UI thread. Host owns DPI awareness,
// COM and its normal Win32 message pump; this module has no settings UI.
// Initialize accepts an absolute or relative writable INI path and is idempotent.
bool Initialize(HINSTANCE instance, const std::wstring& iniPath);
Config GetConfig();
// Values are clamped; identical normalized values do not trigger a repaint.
// persist=true also saves (even if unchanged); save failures show an error.
void ApplyConfig(const Config& config, bool persist = false);
bool SaveConfig();
// Names correspond directly to Config::monitor indices (primary monitor first).
std::vector<std::wstring> MonitorNames();
int MonitorHeightPixels(); // Current overlay monitor, for static share-code scaling.
// Native file dialogs. Import applies and persists only after a successful save.
// Return false on cancellation or error; errors, unlike cancellation, show a dialog.
bool ImportPreset(HWND owner);
bool ExportPreset(HWND owner);
void RefreshTopmost(); // Never activates or invalidates the independent overlay.
void Shutdown(); // Persists current configuration; no owner relationship to main UI.
HWND OverlayWindow();
}
