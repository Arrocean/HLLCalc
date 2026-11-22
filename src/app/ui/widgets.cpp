// SPDX-License-Identifier: GPL-3.0-only
#include "app/ui/widgets.h"
#include "i18n/i18n.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>

namespace ui::detail {
namespace {
// Per-frame card counter used to stagger card entrances on a page.
int cardOrdinal=0,cardFrame=-1;
// Modal stack depth, so EndModal pops exactly what BeginModal pushed.
bool modalOpen=false;
}

// ---- Typography -------------------------------------------------------------
void Font(float size) {ImGui::PushFont(nullptr,S(size));}
void Small(const char* key) {Font(14);ImGui::TextColored(kMuted,"%s",Tr(key));ImGui::PopFont();}
void Muted(const char* key) {
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,kMuted);
    ImGui::TextWrapped("%s",Tr(key));
    ImGui::PopStyleColor();ImGui::PopFont();
}
void Space(float amount) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(ImGui::GetStyle().ItemSpacing.x,0));
    ImGui::Dummy(ImVec2(0,S(amount)));
    ImGui::PopStyleVar();
}
void Divider(float before,float after) {Space(before);ImGui::Separator();Space(after);}
void Title(const char* title,const char* subtitle) {
    Font(24);ImGui::TextUnformatted(Tr(title));ImGui::PopFont();
    Muted(subtitle);
    Space(10);
}
void Heading(const char* title,const char* subtitle) {
    Font(18);ImGui::TextUnformatted(Tr(title));ImGui::PopFont();
    if(subtitle)Muted(subtitle);
    Space(5);
}

// ---- Feedback ---------------------------------------------------------------
MotionState& Feedback(ImGuiID id,bool hovered,bool held) {
    const int frame=ImGui::GetFrameCount();
    auto& states=app.motion;
    if(states.size()>256) {
        for(auto it=states.begin();it!=states.end();)
            it=frame-it->second.lastFrame>120?states.erase(it):std::next(it);
    }
    auto& state=states[id];
    // A widget that was not drawn last frame starts from rest, not mid-motion.
    if(state.lastFrame>=0&&frame-state.lastFrame>1){state.hover.Snap(0);state.press.Snap(0);}
    state.lastFrame=frame;
    if(!MotionEnabled()) {
        state.hover.Snap(hovered?1.0f:0.0f);
        state.press.Snap(held?1.0f:0.0f);
        return state;
    }
    const float dt=ImGui::GetIO().DeltaTime;
    state.hover.Step(hovered?1.0f:0.0f,dt,hovered?26.0f:20.0f);
    state.press.Step(held?1.0f:0.0f,dt,held?42.0f:28.0f);
    return state;
}

// ---- Buttons ----------------------------------------------------------------
namespace {
// Invisible hit target; returns item rect and spring feedback.
bool HitTarget(const char* id,ImVec2 size,MotionState*& feedback,ImVec2& a,ImVec2& b) {
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0,0,0));
    const bool clicked=ImGui::Button(id,size);
    ImGui::PopStyleColor(3);
    // Real item state: dragging across another control or a modal backdrop
    // must not make an unrelated control look pressed.
    feedback=&Feedback(ImGui::GetItemID(),ImGui::IsItemHovered(),ImGui::IsItemActive());
    a=ImGui::GetItemRectMin();b=ImGui::GetItemRectMax();
    return clicked;
}
void FocusRing(ImVec2 a,ImVec2 b) {
    if(ImGui::IsItemFocused()&&ImGui::GetIO().NavVisible)
        ImGui::GetWindowDrawList()->AddRect(a,b,Color(WithAlpha(kAccent,0.65f)),ImGui::GetStyle().FrameRounding,0,S(1));
}
}
bool Button(const char* label,ImVec2 size,bool primary,const char* stableId) {
    const auto visible=VisibleLabel(label);
    const auto textSize=ImGui::CalcTextSize(visible.c_str());
    if(size.x==0)size.x=textSize.x+ImGui::GetStyle().FramePadding.x*2;
    if(size.y==0)size.y=ImGui::GetFrameHeight();
    const auto& colors=ImGui::GetStyle().Colors;
    const ImVec4 base=primary?kPrimary:colors[ImGuiCol_Button];
    const ImVec4 lit=primary?kPrimaryHover:colors[ImGuiCol_ButtonHovered];
    const ImVec4 pressed=primary?kPrimaryPressed:colors[ImGuiCol_ButtonActive];
    const auto id=std::string("###")+(stableId?stableId:label);
    MotionState* fb=nullptr;ImVec2 a,b;
    const bool clicked=HitTarget(id.c_str(),size,fb,a,b);
    auto* draw=ImGui::GetWindowDrawList();
    const float rounding=ImGui::GetStyle().FrameRounding;
    // Press squeezes the face by 1px; it springs back on release.
    const float inset=MotionEnabled()?S(1.0f)*fb->press.value:0.0f;
    const ImVec2 fa(a.x+inset,a.y+inset),fbb(b.x-inset,b.y-inset);
    const ImVec4 fill=Mix(Mix(base,lit,fb->hover.value),pressed,fb->press.value);
    draw->AddRectFilled(fa,fbb,Color(fill),rounding);
    if(fill.w>0.01f&&fb->hover.value>0.01f) {
        const ImVec4 rim=primary?ImVec4(0.55f,0.68f,1.0f,0.35f):ImVec4(1,1,1,0.07f);
        draw->AddRect(fa,fbb,Color(WithAlpha(rim,fb->hover.value)),rounding,0,S(1));
    }
    FocusRing(a,b);
    if(!visible.empty()) {
        draw->PushClipRect(ImVec2(a.x+S(4),a.y),ImVec2(b.x-S(4),b.y),true);
        draw->AddText(ImVec2(a.x+(b.x-a.x-textSize.x)*0.5f,a.y+(b.y-a.y-textSize.y)*0.5f),Color(colors[ImGuiCol_Text]),visible.c_str());
        draw->PopClipRect();
    }
    return clicked;
}
bool GhostButton(const char* id,ImVec2 size,ImVec4 hoverFill) {
    MotionState* fb=nullptr;ImVec2 a,b;
    const bool clicked=HitTarget(id,size,fb,a,b);
    const float amount=std::clamp(fb->hover.value+0.5f*fb->press.value,0.0f,1.5f);
    if(amount>0.01f)
        ImGui::GetWindowDrawList()->AddRectFilled(a,b,Color(WithAlpha(hoverFill,amount)),ImGui::GetStyle().FrameRounding);
    FocusRing(a,b);
    return clicked;
}

// ---- Check ------------------------------------------------------------------
bool Check(const char* label,bool& value,const char* stableId) {
    const auto visible=VisibleLabel(label);
    const float box=ImGui::GetFrameHeight()*0.78f;
    const float gap=ImGui::GetStyle().ItemInnerSpacing.x;
    const auto textSize=ImGui::CalcTextSize(visible.c_str());
    const ImVec2 size(box+gap+textSize.x+S(2),ImGui::GetFrameHeight());
    const auto id=std::string("###check-")+(stableId?stableId:label);
    MotionState* fb=nullptr;ImVec2 a,b;
    const bool clicked=HitTarget(id.c_str(),size,fb,a,b);
    if(clicked)value=!value;
    // The check state glides, so toggling fills and strokes smoothly.
    auto& on=Feedback(ImGui::GetItemID()^0x5f3759dfu,false,value);
    const float t=on.press.value;
    auto* draw=ImGui::GetWindowDrawList();
    const ImVec2 p(a.x,a.y+(size.y-box)*0.5f),q(p.x+box,p.y+box);
    const auto& colors=ImGui::GetStyle().Colors;
    const ImVec4 rest=Mix(colors[ImGuiCol_FrameBg],colors[ImGuiCol_FrameBgHovered],fb->hover.value);
    const float rounding=S(4);
    draw->AddRectFilled(p,q,Color(Mix(rest,kAccent,t)),rounding);
    if(t>0.01f) {
        // Stroke draws from the short leg into the long leg as t advances.
        const ImVec2 s0(p.x+box*0.24f,p.y+box*0.52f),s1(p.x+box*0.43f,p.y+box*0.70f),s2(p.x+box*0.77f,p.y+box*0.31f);
        const float k=std::clamp(t,0.0f,1.0f);const float split=0.38f;
        const ImU32 ink=Color(WithAlpha(ImVec4(1,1,1,1),std::min(1.0f,t*1.6f)));
        const float w=std::max(1.0f,S(1.8f));
        const float k1=std::min(k/split,1.0f);
        draw->AddLine(s0,ImVec2(s0.x+(s1.x-s0.x)*k1,s0.y+(s1.y-s0.y)*k1),ink,w);
        if(k>split) {
            const float k2=(k-split)/(1.0f-split);
            draw->AddLine(s1,ImVec2(s1.x+(s2.x-s1.x)*k2,s1.y+(s2.y-s1.y)*k2),ink,w);
        }
    }
    FocusRing(p,q);
    draw->AddText(ImVec2(q.x+gap,a.y+(size.y-textSize.y)*0.5f),Color(colors[ImGuiCol_Text]),visible.c_str());
    return clicked;
}
bool Check(const char* label,int& value) {
    bool v=value!=0;
    if(!Check(label,v))return false;
    value=v?1:0;
    return true;
}

// ---- Segmented control ------------------------------------------------------
bool Segmented(const char* id,int& value,const char* const* labels,int count,Glide& indicator,float height) {
    ImGui::PushID(id);
    const float width=ImGui::GetContentRegionAvail().x;
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    const float pad=S(3),segment=(width-pad*2)/static_cast<float>(count);
    auto* draw=ImGui::GetWindowDrawList();
    const float rounding=ImGui::GetStyle().FrameRounding;
    draw->AddRectFilled(origin,ImVec2(origin.x+width,origin.y+height),Color(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]),rounding);
    const float x=indicator.Update(static_cast<float>(std::clamp(value,0,count-1)),26.0f);
    const ImVec2 pa(origin.x+pad+segment*x,origin.y+pad),pb(pa.x+segment,origin.y+height-pad);
    draw->AddRectFilled(pa,pb,Color(kPrimary),std::max(0.0f,rounding-S(1)));
    bool changed=false;
    for(int i=0;i<count;++i) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x+pad+segment*static_cast<float>(i),origin.y+pad));
        ImGui::PushID(i);
        const ImVec2 size(segment,height-pad*2);
        if(GhostButton("##segment",size,ImVec4(1,1,1,value==i?0.0f:0.04f))&&value!=i){value=i;changed=true;}
        const auto a=ImGui::GetItemRectMin();
        const auto visible=VisibleLabel(labels[i]);
        const auto ts=ImGui::CalcTextSize(visible.c_str());
        // Label brightness follows how much of the pill covers this segment.
        const float cover=std::clamp(1.0f-std::abs(x-static_cast<float>(i)),0.0f,1.0f);
        draw->PushClipRect(a,ImVec2(a.x+size.x,a.y+size.y),true);
        draw->AddText(ImVec2(a.x+(size.x-ts.x)*0.5f,a.y+(size.y-ts.y)*0.5f),Color(Mix(kMuted,kText,cover)),visible.c_str());
        draw->PopClipRect();
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width,height));
    ImGui::PopID();
    return changed;
}

// ---- Combo / sliders --------------------------------------------------------
bool Combo(const char* id,int& value,const char* const* labels,int count,int firstTestId) {
    bool changed=false;
    ImGui::SetNextItemWidth(-1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(12),S(8)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(S(8),S(4)));
    const auto preview=i18n::Translate(Tr(labels[std::clamp(value,0,count-1)]));
    if(ImGui::BeginCombo(id,preview.c_str())) {
        const auto popup=ImGui::GetWindowPos(),bounds=ImGui::GetWindowSize();
        for(int i=0;i<count;++i) {
            ImGui::PushID(i);
            const bool selected=value==i;
            if(ImGui::Selectable(StableLabel(labels[i]).c_str(),selected,0,ImVec2(0,S(32)))){value=i;changed=true;}
            if(firstTestId>=0)RecordPoint(firstTestId+i);
            if(ImGui::GetItemRectMin().x<popup.x+S(6)||ImGui::GetItemRectMax().x>popup.x+bounds.x-S(6))Flag(IssuePopupMargins);
            if(selected)ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(2);
    return changed;
}
bool Slider(const char* label,int& value,int low,int high,const char* format) {
    Small(label);
    ImGui::SetNextItemWidth(-1);
    const std::string id=std::string("##")+label;
    return ImGui::SliderInt(id.c_str(),&value,low,high,format,ImGuiSliderFlags_AlwaysClamp);
}
bool SignedInput(const char* label,int& value) {
    Small(label);
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::max(S(40),ImGui::GetContentRegionAvail().x-S(50)));
    bool changed=ImGui::InputInt("##value",&value,0,0);
    ImGui::SameLine();
    if(Button("±",ImVec2(S(40),0))){value=-std::clamp(value,-16000,16000);changed=true;}
    value=std::clamp(value,-16000,16000);
    ImGui::PopID();
    return changed;
}
void Progress(float fraction,const char* overlay,Glide& glide,float height) {
    const float shown=std::clamp(glide.Update(std::clamp(fraction,0.0f,1.0f),14.0f),0.0f,1.0f);
    ImGui::ProgressBar(shown,ImVec2(-1,height),overlay);
}
void ResultLine(const char* value,const ImVec4& color) {
    const auto origin=ImGui::GetCursorScreenPos();
    auto* font=ImGui::GetFont();
    const float valueSize=S(38),unitSize=S(14);
    ImGui::PushFont(font,valueSize);
    const auto valueExtent=ImGui::CalcTextSize(value);
    const float valueAscent=ImGui::GetFontBaked()->Ascent;
    ImGui::PopFont();
    ImGui::PushFont(font,unitSize);
    const auto unitExtent=ImGui::CalcTextSize("MIL");
    const float unitAscent=ImGui::GetFontBaked()->Ascent;
    ImGui::PopFont();
    const float unitY=origin.y+valueAscent-unitAscent;
    if(std::abs((origin.y+valueAscent)-(unitY+unitAscent))>0.5f)Flag(IssueMilBaseline);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddText(font,valueSize,origin,Color(color),value);
    draw->AddText(font,unitSize,ImVec2(origin.x+valueExtent.x+S(10),unitY),Color(kMuted),"MIL");
    ImGui::Dummy(ImVec2(valueExtent.x+S(10)+unitExtent.x,std::max(valueExtent.y,unitY-origin.y+unitExtent.y)));
}

// ---- Containers -------------------------------------------------------------
void BeginCard(const char* id,ImVec2 size) {
    const int frame=ImGui::GetFrameCount();
    if(frame!=cardFrame){cardFrame=frame;cardOrdinal=0;}
    // Each successive card on a page trails the previous one by 40ms.
    const float appear=EaseSince(shell.transitionStarted+0.04*cardOrdinal++,0.26);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*(0.35f+0.65f*appear));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,kSurface);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(14),S(12)));
    ImGui::BeginChild(id,size,ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoNavFocus);
}
void EndCard() {
    // EndChild emits the container navigation outline. Change only that draw
    // color, without opening a style-stack scope across EndChild.
    auto& navColor=ImGui::GetStyle().Colors[ImGuiCol_NavCursor];
    const auto saved=navColor;
    navColor=ImVec4(0,0,0,0);
    ImGui::EndChild();
    navColor=saved;
    ImGui::PopStyleVar();ImGui::PopStyleColor();ImGui::PopStyleVar();
}
bool BeginModal(const char* title,double opened,float width) {
    const auto display=ImGui::GetIO().DisplaySize;
    const float appear=EaseSince(opened,0.18);
    ImGui::SetNextWindowPos(ImVec2(display.x*0.5f,display.y*0.5f+S(12)*(1.0f-appear)),ImGuiCond_Always,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(S(width),display.x-S(36)),0),ImGuiCond_Appearing);
    // The backdrop dim is faded by ImGui itself (DimBgRatio) at end of frame.
    ImGui::PushStyleColor(ImGuiCol_WindowBg,kSurface);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*appear);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(22),S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,S(10));
    bool open=true;
    modalOpen=ImGui::BeginPopupModal(title,&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);
    if(!modalOpen){ImGui::PopStyleVar(3);ImGui::PopStyleColor();}
    return modalOpen;
}
void EndModal() {
    if(!modalOpen)return;
    ImGui::EndPopup();
    ImGui::PopStyleVar(3);ImGui::PopStyleColor();
    modalOpen=false;
}
float HalfWidth() {return (ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*0.5f;}
}
