// SPDX-License-Identifier: GPL-3.0-only
// Window chrome: titlebar, sidebar navigation and the floating status toast.
#include "app/ui/pages.h"
#include "app/ui/widgets.h"
#include "i18n/i18n.h"
#include "input/hotkey.h"
#include "version.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui::detail {
namespace {
constexpr float kNavTop=54.0f,kNavPitch=42.0f,kNavHeight=38.0f;

void NavIcon(ImDrawList* draw,ImVec2 p,int icon,ImU32 color) {
    const float r=S(6),w=S(1.4f);
    switch(icon) {
    case PageCalculator:
        draw->AddRect(ImVec2(p.x-r,p.y-r),ImVec2(p.x+r,p.y+r),color,S(2),0,w);
        draw->AddLine(ImVec2(p.x-r/2,p.y),ImVec2(p.x+r/2,p.y),color,w);
        break;
    case PageCrosshair:
        draw->AddCircle(p,r,color,20,w);
        draw->AddLine(ImVec2(p.x-r-S(3),p.y),ImVec2(p.x+r+S(3),p.y),color,S(1.2f));
        draw->AddLine(ImVec2(p.x,p.y-r-S(3)),ImVec2(p.x,p.y+r+S(3)),color,S(1.2f));
        break;
    default:
        draw->AddCircle(p,r,color,20,w);
        draw->AddLine(ImVec2(p.x,p.y-S(1)),ImVec2(p.x,p.y+S(3)),color,w);
        draw->AddCircleFilled(ImVec2(p.x,p.y-S(3)),S(1),color);
        break;
    }
}
int NavTestId(int page) {
    switch(page) {
    case PageCalculator:return TestNavCalculator;
    case PageCrosshair:return TestNavCrosshair;
    case PageHelp:return TestNavHelp;
    case PageHotkey:return TestNavHotkey;
    default:return TestNavSettings;
    }
}
void NavItem(int i,float width) {
    static const char* names[PageCount]={"火炮计算","自定义准星","使用说明","快捷键设置","设置与更新"};
    ImGui::SetCursorPos(ImVec2(S(14),S(kNavTop+static_cast<float>(i)*kNavPitch)));
    ImGui::PushID(i);
    const ImVec2 pos=ImGui::GetCursorScreenPos();
    const float itemWidth=width-S(28);
    if(GhostButton("##navigation",ImVec2(itemWidth,S(kNavHeight)),ImVec4(0.35f,0.40f,0.50f,0.12f)))Navigate(i);
    RecordPoint(NavTestId(i));
    const auto origin=ImGui::GetWindowPos();
    if(pos.x-origin.x<S(13)||pos.x+itemWidth>origin.x+width-S(13))Flag(IssueSidebarMargins);
    // Selection colour glides, so icon and label cross-fade between pages.
    const float selected=shell.navSelection[i].Update(shell.page==i?1.0f:0.0f,22.0f);
    auto* draw=ImGui::GetWindowDrawList();
    // Icon nudges right a touch when selected.
    NavIcon(draw,ImVec2(pos.x+S(18)+S(2)*selected,pos.y+S(19)),i,Color(Mix(kMuted,kAccent,selected)));
    const char* label=Tr(names[i]);
    float size=17.0f;
    Font(size);
    ImVec2 labelSize=ImGui::CalcTextSize(label);
    const float labelWidth=width-S(72);
    if(labelSize.x>labelWidth) {
        size=std::max(14.0f,17.0f*labelWidth/labelSize.x);
        ImGui::PopFont();Font(size);
        labelSize=ImGui::CalcTextSize(label);
    }
    draw->PushClipRect(ImVec2(pos.x+S(36),pos.y),ImVec2(pos.x+width-S(34),pos.y+S(40)),true);
    draw->AddText(ImVec2(pos.x+S(38)+S(2)*selected,pos.y+(S(kNavHeight)-labelSize.y)*0.5f),Color(Mix(kMuted,kText,selected)),label);
    draw->PopClipRect();
    ImGui::PopFont();
    ImGui::PopID();
}
}

void Titlebar(float width,Action& action) {
    auto* draw=ImGui::GetWindowDrawList();
    const float side=S(kSidebarWidth),bar=S(kTitlebarHeight);
    const float height=ImGui::GetWindowHeight();
    draw->AddRectFilled(ImVec2(0,0),ImVec2(side,height),Tint(IM_COL32(23,23,23,255)),S(14),ImDrawFlags_RoundCornersLeft);
    draw->AddLine(ImVec2(side,bar),ImVec2(side,height),Tint(IM_COL32(38,38,38,255)));
    draw->AddLine(ImVec2(side,bar),ImVec2(width,bar),Tint(IM_COL32(38,38,38,255)));
    ImGui::SetCursorPos(ImVec2(S(22),S(14)));
    Font(18);ImGui::TextUnformatted("HLL CALC");ImGui::PopFont();
    if(width>side+S(300)) {
        ImGui::SetCursorPos(ImVec2(side+S(24),S(17)));
        Small("TACTICAL UTILITY  /  原生桌面工具");
    }
    auto button=[&](const char* id,bool close) {
        const ImVec2 pos=ImGui::GetCursorScreenPos();
        const ImVec4 hover=close?ImVec4(0.41f,0.16f,0.21f,1):ImVec4(0.16f,0.16f,0.18f,1);
        if(GhostButton(id,ImVec2(S(32),S(30)),hover))action=Action::Hide;
        const ImVec2 c(pos.x+S(16),pos.y+S(15));
        const ImU32 ink=Color(kText);
        if(close) {
            draw->AddLine(ImVec2(c.x-S(4),c.y-S(4)),ImVec2(c.x+S(4),c.y+S(4)),ink,S(1.4f));
            draw->AddLine(ImVec2(c.x-S(4),c.y+S(4)),ImVec2(c.x+S(4),c.y-S(4)),ink,S(1.4f));
        } else draw->AddLine(ImVec2(c.x-S(5),c.y),ImVec2(c.x+S(5),c.y),ink,S(1.4f));
    };
    ImGui::SetCursorPos(ImVec2(width-S(82),S(9)));
    button("##minimize",false);
    ImGui::SameLine(0,S(6));
    button("##close",true);
    RecordPoint(TestClose);
}

void Sidebar(float width,float height,Action& action) {
    const float bar=S(kTitlebarHeight);
    ImGui::SetCursorPos(ImVec2(0,bar));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(14),S(14)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
    ImGui::BeginChild("sidebar",ImVec2(width,height-bar),ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_NavFlattened,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPosX(S(20));
    Small("战术工具 / TACTICAL");

    // Selection pill and accent bar ride a critically damped spring.
    const float y=shell.navPosition.Update(S(kNavTop+static_cast<float>(shell.page)*kNavPitch),24.0f);
    const auto origin=ImGui::GetWindowPos();
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(origin.x+S(14),origin.y+y),ImVec2(origin.x+width-S(14),origin.y+y+S(kNavHeight)),Color(kNavSelected),S(6));
    draw->AddRectFilled(ImVec2(origin.x+S(14),origin.y+y+S(10)),ImVec2(origin.x+S(17),origin.y+y+S(28)),Color(kAccent),S(1.5f));
    for(int i=0;i<PageCount;++i)NavItem(i,width);

    // Footer is anchored to the actual child bottom. The localized shortcut is
    // measured because long combinations may wrap.
    const float footerWidth=width-S(28);
    char shortcut[512]{};
    std::snprintf(shortcut,sizeof(shortcut),Tr("[%s] 呼出 / 收起"),Utf8(hotkey::Label()).c_str());
    Font(12);const float shortcutHeight=ImGui::CalcTextSize(shortcut,nullptr,false,footerWidth).y;ImGui::PopFont();
    const float bottom=ImGui::GetWindowHeight()-S(14);
    const float versionY=bottom-S(26);
    const float exitY=versionY-S(6)-S(32);
    const float shortcutY=exitY-S(10)-shortcutHeight;
    const float statusY=shortcutY-S(6)-S(14);
    ImGui::SetCursorPos(ImVec2(S(14),statusY-S(10)));ImGui::Separator();
    ImGui::SetCursorPos(ImVec2(S(14),statusY));
    {
        // Status dot breathes while the overlay is live.
        const bool live=xhair.config.enabled;
        const float pulse=live&&MotionEnabled()?0.5f+0.5f*std::sin(static_cast<float>(ImGui::GetTime())*2.4f):1.0f;
        Font(14);
        const ImVec2 p=ImGui::GetCursorScreenPos();
        const float lh=ImGui::GetTextLineHeight();
        const ImVec2 c(p.x+S(4),p.y+lh*0.5f);
        if(live) {
            draw->AddCircleFilled(c,S(4)+S(3)*pulse,Color(WithAlpha(kSuccess,0.18f*(1.0f-pulse*0.6f))));
            draw->AddCircleFilled(c,S(3.5f),Color(kSuccess));
        } else draw->AddCircle(c,S(3.5f),Color(kMuted),0,S(1.2f));
        ImGui::Dummy(ImVec2(S(10),lh));ImGui::SameLine(0,S(6));
        ImGui::TextColored(live?kSuccess:kMuted,"%s",Tr(live?"准星运行中":"准星已暂停"));
        ImGui::PopFont();
    }
    ImGui::SetCursorPos(ImVec2(S(14),shortcutY));
    Font(12);ImGui::PushStyleColor(ImGuiCol_Text,kMuted);
    ImGui::TextWrapped("%s",shortcut);
    ImGui::PopStyleColor();ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(S(14),exitY));
    if(Button("退出程序",ImVec2(footerWidth,S(32))))action=Action::Quit;
    ImGui::SetCursorPos(ImVec2(S(14),versionY));
    Font(14);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    if(Button("v" HLLCALC_VERSION_STRING,ImVec2(footerWidth,S(26)),false,"sidebar-version"))Navigate(PageSettings);
    RecordPoint(TestVersion);
    ImGui::PopStyleColor();ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

// Floating pill, bottom-centred over the page area. It fades and slides in and
// out instead of pushing page content, so cards never reflow.
void StatusToast(float left,float right,float bottom) {
    const double remaining=shell.statusUntil-ImGui::GetTime();
    if(remaining<=0||shell.status.empty())return;
    const double shown=kStatusSeconds-remaining;
    float t=1.0f;
    if(MotionEnabled())t=std::min(motion::EaseOutCubic(static_cast<float>(shown/0.2)),motion::EaseOutCubic(static_cast<float>(remaining/0.25)));
    const auto message=i18n::Translate(shell.status);
    Font(14);
    const ImVec2 size=ImGui::CalcTextSize(message.c_str());
    const ImVec2 pad(S(16),S(9));
    const float w=size.x+pad.x*2+S(14),h=size.y+pad.y*2;
    const float cx=(left+right)*0.5f,y=bottom-S(24)-h+S(10)*(1.0f-t);
    const ImVec2 a(cx-w*0.5f,y),b(cx+w*0.5f,y+h);
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(a.x,a.y+S(3)),ImVec2(b.x,b.y+S(3)),Color(ImVec4(0,0,0,0.35f*t)),h*0.5f);
    draw->AddRectFilled(a,b,Color(ImVec4(0.11f,0.13f,0.12f,0.97f*t)),h*0.5f);
    draw->AddRect(a,b,Color(WithAlpha(kSuccess,0.35f*t)),h*0.5f,0,S(1));
    draw->AddCircleFilled(ImVec2(a.x+pad.x+S(3),a.y+h*0.5f),S(3),Color(WithAlpha(kSuccess,t)));
    draw->AddText(ImVec2(a.x+pad.x+S(14),a.y+pad.y),Color(WithAlpha(kText,t)),message.c_str());
    ImGui::PopFont();
}
}
