// SPDX-License-Identifier: GPL-3.0-only
// One function per screen. Pages draw inside the scrolling page container;
// dialogs are drawn at the root window scope so their popup IDs are stable.
#pragma once
#include "app/ui/common.h"

namespace ui::detail {
void CalculatorPage(Action& action);
void CrosshairPage();
void HelpPage();
void HotkeyPage();
void SettingsPage();

void CsCodeDialog();
void InstallDialog(Action& action);

// Shell chrome
void Titlebar(float width,Action& action);
void Sidebar(float width,float height,Action& action);
void StatusToast(float left,float right,float bottom);
}
