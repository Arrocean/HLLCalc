// SPDX-License-Identifier: GPL-3.0-only
// Styled, animated building blocks shared by every page. All drawing honours
// ImGui's style alpha so page/card fades compose naturally.
#pragma once
#include "app/ui/common.h"

namespace ui::detail {
// Typography
void Font(float size);
void Small(const char* key);
void Muted(const char* key);            // wrapped 14px secondary text
void Space(float amount=6);
void Divider(float before=6,float after=6);
void Title(const char* title,const char* subtitle);
void Heading(const char* title,const char* subtitle=nullptr);

// Controls
// Spring-animated hover/press feedback, shared by every custom control.
MotionState& Feedback(ImGuiID id,bool hovered,bool held);
bool Button(const char* label,ImVec2 size,bool primary=false,const char* stableId=nullptr);
// Ghost button: transparent at rest; `hoverFill` fades in on hover.
bool GhostButton(const char* id,ImVec2 size,ImVec4 hoverFill);
// Checkbox with an animated fill and check stroke. The whole row is clickable.
bool Check(const char* label,bool& value,const char* stableId=nullptr);
bool Check(const char* label,int& value);
// Two or more mutually exclusive options with a sliding selection pill.
bool Segmented(const char* id,int& value,const char* const* labels,int count,Glide& indicator,float height);
bool Combo(const char* id,int& value,const char* const* labels,int count,int firstTestId=-1);
bool Slider(const char* label,int& value,int low,int high,const char* format="%d");
bool SignedInput(const char* label,int& value);
// Bar whose fill glides to `fraction`.
void Progress(float fraction,const char* overlay,Glide& glide,float height);
// Large MIL value with the unit aligned to the value baseline.
void ResultLine(const char* value,const ImVec4& color);

// Containers. Cards fade in with a short stagger after each page change.
void BeginCard(const char* id,ImVec2 size);
void EndCard();
// Centered modal that fades, rises and dims its backdrop when it opens.
bool BeginModal(const char* title,double opened,float width);
void EndModal();
float HalfWidth();                      // width for two controls on one row
}
