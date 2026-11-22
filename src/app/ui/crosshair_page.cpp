// SPDX-License-Identifier: GPL-3.0-only
#include "app/ui/pages.h"
#include "app/ui/widgets.h"
#include "sharecode/cs_crosshair_adapter.h"
#include "i18n/i18n.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ui::detail {
namespace {
// Lightweight DrawList representation clipped to its own panel. Offset is
// deliberately omitted so the edited shape stays visible; the overlay uses it.
void DrawCrosshair(ImDrawList* draw,ImVec2 center,const crosshair::Config& c) {
    const float k=S(static_cast<float>(c.scale)/100.0f);
    const float angle=static_cast<float>(c.rotation)*0.01745329252f;
    const float cs=std::cos(angle),sn=std::sin(angle);
    auto point=[&](float x,float y){return ImVec2(center.x+k*(x*cs-y*sn),center.y+k*(x*sn+y*cs));};
    const ImU32 ink=Tint(IM_COL32(c.red,c.green,c.blue,c.opacity)),black=Tint(IM_COL32(0,0,0,c.opacity));
    const float outline=static_cast<float>(c.outlineThickness);
    auto box=[&](float x1,float y1,float x2,float y2){
        if(c.outline){ImVec2 p[]={point(x1-outline,y1-outline),point(x2+outline,y1-outline),point(x2+outline,y2+outline),point(x1-outline,y2+outline)};draw->AddConvexPolyFilled(p,4,black);}
        ImVec2 p[]={point(x1,y1),point(x2,y1),point(x2,y2),point(x1,y2)};
        draw->AddConvexPolyFilled(p,4,ink);
    };
    const float t=static_cast<float>(c.thickness),g=static_cast<float>(c.gap),l=static_cast<float>(c.length);
    // cross: 0 full, 1 T down, 2 T up, 3 horizontal only, 4 vertical only, 5 off
    if(c.cross!=5&&l>0) {
        if(c.cross!=1&&c.cross!=3)box(-t/2,-g-l,t/2,-g);
        if(c.cross!=2&&c.cross!=3)box(-t/2,g,t/2,g+l);
        if(c.cross!=4){box(-g-l,-t/2,-g,t/2);box(g,-t/2,g+l,t/2);}
    }
    if(c.dot) {
        const float r=static_cast<float>(c.dotSize)*0.5f;
        if(c.shape==0)box(-r,-r,r,r);
        else if(c.shape==1) {
            if(c.outline)draw->AddCircleFilled(center,k*(r+outline),black);
            draw->AddCircleFilled(center,k*r,ink);
        } else {
            if(c.outline){const float o=r+outline;ImVec2 p[]={point(0,-o),point(o,0),point(0,o),point(-o,0)};draw->AddConvexPolyFilled(p,4,black);}
            ImVec2 p[]={point(0,-r),point(r,0),point(0,r),point(-r,0)};
            draw->AddConvexPolyFilled(p,4,ink);
        }
    }
    if(c.ring) {
        const float radius=k*static_cast<float>(c.radius);
        if(c.outline)draw->AddCircle(center,radius,black,0,std::max(1.0f,k*(c.ringThickness+2*outline)));
        draw->AddCircle(center,radius,ink,0,std::max(1.0f,k*c.ringThickness));
    }
    if(c.ruler) {
        const std::string entries=Utf8(c.rulerEntries);
        size_t start=0;int count=0;
        while(start<entries.size()&&count++<32) {
            size_t end=entries.find(',',start);
            if(end==std::string::npos)end=entries.size();
            const size_t split=entries.find(':',start);
            int px=0;
            if(split!=std::string::npos&&split<end&&Parse(entries.substr(split+1,end-split-1).c_str(),px,-2000,2000)) {
                const float y=static_cast<float>(px);
                draw->AddLine(point(0,0),point(0,y),ink,std::max(1.0f,k));
                draw->AddLine(point(-6,y),point(7,y),ink,std::max(1.0f,k));
                draw->AddText(point(11,y-7),ink,entries.substr(start,split-start).c_str());
            }
            start=end+1;
        }
    }
}
void Preview(float height) {
    BeginCard("preview",ImVec2(-1,height));
    Heading("准星预览","居中展示 · 偏移仅作用于屏幕准星");
    const ImVec2 top=ImGui::GetCursorScreenPos();
    ImVec2 size=ImGui::GetContentRegionAvail();
    size.x=std::max(size.x,S(10));size.y=std::max(size.y,S(40));
    const ImVec2 bottom(top.x+size.x,top.y+size.y);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(top,bottom,Tint(IM_COL32(12,13,17,255)),S(10));
    draw->PushClipRect(top,bottom,true);
    for(float x=top.x;x<bottom.x;x+=S(24))draw->AddLine(ImVec2(x,top.y),ImVec2(x,bottom.y),Tint(IM_COL32(31,33,41,255)));
    for(float y=top.y;y<bottom.y;y+=S(24))draw->AddLine(ImVec2(top.x,y),ImVec2(bottom.x,y),Tint(IM_COL32(31,33,41,255)));
    const ImVec2 center(top.x+size.x*0.5f,top.y+size.y*0.5f);
    // Enabling/disabling cross-fades the crosshair and the paused label.
    const float shown=xhair.previewVisible.Update(xhair.config.enabled?1.0f:0.0f,20.0f);
    const float alpha=ImGui::GetStyle().Alpha;
    if(shown>0.01f) {
        ImGui::GetStyle().Alpha=alpha*shown;
        DrawCrosshair(draw,center,xhair.config);
        ImGui::GetStyle().Alpha=alpha;
    }
    if(shown<0.99f) {
        const char* label=Tr("准星已暂停");
        const ImVec2 ts=ImGui::CalcTextSize(label);
        draw->AddText(ImVec2(center.x-ts.x/2,center.y-ts.y/2),Color(WithAlpha(kMuted,1.0f-shown)),label);
    }
    draw->PopClipRect();
    ImGui::Dummy(size);
    EndCard();
}
bool ControlsCard(float width,float height) {
    auto& c=xhair.config;
    bool changed=false;
    BeginCard("crosshair-controls",ImVec2(width,height));
    Heading("外观与形状");
    changed|=Check("启用准星",c.enabled);RecordPoint(TestEnableCrosshair);
    Space(4);
    changed|=Check("中心点",c.dot);ImGui::SameLine(0,S(18));changed|=Check("圆环",c.ring);
    Space(6);Small("点形状");
    const char* shapes[]={"方形","圆形","菱形"};
    changed|=Combo("##dot-shape",c.shape,shapes,3);
    Small("十字类型");
    const char* crosses[]={"完整十字","T 向下","T 向上","仅左右","仅上下","关闭"};
    changed|=Combo("##cross-type",c.cross,crosses,6);
    Space(4);Small("准星颜色");
    float color[]={c.red/255.0f,c.green/255.0f,c.blue/255.0f};
    ImGui::SetNextItemWidth(-1);
    if(ImGui::ColorEdit3("##color",color,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_DisplayHex)) {
        c.red=static_cast<int>(color[0]*255+0.5f);c.green=static_cast<int>(color[1]*255+0.5f);c.blue=static_cast<int>(color[2]*255+0.5f);
        changed=true;
    }
    Divider(5,5);
    changed|=Slider("线长度",c.length,0,500);
    changed|=Slider("线粗细",c.thickness,1,100);
    changed|=Slider("中心间隙",c.gap,-500,500);
    changed|=Slider("中心点尺寸",c.dotSize,1,200);
    changed|=Slider("圆环半径",c.radius,1,1000);
    changed|=Slider("圆环粗细",c.ringThickness,1,100);
    Space(5);
    changed|=Check("黑色描边",c.outline);
    if(c.outline)changed|=Slider("描边宽度",c.outlineThickness,1,20);
    changed|=Slider("不透明度",c.opacity,0,255);
    changed|=Slider("旋转角度",c.rotation,0,359,"%d°");
    changed|=Slider("全局缩放",c.scale,10,500,"%d%%");
    Divider(5,5);
    Heading("位置与标尺");
    changed|=SignedInput("水平偏移 / px",c.offsetX);
    changed|=SignedInput("垂直偏移 / px",c.offsetY);
    Small("显示器");
    RefreshMonitors();
    std::vector<const char*> labels;
    for(const auto& name:xhair.monitors)labels.push_back(name.c_str());
    changed|=Combo("##monitor",c.monitor,labels.data(),static_cast<int>(labels.size()));
    Space(6);
    changed|=Check("启用下落标尺",c.ruler);
    if(c.ruler) {
        Small("标签:像素，英文逗号分隔");
        ImGui::SetNextItemWidth(-1);
        if(ImGui::InputTextMultiline("##ruler",xhair.ruler,sizeof(xhair.ruler),ImVec2(-1,S(70)))){c.rulerEntries=Wide(xhair.ruler);changed=true;}
        Small("例如 100:30,200:70；正数向下。");
    }
    EndCard();
    return changed;
}
void PresetCard(float height) {
    BeginCard("preset-card",ImVec2(-1,height));
    Heading("预设与保存");
    if(Button("导入 CS 准星代码",ImVec2(-1,S(32)),true))csDialog.request=true;
    RecordPoint(TestCsImport);
    const float half=HalfWidth();
    if(Button("导入预设",ImVec2(half,S(32)))&&crosshair::ImportPreset(Owner())){SyncConfig();Notify("预设已同步");}
    ImGui::SameLine();
    if(Button("导出预设",ImVec2(half,S(32))))crosshair::ExportPreset(Owner());
    if(Button("恢复默认",ImVec2(half,S(32)))) {
        crosshair::ApplyConfig(crosshair::Config{},false);SyncConfig();Notify("已恢复默认准星");
    }
    ImGui::SameLine();
    if(Button("保存配置",ImVec2(half,S(32)),true))Notify(Save()?"配置已保存":"保存失败，请检查配置目录写入权限");
    RecordPoint(TestSaveConfig);
    Muted("CS 代码仅导入静态外观；文件预设为 INI。收起时自动保存。");
    EndCard();
}
}

void CrosshairPage() {
    Title("自定义准星","调整即刻应用到独立叠加层；收起计算器后，准星仍持续显示。");
    const float width=ImGui::GetContentRegionAvail().x;
    const bool columns=width>=S(650);
    const float gap=S(12),height=S(525);
    const float leftWidth=columns?(width-gap)*0.55f:width;
    if(ControlsCard(leftWidth,height)) {
        crosshair::ApplyConfig(xhair.config,false);
        xhair.config=crosshair::GetConfig();
    }
    if(columns)ImGui::SameLine(0,gap);else Space(6);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::BeginChild("preview-column",ImVec2(columns?width-leftWidth-gap:width,height),ImGuiChildFlags_NavFlattened,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    Preview(S(281));
    Space(6);
    PresetCard(S(228));
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void CsCodeDialog() {
    // Keep the popup ID in the root scope, never in a transient child card.
    const auto title=StableLabel("导入 CS 准星代码","cs-code-modal");
    if(csDialog.cancel)csDialog.request=false;
    if(csDialog.request) {
        csDialog.error.clear();
        ImGui::OpenPopup(title.c_str());
        csDialog.request=false;
        csDialog.opened=ImGui::GetTime();
    }
    csDialog.visible=false;
    if(BeginModal(title.c_str(),csDialog.opened,540)) {
        if(csDialog.cancel)ImGui::CloseCurrentPopup();
        else {
            csDialog.visible=true;
            Font(14);ImGui::TextWrapped("%s",Tr("粘贴 CSGO-... 或新版 CS... 分享码，只导入基础静态外观。"));ImGui::PopFont();
            Space(8);
            if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            const bool enter=ImGui::InputTextWithHint("##cs-code","CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx",csDialog.code,sizeof(csDialog.code),
                ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue);
            RecordPoint(TestCsCode);
            if(Button("从剪贴板粘贴",ImVec2(-1,S(32)))) {
                if(const char* value=ImGui::GetClipboardText()) {
                    if(std::strlen(value)<sizeof(csDialog.code)){Copy(csDialog.code,sizeof(csDialog.code),value);csDialog.error.clear();}
                    else csDialog.error="剪贴板内容过长，请只复制准星分享码。";
                }
            }
            RecordPoint(TestCsPaste);
            Space(8);
            Muted("保留颜色、线长、粗细、间隙、中心点、描边、透明度和 T 型。动态扩散、开火/移动响应、后坐力与武器相关参数均忽略。");
            Space(5);
            Muted("样式统一为静态十字 / T 型，按当前显示器换算为整数像素。描边使用黑色，半描边近似为完整描边；不导入瞄准镜准星。与 CS 内可能略有差异，可导入后微调。若自定义快捷键占用了输入字符，可使用剪贴板粘贴按钮。");
            Space(8);
            if(!csDialog.error.empty()) {
                ImGui::TextColored(kCaution,"%s",Tr("导入失败"));
                ImGui::TextWrapped("%s",i18n::Translate(csDialog.error).c_str());
                Space(6);
            }
            const float half=HalfWidth();
            const bool apply=Button("导入静态准星",ImVec2(half,S(36)),true);RecordPoint(TestCsApply);
            ImGui::SameLine();
            const bool cancel=Button("取消",ImVec2(half,S(36)));RecordPoint(TestCsCancel);
            if(apply||enter) {
                cs_crosshair::Settings settings;crosshair::Config next;
                if(cs_crosshair::Decode(csDialog.code,settings,csDialog.error)&&
                   cs_crosshair::ToStaticConfig(settings,crosshair::GetConfig(),crosshair::MonitorHeightPixels(),next,csDialog.error)) {
                    crosshair::ApplyConfig(next);SyncConfig();
                    Notify(Save()?"已导入 CS 静态准星，动态相关参数已忽略":"静态准星已应用，但保存失败，请检查配置目录权限");
                    csDialog.visible=false;
                    ImGui::CloseCurrentPopup();
                }
            }
            if(cancel){csDialog.error.clear();csDialog.visible=false;ImGui::CloseCurrentPopup();}
        }
        EndModal();
    }
    csDialog.cancel=false;
}
}
