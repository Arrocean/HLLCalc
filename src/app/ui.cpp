// SPDX-License-Identifier: GPL-3.0-only
// Standalone Dear ImGui 1.92.4 UI. Visual reference: b-launcher dark shell;
// no launcher assets, runtime, widgets or dependencies are included.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "app/ui.h"
#include "ballistics/ballistics.h"
#include "crosshair/crosshair.h"
#include "sharecode/cs_crosshair_adapter.h"
#include "imgui.h"
#include "input/hotkey.h"
#include "version.h"
#include "i18n/i18n.h"
#include "update/updater.h"
#include <shellapi.h>
#include <unordered_map>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace ui {
namespace {
float scale = 1.0f;
std::wstring configPath;
int page = 0, nation = 0, mode = 0;
char distanceText[24] = "500", inclinationText[24] = "0", rulerText[4096]{},csCodeText[160]{};
std::string csCodeError;
bool requestCsDialog=false,cancelCsDialog=false,csDialogVisible=false;
bool initialized = false, focusDistance = true, distanceActive = false;
crosshair::Config config;
std::vector<std::string> monitorNames;
double statusUntil = 0.0;
struct TestPoint {int x=0,y=0; bool valid=false;};
TestPoint testPoints[42];
bool animations=true,animationSuppressed=false;
int transitionPage=-1;
double pageTransitionStarted=0.0;
bool requestInstallConfirmation=false,cancelInstallConfirmation=false;
struct MotionState {float hover=0,press=0;};
std::unordered_map<ImGuiID,MotionState> motionStates;
struct KeyChoice {UINT key;std::string label;};
std::vector<KeyChoice> manualKeys;
int manualKeyIndex=0;
bool manualCtrl=false,manualAlt=false,manualShift=false,manualWin=false;
UINT manualLoadedKey=0,manualLoadedMods=0;
int layoutIssues=0;
void RecordPoint(int id) {
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    testPoints[id]={static_cast<int>((a.x+b.x)*0.5f),static_cast<int>((a.y+b.y)*0.5f),ImGui::IsItemVisible()};
}
std::string statusText;
const ImVec4 text(0.98f, 0.98f, 0.98f, 1.0f), muted(0.63f, 0.63f, 0.65f, 1.0f);
const ImVec4 accent(0.39f, 0.56f, 1.0f, 1.0f), purple(0.65f, 0.48f, 1.0f, 1.0f);
const ImVec4 success(0.36f, 0.79f, 0.54f, 1.0f), caution(0.91f, 0.66f, 0.29f, 1.0f);
float S(float n) { return n * scale; }
const char* Tr(const char* key) { return i18n::Tr(key); }
std::string VisibleLabel(const char* source) {
    const char* hidden=std::strstr(source,"##");
    std::string visible(source,hidden?static_cast<size_t>(hidden-source):std::strlen(source));
    return i18n::Translate(Tr(visible.c_str()));
}
std::string StableLabel(const char* source,const char* id=nullptr) {
    return VisibleLabel(source)+"###"+(id?id:source);
}
bool MotionEnabled() {return animations&&!animationSuppressed;}
ImVec4 Mix(ImVec4 a,ImVec4 b,float t) {
    return ImVec4(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t);
}
ImU32 Color(ImVec4 c) {c.w*=ImGui::GetStyle().Alpha;return ImGui::ColorConvertFloat4ToU32(c);}
ImU32 Tint(ImU32 c) {
    const auto alpha=static_cast<unsigned>(static_cast<float>((c>>24)&255u)*ImGui::GetStyle().Alpha);
    return (c&0x00ffffffu)|(alpha<<24);
}
MotionState& Motion(ImGuiID id,bool hovered,bool held) {
    if(motionStates.size()>256)motionStates.clear();
    auto& state=motionStates[id];
    if(!MotionEnabled()){state.hover=hovered?1.0f:0.0f;state.press=held?1.0f:0.0f;return state;}
    const float dt=std::clamp(ImGui::GetIO().DeltaTime,0.0f,0.1f);
    state.hover+=((hovered?1.0f:0.0f)-state.hover)*(1.0f-std::exp(-18.0f*dt));
    state.press+=((held?1.0f:0.0f)-state.press)*(1.0f-std::exp(-25.0f*dt));
    return state;
}
std::string Utf8(const std::wstring& value) {
    if(value.empty()) return {};
    const int n=WideCharToMultiByte(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(static_cast<size_t>(n),'\0');
    if(n) WideCharToMultiByte(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),result.data(),n,nullptr,nullptr);
    return result;
}
std::wstring Wide(const char* value) {
    const int length=static_cast<int>(std::strlen(value));
    const int n=MultiByteToWideChar(CP_UTF8,0,value,length,nullptr,0);
    std::wstring result(static_cast<size_t>(n),L'\0');
    if(n) MultiByteToWideChar(CP_UTF8,0,value,length,result.data(),n);
    return result;
}
void Copy(char* output, size_t capacity, const std::string& value) {
    if(capacity) { const auto n=std::min(capacity-1,value.size()); std::memcpy(output,value.data(),n);output[n]=0; }
}
bool Parse(const char* value, int& result, int low, int high, bool emptyZero=false) {
    if(!*value) {result=0;return emptyZero;}
    char* end=nullptr;const long parsed=std::strtol(value,&end,10);
    if(end==value || *end || parsed<low || parsed>high) return false;
    result=static_cast<int>(parsed);return true;
}
void Font(float size) { ImGui::PushFont(nullptr,S(size)); }
void Small(const char* value) {Font(14);ImGui::TextColored(muted,"%s",Tr(value));ImGui::PopFont();}
void Space(float amount=6) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(ImGui::GetStyle().ItemSpacing.x,0));
    ImGui::Dummy(ImVec2(0,S(amount)));ImGui::PopStyleVar();
}
void Title(const char* title,const char* subtitle) {
    Font(24);ImGui::TextUnformatted(Tr(title));ImGui::PopFont();
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);
    ImGui::TextWrapped("%s",Tr(subtitle));ImGui::PopStyleColor();ImGui::PopFont();Space(8);
}
void Heading(const char* title,const char* subtitle=nullptr) {
    Font(18);ImGui::TextUnformatted(Tr(title));ImGui::PopFont();
    if(subtitle){Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);ImGui::TextWrapped("%s",Tr(subtitle));ImGui::PopStyleColor();ImGui::PopFont();}
    Space(5);
}
bool Button(const char* label,ImVec2 size,bool primary=false,const char* stableId=nullptr) {
    const auto translated=VisibleLabel(label),widget=StableLabel(label,stableId);
    const auto textSize=ImGui::CalcTextSize(translated.c_str());
    if(size.x==0)size.x=textSize.x+ImGui::GetStyle().FramePadding.x*2;
    if(size.y==0)size.y=ImGui::GetFrameHeight();
    const auto origin=ImGui::GetCursorScreenPos();
    const float width=size.x<0?std::max(1.0f,ImGui::GetContentRegionAvail().x+size.x):size.x;
    const bool hover=ImGui::IsMouseHoveringRect(origin,ImVec2(origin.x+width,origin.y+size.y));
    auto& motion=Motion(ImGui::GetID(widget.c_str()),hover,hover&&ImGui::IsMouseDown(ImGuiMouseButton_Left));
    const auto base=primary?ImVec4(0.25f,0.38f,0.76f,1):ImGui::GetStyle().Colors[ImGuiCol_Button];
    const auto lit=primary?ImVec4(0.34f,0.48f,0.88f,1):ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
    const auto pressed=primary?ImVec4(0.22f,0.32f,0.66f,1):ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
    const auto empty=std::string("###")+(stableId?stableId:label);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0,0,0));
    const bool clicked=ImGui::Button(empty.c_str(),size);ImGui::PopStyleColor(3);
    if(MotionEnabled()&&(ImGui::IsItemActivated()||clicked))motion.press=1.0f;
    auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    const float inset=MotionEnabled()?S(1.2f)*motion.press:0.0f;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(a.x+inset,a.y+inset),ImVec2(b.x-inset,b.y-inset),Color(Mix(Mix(base,lit,motion.hover),pressed,motion.press)),ImGui::GetStyle().FrameRounding);
    if(!translated.empty()) {
        draw->PushClipRect(ImVec2(a.x+S(4),a.y),ImVec2(b.x-S(4),b.y),true);
        draw->AddText(ImVec2(a.x+(b.x-a.x-textSize.x)*0.5f,a.y+(b.y-a.y-textSize.y)*0.5f+inset*0.3f),Color(ImGui::GetStyle().Colors[ImGuiCol_Text]),translated.c_str());
        draw->PopClipRect();
    }
    return clicked;
}
bool Check(const char* label,int& value) {
    bool v=value!=0;const auto id=StableLabel(label);
    if(ImGui::Checkbox(id.c_str(),&v)){value=v?1:0;return true;}return false;
}
void BeginCard(const char* id,ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0.09f,0.09f,0.09f,1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(16),S(14)));
    ImGui::BeginChild(id,size,ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoNavFocus);
}
void EndCard() {
    // EndChild emits the container navigation outline. Temporarily change only
    // that draw color without opening a style-stack scope across EndChild.
    auto& navColor=ImGui::GetStyle().Colors[ImGuiCol_NavCursor];const auto saved=navColor;
    navColor=ImVec4(0,0,0,0);ImGui::EndChild();navColor=saved;
    ImGui::PopStyleVar();ImGui::PopStyleColor();
}
bool Combo(const char* id,int& value,const char* const* labels,int count) {
    bool changed=false;ImGui::SetNextItemWidth(-1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(12),S(8)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(S(8),S(4)));
    const auto preview=i18n::Translate(Tr(labels[std::clamp(value,0,count-1)]));
    if(ImGui::BeginCombo(id,preview.c_str())) {
        for(int i=0;i<count;++i) {
            ImGui::PushID(i);const bool selected=value==i;const auto label=StableLabel(labels[i]);
            if(ImGui::Selectable(label.c_str(),selected,0,ImVec2(0,S(32)))){value=i;changed=true;}
            if(std::strcmp(id,"##nation")==0)RecordPoint(10+i);
            if(std::strcmp(id,"##language")==0)RecordPoint(40+i);
            const auto popup=ImGui::GetWindowPos(),bounds=ImGui::GetWindowSize();
            if(ImGui::GetItemRectMin().x<popup.x+S(6)||ImGui::GetItemRectMax().x>popup.x+bounds.x-S(6))layoutIssues|=64;
            if(selected)ImGui::SetItemDefaultFocus();ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(2);return changed;
}
void ResultLine(const char* value,const ImVec4& color) {
    const auto origin=ImGui::GetCursorScreenPos();auto* font=ImGui::GetFont();
    const float valueFontSize=S(38),unitFontSize=S(14);
    ImGui::PushFont(font,valueFontSize);const auto valueSize=ImGui::CalcTextSize(value);
    const float valueAscent=ImGui::GetFontBaked()->Ascent;ImGui::PopFont();
    ImGui::PushFont(font,unitFontSize);const auto unitSize=ImGui::CalcTextSize("MIL");
    const float unitAscent=ImGui::GetFontBaked()->Ascent;ImGui::PopFont();
    const float unitY=origin.y+valueAscent-unitAscent;
    if(std::abs((origin.y+valueAscent)-(unitY+unitAscent))>0.5f)layoutIssues|=16;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddText(font,valueFontSize,origin,Color(color),value);
    draw->AddText(font,unitFontSize,ImVec2(origin.x+valueSize.x+S(10),unitY),Color(muted),"MIL");
    ImGui::Dummy(ImVec2(valueSize.x+S(10)+unitSize.x,std::max(valueSize.y,unitY-origin.y+unitSize.y)));
}
void Notify(const char* message) {statusText=message;statusUntil=ImGui::GetTime()+3.0;}
void SyncConfig() {
    config=crosshair::GetConfig();Copy(rulerText,sizeof(rulerText),Utf8(config.rulerEntries));
    monitorNames.clear();for(const auto& name:crosshair::MonitorNames()) monitorNames.push_back(Utf8(name));
    if(monitorNames.empty()) monitorNames.emplace_back("主显示器");
}
HWND Owner() {auto* viewport=ImGui::GetMainViewport();return viewport && viewport->PlatformHandleRaw?static_cast<HWND>(viewport->PlatformHandleRaw):GetActiveWindow();}
void CsCodeDialog() {
    // Keep the popup ID in the root scope, never in a transient child card.
    if(cancelCsDialog)requestCsDialog=false;
    if(requestCsDialog&&!cancelCsDialog) {
        csCodeError.clear();ImGui::OpenPopup(StableLabel("导入 CS 准星代码","cs-code-modal").c_str());requestCsDialog=false;
    }
    csDialogVisible=false;
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x*0.5f,display.y*0.5f),ImGuiCond_Appearing,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(S(540),display.x-S(36)),0),ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(22),S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,S(10));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.09f,0.09f,0.09f,1));
    bool open=true;
    if(ImGui::BeginPopupModal(StableLabel("导入 CS 准星代码","cs-code-modal").c_str(),&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize)) {
        if(cancelCsDialog)ImGui::CloseCurrentPopup();
        else {
            csDialogVisible=true;
            Font(14);ImGui::TextWrapped(Tr("粘贴 CSGO-... 或新版 CS... 分享码，只导入基础静态外观。"));ImGui::PopFont();Space(8);
            if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            const bool enter=ImGui::InputTextWithHint("##cs-code","CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx",csCodeText,sizeof(csCodeText),ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue);
            RecordPoint(15);
            if(Button("从剪贴板粘贴",ImVec2(-1,S(32)))) {
                if(const char* value=ImGui::GetClipboardText()) {
                    if(std::strlen(value)<sizeof(csCodeText)){Copy(csCodeText,sizeof(csCodeText),value);csCodeError.clear();}
                    else csCodeError="剪贴板内容过长，请只复制准星分享码。";
                }
            }
            RecordPoint(16);Space(8);
            Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);
            ImGui::TextWrapped(Tr("保留颜色、线长、粗细、间隙、中心点、描边、透明度和 T 型。动态扩散、开火/移动响应、后坐力与武器相关参数均忽略。"));
            Space(5);ImGui::TextWrapped(Tr("样式统一为静态十字 / T 型，按当前显示器换算为整数像素。描边使用黑色，半描边近似为完整描边；不导入瞄准镜准星。与 CS 内可能略有差异，可导入后微调。若自定义快捷键占用了输入字符，可使用剪贴板粘贴按钮。"));
            ImGui::PopStyleColor();ImGui::PopFont();Space(8);
            if(!csCodeError.empty()) {ImGui::TextColored(caution,Tr("导入失败"));ImGui::TextWrapped("%s",i18n::Translate(csCodeError).c_str());Space(6);}
            const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*0.5f;
            const bool apply=Button("导入静态准星",ImVec2(half,S(36)),true);RecordPoint(17);
            ImGui::SameLine();const bool cancel=Button("取消",ImVec2(half,S(36)));RecordPoint(18);
            if(apply||enter) {
                cs_crosshair::Settings settings;crosshair::Config next;
                if(cs_crosshair::Decode(csCodeText,settings,csCodeError)&&
                   cs_crosshair::ToStaticConfig(settings,crosshair::GetConfig(),crosshair::MonitorHeightPixels(),next,csCodeError)) {
                    crosshair::ApplyConfig(next);SyncConfig();
                    if(Save())Notify("已导入 CS 静态准星，动态相关参数已忽略");
                    else Notify("静态准星已应用，但保存失败，请检查配置目录权限");
                    csDialogVisible=false;ImGui::CloseCurrentPopup();
                }
            }
            if(cancel){csCodeError.clear();csDialogVisible=false;ImGui::CloseCurrentPopup();}
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();ImGui::PopStyleVar(2);cancelCsDialog=false;
}
void Negate(char* buffer,size_t capacity) {int value=0;if(Parse(buffer,value,-9999,9999,true)) std::snprintf(buffer,capacity,"%d",-value);}
const char* LimitText(ballistics::Status status) {
    switch(status) {
    case ballistics::Status::RangeExceeded:return "超过最大射程 · 常规火炮上限 1600 米";
    case ballistics::Status::ElevationExceeded:return "超出射角限制 · 请调整距离或车体倾角";
    case ballistics::Status::InvalidDistance:return "请输入有效的距离及整数倾角";
    default:return "参数在允许范围内";
    }
}
void Calculator(Action& action) {
    Title("火炮计算","手动输入目标距离，快速获取射击参数与邻近距离修正。");
    const auto available=ImGui::GetContentRegionAvail();
    const bool columns=available.x>=S(680);
    const float gap=S(16),leftWidth=columns?(available.x-gap)*0.49f:available.x;
    // A full calculator card always has enough room for its contents. On short
    // windows the page scrolls as a whole instead of clipping the action button.
    const float cardHeight=columns?std::max(S(500),available.y):S(500);
    BeginCard("input-card",ImVec2(leftWidth,cardHeight));
    Heading("射击参数");
    Small("阵营");
    const char* nations[]={"德军 / GER","美军 / US","苏军 / USSR","英军 / UK"};
    Combo("##nation",nation,nations,4);RecordPoint(9);
    Space(6);Small("火炮类型");
    const float segment=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*0.5f;
    Font(14);
    if(Button("自行火炮 SPG",ImVec2(segment,S(36)),mode==0))mode=0;
    ImGui::SameLine();if(Button("常规火炮 STD",ImVec2(segment,S(36)),mode==1))mode=1;
    ImGui::PopFont();
    Space(8);Small("目标距离 / 米");
    if(focusDistance){ImGui::SetScrollY(0);ImGui::SetKeyboardFocusHere();}
    Font(20);ImGui::SetNextItemWidth(-1);
    bool selectOnFocus=focusDistance;
    auto selectDistance=[](ImGuiInputTextCallbackData* data)->int {
        if(*static_cast<bool*>(data->UserData)){data->SelectAll();data->CursorPos=data->BufTextLen;}
        return 0;
    };
    ImGui::InputText("##distance",distanceText,sizeof(distanceText),ImGuiInputTextFlags_CharsDecimal|ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_CallbackAlways,selectDistance,&selectOnFocus);
    RecordPoint(6);distanceActive=ImGui::IsItemActive();focusDistance=false;ImGui::PopFont();
    Space(6);Small("车体倾角 / MIL");
    ImGui::BeginDisabled(mode==1);Font(20);
    const float fieldHeight=ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(std::max(S(50),ImGui::GetContentRegionAvail().x-S(42)-ImGui::GetStyle().ItemSpacing.x));
    ImGui::InputText("##inclination",inclinationText,sizeof(inclinationText),ImGuiInputTextFlags_CharsDecimal|ImGuiInputTextFlags_AutoSelectAll);
    const auto fieldMin=ImGui::GetItemRectMin(),fieldMax=ImGui::GetItemRectMax();
    ImGui::SameLine();if(Button("±##inclination",ImVec2(S(42),fieldHeight)))Negate(inclinationText,sizeof(inclinationText));
    RecordPoint(7);
    if(std::abs(fieldMin.y-ImGui::GetItemRectMin().y)>0.5f||std::abs(fieldMax.y-ImGui::GetItemRectMax().y)>0.5f)layoutIssues|=1;
    ImGui::PopFont();ImGui::EndDisabled();
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);
    ImGui::TextWrapped("%s",Tr(hotkey::Key()==VK_OEM_MINUS&&hotkey::Modifiers()==0?"减号当前用作快捷键，负值请用 ± 或粘贴。":"负值可直接输入，也可使用 ± 按钮。"));
    ImGui::PopStyleColor();ImGui::PopFont();
    Space(6);ImGui::Separator();Space(6);
    int distance=0,inclination=0;
    const bool valid=Parse(distanceText,distance,1,99999)&&(mode==1||Parse(inclinationText,inclination,-9999,9999,true));
    const auto result=ballistics::Calculate(static_cast<ballistics::Nation>(nation),mode==0?ballistics::Mode::SPG:ballistics::Mode::Standard,valid?distance:0,inclination);
    Small("建议射击参数");
    const auto value=valid?std::to_string(result.mil):std::string("—");
    ResultLine(value.c_str(),result.status==ballistics::Status::Valid?accent:caution);
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,result.status==ballistics::Status::Valid?success:caution);
    ImGui::TextWrapped("%s",Tr(LimitText(result.status)));ImGui::PopStyleColor();ImGui::PopFont();
    Space(8);
    if(Button("收起计算器",ImVec2(-1,S(36)),true))action=Action::Hide;
    RecordPoint(8);
    if(ImGui::GetCursorPosY()+ImGui::GetScrollY()-ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y>cardHeight+1)layoutIssues|=2;
    EndCard();

    if(columns)ImGui::SameLine(0,gap);else Space(12);
    BeginCard("reference-card",ImVec2(columns?available.x-leftWidth-gap:available.x,cardHeight));
    Heading("距离修正表","11 个参考点 · 每行相差 3 米");
    const auto rows=ballistics::ReferenceTable(static_cast<ballistics::Nation>(nation),mode==0?ballistics::Mode::SPG:ballistics::Mode::Standard,valid?distance:0,inclination);
    Font(15);ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(S(12),S(7)));
    const float tableLeft=ImGui::GetCursorScreenPos().x;
    if(ImGui::BeginTable("range-table",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn(Tr("距离 / m"));ImGui::TableSetupColumn(Tr("射击参数 / MIL"));ImGui::TableHeadersRow();
        for(int i=0;i<11;++i) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None,S(31));
            if(i==5)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,IM_COL32(37,49,78,255));
            const ImVec4 color=i==5?accent:(rows[i].status!=ballistics::Status::Valid?muted:text);
            ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::TableSetColumnIndex(0);
            if(ImGui::GetCursorScreenPos().x-tableLeft<S(10))layoutIssues|=4;
            if(rows[i].status==ballistics::Status::InvalidDistance)ImGui::TextUnformatted("—");
            else ImGui::Text(Tr(i==5?"%d  ·  目标":"%d"),rows[i].distance);
            ImGui::TableSetColumnIndex(1);
            if(rows[i].status==ballistics::Status::InvalidDistance)ImGui::TextUnformatted("—");else ImGui::Text("%d",rows[i].mil);
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();ImGui::PopFont();
    Space(10);Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);
    ImGui::TextWrapped(Tr("高亮行为当前目标，请以实际游戏版本及炮位为准。"));
    ImGui::PopStyleColor();ImGui::PopFont();
    if(ImGui::GetCursorPosY()+ImGui::GetScrollY()-ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y>cardHeight+1)layoutIssues|=8;
    EndCard();
}
// The preview is a lightweight DrawList representation, clipped to its own panel.
// Offset is deliberately omitted so the edited shape stays visible; the overlay uses it.
void Preview(float height) {
    BeginCard("preview",ImVec2(-1,height));Heading("准星预览","居中展示 · 偏移仅作用于屏幕准星");
    ImVec2 top=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();size.x=std::max(size.x,S(10));size.y=std::max(size.y,S(40));
    ImDrawList* draw=ImGui::GetWindowDrawList();ImVec2 bottom(top.x+size.x,top.y+size.y);
    draw->AddRectFilled(top,bottom,Tint(IM_COL32(12,13,17,255)),S(10));
    draw->PushClipRect(top,bottom,true);
    for(float x=top.x;x<bottom.x;x+=S(24))draw->AddLine(ImVec2(x,top.y),ImVec2(x,bottom.y),Tint(IM_COL32(31,33,41,255)));
    for(float y=top.y;y<bottom.y;y+=S(24))draw->AddLine(ImVec2(top.x,y),ImVec2(bottom.x,y),Tint(IM_COL32(31,33,41,255)));
    const ImVec2 center(top.x+size.x*0.5f,top.y+size.y*0.5f);
    const float k=S(static_cast<float>(config.scale)/100.0f),angle=static_cast<float>(config.rotation)*0.01745329252f;
    const float c=std::cos(angle),s=std::sin(angle);
    auto point=[&](float x,float y){return ImVec2(center.x+k*(x*c-y*s),center.y+k*(x*s+y*c));};
    const ImU32 ink=Tint(IM_COL32(config.red,config.green,config.blue,config.opacity)),black=Tint(IM_COL32(0,0,0,config.opacity));
    auto box=[&](float x1,float y1,float x2,float y2){
        if(config.outline){float o=static_cast<float>(config.outlineThickness);ImVec2 p[]={point(x1-o,y1-o),point(x2+o,y1-o),point(x2+o,y2+o),point(x1-o,y2+o)};draw->AddConvexPolyFilled(p,4,black);}
        ImVec2 p[]={point(x1,y1),point(x2,y1),point(x2,y2),point(x1,y2)};draw->AddConvexPolyFilled(p,4,ink);
    };
    if(config.enabled) {
        float t=static_cast<float>(config.thickness),g=static_cast<float>(config.gap),l=static_cast<float>(config.length);
        if(config.cross!=5 && l>0){
            if(config.cross!=1&&config.cross!=3)box(-t/2,-g-l,t/2,-g);
            if(config.cross!=2&&config.cross!=3)box(-t/2,g,t/2,g+l);
            if(config.cross!=4){box(-g-l,-t/2,-g,t/2);box(g,-t/2,g+l,t/2);}
        }
        if(config.dot){float r=static_cast<float>(config.dotSize)*0.5f;
            if(config.shape==0)box(-r,-r,r,r);
            else if(config.shape==1){if(config.outline)draw->AddCircleFilled(center,k*(r+config.outlineThickness),black);draw->AddCircleFilled(center,k*r,ink);}
            else {if(config.outline){float o=r+config.outlineThickness;ImVec2 p[]={point(0,-o),point(o,0),point(0,o),point(-o,0)};draw->AddConvexPolyFilled(p,4,black);}ImVec2 p[]={point(0,-r),point(r,0),point(0,r),point(-r,0)};draw->AddConvexPolyFilled(p,4,ink);}
        }
        if(config.ring){if(config.outline)draw->AddCircle(center,k*config.radius,black,0,std::max(1.0f,k*(config.ringThickness+2*config.outlineThickness)));draw->AddCircle(center,k*config.radius,ink,0,std::max(1.0f,k*config.ringThickness));}
        if(config.ruler){
            std::string entries=Utf8(config.rulerEntries);size_t start=0;int count=0;
            while(start<entries.size()&&count++<32){auto end=entries.find(',',start);if(end==std::string::npos)end=entries.size();auto split=entries.find(':',start);if(split!=std::string::npos&&split<end){int px=0;auto value=entries.substr(split+1,end-split-1);if(Parse(value.c_str(),px,-2000,2000)){draw->AddLine(point(0,0),point(0,static_cast<float>(px)),ink,std::max(1.0f,k));draw->AddLine(point(-6,static_cast<float>(px)),point(7,static_cast<float>(px)),ink,std::max(1.0f,k));auto label=entries.substr(start,split-start);draw->AddText(point(11,static_cast<float>(px)-7),ink,label.c_str());}}start=end+1;}
        }
    } else {const char* label=Tr("准星已暂停");ImVec2 ts=ImGui::CalcTextSize(label);draw->AddText(ImVec2(center.x-ts.x/2,center.y-ts.y/2),Color(muted),label);}
    draw->PopClipRect();ImGui::Dummy(size);EndCard();
}
bool Slider(const char* label,int& value,int low,int high,const char* format="%d") {
    Small(label);ImGui::SetNextItemWidth(-1);std::string id="##";id+=label;return ImGui::SliderInt(id.c_str(),&value,low,high,format,ImGuiSliderFlags_AlwaysClamp);
}
bool SignedInput(const char* label,int& value) {
    Small(label);ImGui::PushID(label);ImGui::SetNextItemWidth(std::max(S(40),ImGui::GetContentRegionAvail().x-S(50)));
    bool changed=ImGui::InputInt("##value",&value,0,0);ImGui::SameLine();if(Button("±",ImVec2(S(40),0))){value=-std::clamp(value,-16000,16000);changed=true;}
    value=std::clamp(value,-16000,16000);ImGui::PopID();return changed;
}
void CrosshairPage() {
    Title("自定义准星","调整即刻应用到独立叠加层；收起计算器后，准星仍持续显示。");
    const float width=ImGui::GetContentRegionAvail().x;const bool columns=width>=S(650);
    const float gap=S(16),leftWidth=columns?(width-gap)*0.55f:width;
    bool changed=false;
    BeginCard("crosshair-controls",ImVec2(leftWidth,S(525)));
    Heading("外观与形状");
    changed|=Check("启用准星",config.enabled);RecordPoint(3);Space(4);changed|=Check("中心点",config.dot);ImGui::SameLine();changed|=Check("圆环",config.ring);
    Space(6);Small("点形状");const char* shapes[]={"方形","圆形","菱形"};changed|=Combo("##dot-shape",config.shape,shapes,3);
    Small("十字类型");const char* crosses[]={"完整十字","T 向下","T 向上","仅左右","仅上下","关闭"};changed|=Combo("##cross-type",config.cross,crosses,6);
    Space(4);Small("准星颜色");float color[]={config.red/255.0f,config.green/255.0f,config.blue/255.0f};ImGui::SetNextItemWidth(-1);
    if(ImGui::ColorEdit3("##color",color,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_DisplayHex)){config.red=static_cast<int>(color[0]*255+0.5f);config.green=static_cast<int>(color[1]*255+0.5f);config.blue=static_cast<int>(color[2]*255+0.5f);changed=true;}
    Space(5);ImGui::Separator();Space(5);
    changed|=Slider("线长度",config.length,0,500);changed|=Slider("线粗细",config.thickness,1,100);
    changed|=Slider("中心间隙",config.gap,-500,500);changed|=Slider("中心点尺寸",config.dotSize,1,200);
    changed|=Slider("圆环半径",config.radius,1,1000);changed|=Slider("圆环粗细",config.ringThickness,1,100);
    Space(5);changed|=Check("黑色描边",config.outline);if(config.outline)changed|=Slider("描边宽度",config.outlineThickness,1,20);
    changed|=Slider("不透明度",config.opacity,0,255);changed|=Slider("旋转角度",config.rotation,0,359,"%d°");
    changed|=Slider("全局缩放",config.scale,10,500,"%d%%");
    Space(5);ImGui::Separator();Space(5);Heading("位置与标尺");
    changed|=SignedInput("水平偏移 / px",config.offsetX);changed|=SignedInput("垂直偏移 / px",config.offsetY);
    Small("显示器");
    monitorNames.clear();for(const auto& name:crosshair::MonitorNames())monitorNames.push_back(Utf8(name));
    if(monitorNames.empty())monitorNames.emplace_back("主显示器");
    std::vector<const char*> monitorLabels;for(const auto& name:monitorNames)monitorLabels.push_back(name.c_str());
    changed|=Combo("##monitor",config.monitor,monitorLabels.data(),static_cast<int>(monitorLabels.size()));
    Space(6);changed|=Check("启用下落标尺",config.ruler);
    if(config.ruler){Small("标签:像素，英文逗号分隔");ImGui::SetNextItemWidth(-1);if(ImGui::InputTextMultiline("##ruler",rulerText,sizeof(rulerText),ImVec2(-1,S(70)))){config.rulerEntries=Wide(rulerText);changed=true;}Font(14);ImGui::TextColored(muted,Tr("例如 100:30,200:70；正数向下。"));ImGui::PopFont();}
    EndCard();
    if(changed){crosshair::ApplyConfig(config,false);config=crosshair::GetConfig();}
    if(columns)ImGui::SameLine(0,gap);else Space(6);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::BeginChild("preview-column",ImVec2(columns?width-leftWidth-gap:width,S(525)),ImGuiChildFlags_NavFlattened,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    Preview(S(281));Space(6);
    BeginCard("preset-card",ImVec2(-1,S(228)));
    Heading("预设与保存");
    if(Button("导入 CS 准星代码",ImVec2(-1,S(32)),true))requestCsDialog=true;
    RecordPoint(14);
    const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*0.5f;
    if(Button("导入预设",ImVec2(half,S(32)))){if(crosshair::ImportPreset(Owner())){SyncConfig();Notify("预设已同步");}}
    ImGui::SameLine();if(Button("导出预设",ImVec2(half,S(32)))){crosshair::ExportPreset(Owner());}
    if(Button("恢复默认",ImVec2(half,S(32)))){config=crosshair::Config{};crosshair::ApplyConfig(config,false);SyncConfig();Notify("已恢复默认准星");}
    ImGui::SameLine();if(Button("保存配置",ImVec2(half,S(32)),true)){if(Save())Notify("配置已保存");else Notify("保存失败，请检查配置目录写入权限");}RecordPoint(4);
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);ImGui::TextWrapped(Tr("CS 代码仅导入静态外观；文件预设为 INI。收起时自动保存。"));ImGui::PopStyleColor();ImGui::PopFont();EndCard();ImGui::EndChild();ImGui::PopStyleVar();
}
void SyncManualHotkey() {
    if(manualKeys.empty()) {
        manualKeys.push_back({VK_OEM_5,"\\"});
        for(UINT key=VK_F1;key<=VK_F24;++key)if(key!=VK_F12)manualKeys.push_back({key,"F"+std::to_string(key-VK_F1+1)});
        for(UINT key='A';key<='Z';++key)manualKeys.push_back({key,std::string(1,static_cast<char>(key))});
        for(UINT key='0';key<='9';++key)manualKeys.push_back({key,std::string(1,static_cast<char>(key))});
        for(UINT key=VK_NUMPAD0;key<=VK_NUMPAD9;++key)manualKeys.push_back({key,"小键盘 "+std::to_string(key-VK_NUMPAD0)});
        const KeyChoice extra[]={{VK_OEM_MINUS,"-"},{VK_OEM_PLUS,"="},{VK_OEM_1,";"},{VK_OEM_2,"/"},{VK_OEM_3,"`"},{VK_OEM_4,"["},{VK_OEM_6,"]"},{VK_OEM_7,"'"},{VK_OEM_COMMA,","},{VK_OEM_PERIOD,"."},{VK_OEM_102,"ISO \\"},{VK_INSERT,"Insert"},{VK_DELETE,"Delete"},{VK_HOME,"Home"},{VK_END,"End"},{VK_PRIOR,"Page Up"},{VK_NEXT,"Page Down"},{VK_LEFT,"左方向键"},{VK_RIGHT,"右方向键"},{VK_UP,"上方向键"},{VK_DOWN,"下方向键"},{VK_BACK,"Backspace"},{VK_ADD,"小键盘 +"},{VK_SUBTRACT,"小键盘 -"},{VK_MULTIPLY,"小键盘 *"},{VK_DIVIDE,"小键盘 /"},{VK_DECIMAL,"小键盘 ."}};
        for(const auto& choice:extra)manualKeys.push_back(choice);
    }
    manualLoadedKey=hotkey::Key();manualLoadedMods=hotkey::Modifiers();manualKeyIndex=0;
    for(size_t i=0;i<manualKeys.size();++i)if(manualKeys[i].key==manualLoadedKey)manualKeyIndex=static_cast<int>(i);
    manualCtrl=(manualLoadedMods&MOD_CONTROL)!=0;manualAlt=(manualLoadedMods&MOD_ALT)!=0;
    manualShift=(manualLoadedMods&MOD_SHIFT)!=0;manualWin=(manualLoadedMods&MOD_WIN)!=0;
}
void HotkeyPage() {
    if(manualKeys.empty()||manualLoadedKey!=hotkey::Key()||manualLoadedMods!=hotkey::Modifiers())SyncManualHotkey();
    Title("快捷键设置","自定义计算器呼出 / 收起按键，准星始终独立显示。");
    BeginCard("hotkey-card",ImVec2(-1,std::max(S(500),ImGui::GetContentRegionAvail().y)));
    Heading("全局快捷键");Small("当前绑定");
    Font(24);ImGui::TextColored(accent,"%s",Utf8(hotkey::Label()).c_str());ImGui::PopFont();Space(8);
    if(Button(hotkey::IsCapturing()?"正在录制：请按快捷键…":"点击录制新快捷键",ImVec2(-1,S(36)),true,"record-hotkey"))hotkey::BeginCapture();
    RecordPoint(20);Space(6);
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,hotkey::HasError()?caution:muted);
    ImGui::TextWrapped("%s",i18n::Translate(hotkey::Status()).c_str());ImGui::PopStyleColor();ImGui::PopFont();Space(8);
    const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*0.5f;
    ImGui::BeginDisabled(!hotkey::IsCapturing());
    if(Button("取消录制",ImVec2(half,S(32))))hotkey::CancelCapture();RecordPoint(21);
    ImGui::EndDisabled();ImGui::SameLine();
    if(Button("恢复默认  \\",ImVec2(half,S(32)))){if(hotkey::ResetDefault())SyncManualHotkey();}RecordPoint(22);
    Space(10);ImGui::Separator();Space(8);Small("也可手动选择（按键被系统拦截时使用）");
    Font(14);ImGui::Checkbox("Ctrl",&manualCtrl);RecordPoint(24);ImGui::SameLine();
    ImGui::Checkbox("Alt",&manualAlt);RecordPoint(25);ImGui::SameLine();
    ImGui::Checkbox("Shift",&manualShift);ImGui::SameLine();ImGui::Checkbox("Win",&manualWin);ImGui::PopFont();
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##manual-hotkey",i18n::Translate(manualKeys[static_cast<size_t>(manualKeyIndex)].label).c_str(),ImGuiComboFlags_HeightLarge)) {
        for(size_t i=0;i<manualKeys.size();++i) {
            if(ImGui::Selectable(StableLabel(manualKeys[i].label.c_str()).c_str(),manualKeyIndex==static_cast<int>(i),0,ImVec2(0,S(30))))manualKeyIndex=static_cast<int>(i);
            if(manualKeys[i].key==VK_F9)RecordPoint(26);
            if(manualKeyIndex==static_cast<int>(i))ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    RecordPoint(28);
    if(Button("应用所选快捷键",ImVec2(-1,S(32)),true)) {
        hotkey::CancelCapture();
        const UINT mods=(manualCtrl?MOD_CONTROL:0u)|(manualAlt?MOD_ALT:0u)|(manualShift?MOD_SHIFT:0u)|(manualWin?MOD_WIN:0u);
        hotkey::Set(manualKeys[static_cast<size_t>(manualKeyIndex)].key,mods);
    }
    RecordPoint(23);Space(8);Font(14);
    if(ImGui::CollapsingHeader(StableLabel("快捷键使用说明").c_str())) {
        ImGui::TextWrapped(Tr("录制时先按住修饰键，再按主键；单独按修饰键不会提交，Esc 取消。F12 和系统保留组合不可用。若系统拦截按键无法录制，请使用手动选择。"));Space(6);
        ImGui::TextWrapped(Tr("设置立即生效并自动保存；冲突或保存失败保留原绑定。默认反斜杠 \\，录制用的那次按键不会收起窗口。请避免占用游戏及文本编辑常用键，仍可用托盘打开界面。"));
    }
    if(!hotkey::IsRegistered())ImGui::TextColored(caution,Tr("当前没有注册成功的快捷键，请选择其他按键。"));
    ImGui::PopFont();EndCard();
}
bool UpdateBusy(updater::Phase phase) {
    return phase==updater::Phase::Checking||phase==updater::Phase::Downloading||phase==updater::Phase::Launching;
}
const char* UpdatePhaseText(updater::Phase phase) {
    switch(phase) {
    case updater::Phase::Checking:return "正在检查更新…";
    case updater::Phase::UpToDate:return "已是最新版本";
    case updater::Phase::Available:return "发现新版本";
    case updater::Phase::Downloading:return "正在下载更新…";
    case updater::Phase::Ready:return "更新已准备就绪";
    case updater::Phase::Launching:return "正在启动更新…";
    case updater::Phase::Error:return "更新失败";
    case updater::Phase::Cancelled:return "更新已取消";
    default:return "尚未检查更新";
    }
}
void UpdateConfirmation(Action& action) {
    if(cancelInstallConfirmation)requestInstallConfirmation=false;
    const auto title=StableLabel("确认安装更新","install-update-modal");
    if(requestInstallConfirmation&&!cancelInstallConfirmation){ImGui::OpenPopup(title.c_str());requestInstallConfirmation=false;}
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x*0.5f,display.y*0.5f),ImGuiCond_Appearing,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(S(500),display.x-S(32)),0),ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(20),S(18)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.09f,0.10f,0.12f,1));
    bool open=true;
    if(ImGui::BeginPopupModal(title.c_str(),&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize)) {
        const auto snapshot=updater::GetSnapshot();
        if(cancelInstallConfirmation||snapshot.phase!=updater::Phase::Ready)ImGui::CloseCurrentPopup();
        else {
            ImGui::TextWrapped(Tr("确认安装已下载的更新并重启？当前设置会先保存。"));Space(8);
            ImGui::Text("%s  %s",Tr("当前版本"),snapshot.currentVersion.c_str());
            ImGui::Text("%s  %s",Tr("最新版本"),snapshot.latestVersion.c_str());Space(12);
            const float half=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*0.5f;
            if(Button("确认安装并重启",ImVec2(half,S(36)),true)){action=Action::InstallUpdate;ImGui::CloseCurrentPopup();}RecordPoint(38);
            ImGui::SameLine();if(Button("取消",ImVec2(half,S(36))))ImGui::CloseCurrentPopup();RecordPoint(39);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();ImGui::PopStyleVar();cancelInstallConfirmation=false;
}
void SettingsPage() {
    Title("设置与更新","自动保存的界面选项。");
    BeginCard("settings-card",ImVec2(-1,std::max(S(500),ImGui::GetContentRegionAvail().y)));
    Heading("界面语言");
    int language=i18n::Current()==i18n::Language::English?1:0;
    const char* languages[]={"中文","English"};
    if(Combo("##language",language,languages,2)) {
        if(!i18n::SetLanguage(language==1?i18n::Language::English:i18n::Language::Chinese))Notify("语言设置保存失败");
    }
    RecordPoint(31);Space(8);
    int enabled=animations?1:0;
    if(Check("启用非线性动画",enabled)) {
        const bool next=enabled!=0;
        if(WritePrivateProfileStringW(L"Appearance",L"Animations",next?L"1":L"0",configPath.c_str())) {
            animations=next;motionStates.clear();pageTransitionStarted=ImGui::GetTime();
        }else Notify("界面设置保存失败");
    }
    RecordPoint(32);Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);
    ImGui::TextWrapped(Tr("页面和按钮动效可随时关闭，窗口动画使用同一设置。"));
    ImGui::PopStyleColor();ImGui::PopFont();Space(12);ImGui::Separator();Space(10);
    Heading("应用更新");
    const auto snapshot=updater::GetSnapshot();
    const auto current=snapshot.currentVersion.empty()?std::string(HLLCALC_VERSION_STRING):snapshot.currentVersion;
    ImGui::Text("%s  %s",Tr("当前版本"),current.c_str());
    if(!snapshot.latestVersion.empty())ImGui::Text("%s  %s",Tr("最新版本"),snapshot.latestVersion.c_str());
    ImGui::TextColored(snapshot.phase==updater::Phase::Error?caution:muted,"%s",Tr(UpdatePhaseText(snapshot.phase)));Space(8);
    ImGui::BeginDisabled(UpdateBusy(snapshot.phase));
    if(Button("检查更新",ImVec2(-1,S(34)),true))updater::CheckAsync();RecordPoint(33);ImGui::EndDisabled();
    if(snapshot.phase==updater::Phase::Available) {
        if(Button("下载更新",ImVec2(-1,S(34)),true))updater::DownloadAsync();RecordPoint(34);
    }
    if(snapshot.phase==updater::Phase::Downloading) {
        const float progress=snapshot.total?static_cast<float>(std::clamp(static_cast<double>(snapshot.downloaded)/static_cast<double>(snapshot.total),0.0,1.0)):0;
        const auto count=std::to_string(snapshot.downloaded/1024)+" / "+(snapshot.total?std::to_string(snapshot.total/1024):std::string("?"))+" KB";
        ImGui::ProgressBar(progress,ImVec2(-1,S(22)),count.c_str());
    }
    if(snapshot.phase==updater::Phase::Downloading||snapshot.phase==updater::Phase::Checking) {
        if(Button(snapshot.phase==updater::Phase::Checking?"取消检查":"取消下载",ImVec2(-1,S(32)),false,"cancel-update"))updater::Cancel();RecordPoint(36);
    }
    if(snapshot.phase==updater::Phase::Ready) {
        if(Button("安装并重启",ImVec2(-1,S(34)),true))requestInstallConfirmation=true;RecordPoint(35);
    }
    if(!snapshot.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text,caution);ImGui::TextWrapped("%s",i18n::Translate(snapshot.error).c_str());ImGui::PopStyleColor();
    }
    if(!snapshot.releaseUrl.empty()) {
        if(Button("打开发布页面",ImVec2(-1,S(32)))) {
            if(snapshot.releaseUrl.rfind("https://github.com/",0)==0) {
                const auto url=Wide(snapshot.releaseUrl.c_str());
                if(reinterpret_cast<INT_PTR>(ShellExecuteW(Owner(),L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)Notify("无法打开发布页面");
            }else Notify("发布页面地址无效");
        }
        RecordPoint(37);
    }
    Space(8);Font(14);ImGui::PushStyleColor(ImGuiCol_Text,muted);
    ImGui::TextWrapped(Tr("默认不自动下载或安装。更新完成后需确认安装并重启。"));ImGui::PopStyleColor();ImGui::PopFont();
    if(!snapshot.notes.empty()&&ImGui::CollapsingHeader(StableLabel("发行说明").c_str()))ImGui::TextWrapped("%s",snapshot.notes.c_str());
    EndCard();
}

void HelpPage() {
    Title("使用说明","专注于手动计算与独立准星，不读取、不注入、不修改游戏进程。");
    BeginCard("help-card",ImVec2(-1,S(515)));
    Heading("一键呼出，随时回到游戏");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::BulletText(Tr("当前快捷键：%s，呼出或收起计算器。"),Utf8(hotkey::Label()).c_str());
    ImGui::BulletText(Tr("可在左侧“快捷键设置”录制新绑定，默认是反斜杠 \\。"));
    ImGui::BulletText(Tr("呼出后直接输入距离；收起后归还原窗口焦点。"));
    ImGui::BulletText(Tr("右上角最小化与关闭均为收起，准星不会消失。"));
    ImGui::BulletText(Tr("彻底关闭请使用左下角“退出程序”。"));
    ImGui::PopTextWrapPos();
    Space(10);ImGui::Separator();Space(10);Heading("弹道与单位");
    ImGui::TextWrapped(Tr("支持德、美、苏、英四国。SPG 使用距离与车体倾角，STD 只使用距离。倾角沿用原公式的 MIL 单位，不是角度 °。负数可通过 ± 按钮或粘贴输入。"));Space(6);
    ImGui::TextWrapped(Tr("常规火炮射程上限 1600 米；SPG 射角范围为 -89 至 466 MIL，英军上限 356 MIL。参考表按目标距离前后 15 米生成，每行 3 米。"));
    Space(10);ImGui::Separator();Space(10);Heading("窗口模式与兼容性");
    ImGui::PushStyleColor(ImGuiCol_Text,caution);ImGui::TextWrapped(Tr("建议使用无边框窗口模式。普通置顶窗口无法保证覆盖独占全屏，也不保证所有游戏输入捕获模式下的焦点切换。"));ImGui::PopStyleColor();Space(6);
    ImGui::TextWrapped(Tr("准星为鼠标穿透的独立桌面窗口。请遵守游戏及服务器规则；本工具不承诺反作弊兼容性。参数沿用 AimerHLL v6，请自行核对游戏版本变化。"));
    Space(10);Small("HLLCalc " HLLCALC_VERSION_STRING " · C++17 / Dear ImGui / Win32 · GPL-3.0-only");EndCard();
}
void NavIcon(ImDrawList* draw,ImVec2 p,int icon,ImU32 color) {
    float r=S(6);if(icon==0){draw->AddRect(ImVec2(p.x-r,p.y-r),ImVec2(p.x+r,p.y+r),color,S(2),0,S(1.4f));draw->AddLine(ImVec2(p.x-r/2,p.y),ImVec2(p.x+r/2,p.y),color,S(1.4f));}
    else if(icon==1){draw->AddCircle(p,r,color,20,S(1.4f));draw->AddLine(ImVec2(p.x-r-S(3),p.y),ImVec2(p.x+r+S(3),p.y),color,S(1.2f));draw->AddLine(ImVec2(p.x,p.y-r-S(3)),ImVec2(p.x,p.y+r+S(3)),color,S(1.2f));}
    else {draw->AddCircle(p,r,color,20,S(1.4f));draw->AddLine(ImVec2(p.x,p.y-S(1)),ImVec2(p.x,p.y+S(3)),color,S(1.4f));draw->AddCircleFilled(ImVec2(p.x,p.y-S(3)),S(1),color);}
}
void Sidebar(float width,float height,Action& action) {
    ImGui::SetCursorPos(ImVec2(0,S(48)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(14),S(14)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
    ImGui::BeginChild("sidebar",ImVec2(width,height-S(48)),
        ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_NavFlattened,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPosX(S(20));Font(14);ImGui::TextColored(muted,"%s",Tr("战术工具 / TACTICAL"));ImGui::PopFont();
    const char* names[]={"火炮计算","自定义准星","使用说明","快捷键设置","设置与更新"};
    for(int i=0;i<5;++i) {
        ImGui::SetCursorPos(ImVec2(S(14),S(60.0f+static_cast<float>(i)*46.0f)));ImGui::PushID(i);
        const auto pos=ImGui::GetCursorScreenPos();const bool selected=page==i;
        ImGui::PushStyleColor(ImGuiCol_Button,selected?ImVec4(0.14f,0.18f,0.28f,1):ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,selected?ImVec4(0.18f,0.23f,0.34f,1):ImVec4(0.15f,0.16f,0.18f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.20f,0.25f,0.36f,1));
        if(Button("##navigation",ImVec2(width-S(28),S(40)))){if(page!=i)hotkey::CancelCapture();page=i;distanceActive=false;}
        RecordPoint(i<3?i:(i==3?19:29));ImGui::PopStyleColor(3);
        const auto sideOrigin=ImGui::GetWindowPos();
        if(pos.x-sideOrigin.x<S(13)||pos.x+width-S(28)>sideOrigin.x+width-S(13))layoutIssues|=32;
        auto* draw=ImGui::GetWindowDrawList();NavIcon(draw,ImVec2(pos.x+S(18),pos.y+S(20)),i,Color(selected?accent:muted));
        Font(17);const char* label=Tr(names[i]);auto labelSize=ImGui::CalcTextSize(label);
        const float labelWidth=width-S(72);
        if(labelSize.x>labelWidth){const float fit=std::max(14.0f,17.0f*labelWidth/labelSize.x);ImGui::PopFont();Font(fit);labelSize=ImGui::CalcTextSize(label);}
        draw->PushClipRect(ImVec2(pos.x+S(36),pos.y),ImVec2(pos.x+width-S(34),pos.y+S(40)),true);
        draw->AddText(ImVec2(pos.x+S(38),pos.y+(S(40)-labelSize.y)*0.5f),Color(selected?text:muted),label);draw->PopClipRect();ImGui::PopFont();
        ImGui::PopID();
    }
    const float footer=std::max(S(300),height-S(48)-S(172));
    ImGui::SetCursorPos(ImVec2(S(14),footer));ImGui::Separator();Space(8);
    Font(14);ImGui::TextColored(config.enabled?success:muted,"%s",Tr(config.enabled?"●  准星运行中":"○  准星已暂停"));ImGui::PopFont();
    Font(12);ImGui::PushStyleColor(ImGuiCol_Text,muted);ImGui::TextWrapped(Tr("[%s] 呼出 / 收起"),Utf8(hotkey::Label()).c_str());ImGui::PopStyleColor();ImGui::PopFont();Space(8);
    if(Button("退出程序",ImVec2(width-S(28),S(32))))action=Action::Quit;
    Space(5);Font(14);ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    if(Button("v" HLLCALC_VERSION_STRING,ImVec2(width-S(28),S(26)),false,"sidebar-version")){hotkey::CancelCapture();page=4;distanceActive=false;}
    RecordPoint(30);ImGui::PopStyleColor();ImGui::PopFont();
    ImGui::EndChild();ImGui::PopStyleVar(2);
}
void WindowButtons(float width,Action& action) {
    auto button=[&](const char* id,bool close){
        const auto pos=ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,close?ImVec4(0.41f,0.16f,0.21f,1):ImVec4(0.16f,0.16f,0.18f,1));
        if(Button(id,ImVec2(S(32),S(30))))action=Action::Hide;ImGui::PopStyleColor(2);
        auto* draw=ImGui::GetWindowDrawList();const ImVec2 c(pos.x+S(16),pos.y+S(15));
        if(close){draw->AddLine(ImVec2(c.x-S(4),c.y-S(4)),ImVec2(c.x+S(4),c.y+S(4)),Color(text),S(1.4f));draw->AddLine(ImVec2(c.x-S(4),c.y+S(4)),ImVec2(c.x+S(4),c.y-S(4)),Color(text),S(1.4f));}
        else draw->AddLine(ImVec2(c.x-S(5),c.y),ImVec2(c.x+S(5),c.y),Color(text),S(1.4f));
    };
    ImGui::SetCursorPos(ImVec2(width-S(82),S(9)));button("##minimize",false);ImGui::SameLine(0,S(6));button("##close",true);RecordPoint(5);
}

}
bool Initialize(const std::wstring& iniPath,float uiScale) {
    if(!ImGui::GetCurrentContext() || iniPath.empty())return false;
    configPath=iniPath;scale=std::clamp(uiScale,0.5f,4.0f);page=0;focusDistance=true;distanceActive=false;
    nation=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Calculator",L"Nation",0,configPath.c_str())),0,3);
    mode=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Calculator",L"Mode",0,configPath.c_str())),0,1);
    wchar_t buffer[64]{};GetPrivateProfileStringW(L"Calculator",L"Distance",L"500",buffer,64,configPath.c_str());Copy(distanceText,sizeof(distanceText),Utf8(buffer));
    GetPrivateProfileStringW(L"Calculator",L"Inclination",L"0",buffer,64,configPath.c_str());Copy(inclinationText,sizeof(inclinationText),Utf8(buffer));
    SyncConfig();
    animations=GetPrivateProfileIntW(L"Appearance",L"Animations",1,configPath.c_str())!=0;
    motionStates.clear();transitionPage=-1;
    // Start from an unscaled style every time; never multiply a previously scaled style.
    ImGuiStyle style;ImGui::StyleColorsDark(&style);
    style.WindowPadding=ImVec2(16,14);style.FramePadding=ImVec2(10,6);style.ItemSpacing=ImVec2(8,5);style.ItemInnerSpacing=ImVec2(8,5);
    style.CellPadding=ImVec2(12,7);style.SelectableTextAlign=ImVec2(0,0.5f);style.ButtonTextAlign=ImVec2(0.5f,0.5f);
    style.WindowRounding=12;style.ChildRounding=10;style.FrameRounding=6;style.PopupRounding=8;style.ScrollbarRounding=8;
    style.WindowBorderSize=0;style.ChildBorderSize=1;style.FrameBorderSize=0;style.PopupBorderSize=1;style.ScrollbarSize=9;style.GrabMinSize=12;
    style.Colors[ImGuiCol_Text]=text;style.Colors[ImGuiCol_TextDisabled]=muted;style.Colors[ImGuiCol_WindowBg]=ImVec4(0.039f,0.039f,0.039f,1);
    style.Colors[ImGuiCol_ChildBg]=ImVec4(0,0,0,0);style.Colors[ImGuiCol_PopupBg]=ImVec4(0.10f,0.10f,0.10f,1);
    style.Colors[ImGuiCol_NavCursor]=ImVec4(0.39f,0.56f,1.0f,0.65f);
    style.Colors[ImGuiCol_Border]=ImVec4(0.16f,0.16f,0.16f,1);style.Colors[ImGuiCol_Separator]=style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_FrameBg]=ImVec4(0.15f,0.15f,0.15f,1);style.Colors[ImGuiCol_FrameBgHovered]=ImVec4(0.20f,0.20f,0.22f,1);style.Colors[ImGuiCol_FrameBgActive]=ImVec4(0.20f,0.23f,0.31f,1);
    style.Colors[ImGuiCol_Button]=ImVec4(0.15f,0.15f,0.15f,1);style.Colors[ImGuiCol_ButtonHovered]=ImVec4(0.23f,0.23f,0.26f,1);style.Colors[ImGuiCol_ButtonActive]=ImVec4(0.20f,0.25f,0.38f,1);
    style.Colors[ImGuiCol_Header]=ImVec4(0.19f,0.25f,0.40f,1);style.Colors[ImGuiCol_HeaderHovered]=ImVec4(0.23f,0.30f,0.47f,1);style.Colors[ImGuiCol_HeaderActive]=ImVec4(0.28f,0.36f,0.55f,1);
    style.Colors[ImGuiCol_CheckMark]=accent;style.Colors[ImGuiCol_SliderGrab]=accent;style.Colors[ImGuiCol_SliderGrabActive]=purple;style.Colors[ImGuiCol_TextSelectedBg]=ImVec4(0.31f,0.43f,0.74f,0.5f);
    style.Colors[ImGuiCol_TableHeaderBg]=ImVec4(0.12f,0.12f,0.13f,1);style.ScaleAllSizes(scale);ImGui::GetStyle()=style;
    initialized=true;return true;
}
bool Save() {
    if(!initialized)return false;
    bool ok=true;
    auto put=[&](const wchar_t* key,const std::wstring& value){if(!WritePrivateProfileStringW(L"Calculator",key,value.c_str(),configPath.c_str()))ok=false;};
    put(L"Nation",std::to_wstring(nation));put(L"Mode",std::to_wstring(mode));
    put(L"Distance",Wide(distanceText));put(L"Inclination",Wide(inclinationText));
    if(!crosshair::SaveConfig())ok=false;
    if(!WritePrivateProfileStringW(L"Appearance",L"Animations",animations?L"1":L"0",configPath.c_str()))ok=false;
    return ok;
}
void Shutdown() {if(initialized)Save();initialized=false;distanceActive=false;monitorNames.clear();motionStates.clear();configPath.clear();}
void FocusDistance() {page=0;focusDistance=true;distanceActive=false;cancelCsDialog=true;cancelInstallConfirmation=true;}
void OpenCrosshair() {page=1;distanceActive=false;SyncConfig();}
int DistanceValue() {int value=0;return Parse(distanceText,value,1,99999)?value:0;}
bool DistanceFocused() {return distanceActive;}
int CurrentPage() {return page;}
int NationValue() {return nation;}
bool CsDialogVisible() {return csDialogVisible;}
bool CsImportFailed() {return !csCodeError.empty();}
int LayoutIssues() {return layoutIssues;}
bool AnimationsEnabled() {return MotionEnabled();}
void SetAnimationSuppressed(bool value) {animationSuppressed=value;if(value){motionStates.clear();transitionPage=page;}}
bool TestWidgetPoint(int id,int& x,int& y) {
    if(id<0||id>=42||!testPoints[id].valid)return false;
    x=testPoints[id].x;y=testPoints[id].y;return true;
}
Action Frame() {
    if(!initialized)return Action::None;
    Action action=Action::None;distanceActive=false;layoutIssues=0;for(auto& point:testPoints)point.valid=false;
    const ImVec2 display=ImGui::GetIO().DisplaySize;const float width=std::max(display.x,1.0f),height=std::max(display.y,1.0f),side=S(208);
    ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(width,height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("HLLCalc shell",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    CsCodeDialog();UpdateConfirmation(action);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(0,0),ImVec2(side,height),IM_COL32(23,23,23,255),S(14),ImDrawFlags_RoundCornersLeft);
    draw->AddLine(ImVec2(side,S(48)),ImVec2(side,height),IM_COL32(38,38,38,255));draw->AddLine(ImVec2(side,S(48)),ImVec2(width,S(48)),IM_COL32(38,38,38,255));
    ImGui::SetCursorPos(ImVec2(S(22),S(14)));Font(18);ImGui::TextUnformatted("HLL CALC");ImGui::PopFont();
    if(width>side+S(300)){ImGui::SetCursorPos(ImVec2(side+S(24),S(17)));Font(14);Small("TACTICAL UTILITY  /  原生桌面工具");ImGui::PopFont();}
    WindowButtons(width,action);Sidebar(side,height,action);
    ImGui::SetCursorPos(ImVec2(side,S(48)));
    if(page!=transitionPage){transitionPage=page;pageTransitionStarted=ImGui::GetTime();}
    const float progress=MotionEnabled()?std::clamp(static_cast<float>((ImGui::GetTime()-pageTransitionStarted)/0.18),0.0f,1.0f):1.0f;
    const float eased=1.0f-(1.0f-progress)*(1.0f-progress)*(1.0f-progress);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*(0.35f+0.65f*eased));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(20),S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
    ImGui::BeginChild("page-content",ImVec2(std::max(S(50),width-side),std::max(S(50),height-S(48))),
        ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_NavFlattened,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(); // Cards use their own rounding, not the structural container's.
    if(focusDistance&&page==0)ImGui::SetScrollY(0);
    if(page==0)Calculator(action);else if(page==1)CrosshairPage();else if(page==2)HelpPage();else if(page==3)HotkeyPage();else SettingsPage();
    if(ImGui::GetTime()<statusUntil){Space(6);ImGui::TextColored(success,"%s",i18n::Translate(statusText).c_str());}
    ImGui::EndChild();ImGui::PopStyleVar(2);ImGui::End();ImGui::PopStyleVar();return action;
}
}
