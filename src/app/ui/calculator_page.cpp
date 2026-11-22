// SPDX-License-Identifier: GPL-3.0-only
#include "app/ui/pages.h"
#include "app/ui/widgets.h"
#include "ballistics/ballistics.h"
#include "input/hotkey.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui::detail {
namespace {
const char* LimitText(ballistics::Status status) {
    switch(status) {
    case ballistics::Status::RangeExceeded:return "超过最大射程 · 常规火炮上限 1600 米";
    case ballistics::Status::ElevationExceeded:return "超出射角限制 · 请调整距离或车体倾角";
    case ballistics::Status::InvalidDistance:return "请输入有效的距离及整数倾角";
    default:return "参数在允许范围内";
    }
}
void Negate(char* buffer,size_t capacity) {
    int value=0;
    if(Parse(buffer,value,-9999,9999,true))std::snprintf(buffer,capacity,"%d",-value);
}
// Content must fit the card; on short windows the page scrolls as a whole.
void CheckCardFit(float cardHeight,int issue) {
    const auto& style=ImGui::GetStyle();
    if(ImGui::GetCursorPosY()+ImGui::GetScrollY()-style.ItemSpacing.y+style.WindowPadding.y>cardHeight+1)Flag(issue);
}
struct Inputs { bool valid; int distance,inclination; ballistics::Nation nation; ballistics::Mode mode; };

void InputCard(Action& action,float width,float height,Inputs& in) {
    BeginCard("input-card",ImVec2(width,height));
    Heading("射击参数");
    Small("阵营");
    const char* nations[]={"德军 / GER","美军 / US","苏军 / USSR","英军 / UK"};
    Combo("##nation",calc.nation,nations,4,TestNationOption);
    RecordPoint(TestNation);

    Space(6);Small("火炮类型");
    const char* modes[]={"自行火炮 SPG","常规火炮 STD"};
    Font(14);Segmented("mode",calc.mode,modes,2,calc.modeIndicator,S(36));ImGui::PopFont();

    Space(8);Small("目标距离 / 米");
    if(calc.focusDistance){ImGui::SetScrollY(0);ImGui::SetKeyboardFocusHere();}
    Font(20);ImGui::SetNextItemWidth(-1);
    bool selectOnFocus=calc.focusDistance;
    auto selectAll=[](ImGuiInputTextCallbackData* data)->int {
        if(*static_cast<bool*>(data->UserData)){data->SelectAll();data->CursorPos=data->BufTextLen;}
        return 0;
    };
    ImGui::InputText("##distance",calc.distance,sizeof(calc.distance),
        ImGuiInputTextFlags_CharsDecimal|ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_CallbackAlways,selectAll,&selectOnFocus);
    RecordPoint(TestDistance);
    calc.distanceActive=ImGui::IsItemActive();calc.focusDistance=false;
    ImGui::PopFont();

    Space(6);Small("车体倾角 / MIL");
    ImGui::BeginDisabled(calc.mode==1);
    Font(20);
    const float fieldHeight=ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(std::max(S(50),ImGui::GetContentRegionAvail().x-S(42)-ImGui::GetStyle().ItemSpacing.x));
    ImGui::InputText("##inclination",calc.inclination,sizeof(calc.inclination),ImGuiInputTextFlags_CharsDecimal|ImGuiInputTextFlags_AutoSelectAll);
    const auto fieldMin=ImGui::GetItemRectMin(),fieldMax=ImGui::GetItemRectMax();
    ImGui::SameLine();
    if(Button("±##inclination",ImVec2(S(42),fieldHeight)))Negate(calc.inclination,sizeof(calc.inclination));
    RecordPoint(TestInclinationSign);
    if(std::abs(fieldMin.y-ImGui::GetItemRectMin().y)>0.5f||std::abs(fieldMax.y-ImGui::GetItemRectMax().y)>0.5f)Flag(IssueFieldAlign);
    ImGui::PopFont();
    ImGui::EndDisabled();
    Muted(hotkey::Key()==VK_OEM_MINUS&&hotkey::Modifiers()==0?"减号当前用作快捷键，负值请用 ± 或粘贴。":"负值可直接输入，也可使用 ± 按钮。");

    Divider();
    in.nation=static_cast<ballistics::Nation>(calc.nation);
    in.mode=calc.mode==0?ballistics::Mode::SPG:ballistics::Mode::Standard;
    in.distance=in.inclination=0;
    in.valid=Parse(calc.distance,in.distance,1,99999)&&(calc.mode==1||Parse(calc.inclination,in.inclination,-9999,9999,true));
    const auto result=ballistics::Calculate(in.nation,in.mode,in.valid?in.distance:0,in.inclination);
    const bool ok=result.status==ballistics::Status::Valid;

    Small("建议射击参数");
    const auto value=in.valid?std::to_string(result.mil):std::string("—");
    if(value!=calc.lastResult){if(!calc.lastResult.empty())calc.resultChanged=ImGui::GetTime();calc.lastResult=value;}
    // A new firing solution briefly brightens so it is noticed at a glance.
    const float flash=MotionEnabled()?1.0f-EaseSince(calc.resultChanged,0.35):0.0f;
    ResultLine(value.c_str(),Mix(ok?kAccent:kCaution,kText,0.55f*flash));
    Font(14);ImGui::PushStyleColor(ImGuiCol_Text,ok?kSuccess:kCaution);
    ImGui::TextWrapped("%s",Tr(LimitText(result.status)));
    ImGui::PopStyleColor();ImGui::PopFont();

    Space(8);
    if(Button("收起计算器",ImVec2(-1,S(36)),true))action=Action::Hide;
    RecordPoint(TestHide);
    CheckCardFit(height,IssueInputOverflow);
    EndCard();
}

void ReferenceCard(float width,float height,const Inputs& in) {
    BeginCard("reference-card",ImVec2(width,height));
    Heading("距离修正表","11 个参考点 · 每行相差 3 米");
    const auto rows=ballistics::ReferenceTable(in.nation,in.mode,in.valid?in.distance:0,in.inclination);
    Font(15);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(S(12),S(7)));
    const float tableLeft=ImGui::GetCursorScreenPos().x;
    if(ImGui::BeginTable("range-table",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn(Tr("距离 / m"));
        ImGui::TableSetupColumn(Tr("射击参数 / MIL"));
        ImGui::TableHeadersRow();
        // The target row pulses with the result flash, tying the two cards together.
        const float flash=MotionEnabled()?1.0f-EaseSince(calc.resultChanged,0.45):0.0f;
        const ImVec4 targetRow=Mix(ImVec4(37/255.0f,49/255.0f,78/255.0f,1),ImVec4(52/255.0f,70/255.0f,112/255.0f,1),flash);
        for(int i=0;i<11;++i) {
            const bool target=i==5;
            const bool invalid=rows[i].status==ballistics::Status::InvalidDistance;
            ImGui::TableNextRow(ImGuiTableRowFlags_None,S(31));
            if(target)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,Color(targetRow));
            ImGui::PushStyleColor(ImGuiCol_Text,target?kAccent:(rows[i].status!=ballistics::Status::Valid?kMuted:kText));
            ImGui::TableSetColumnIndex(0);
            if(ImGui::GetCursorScreenPos().x-tableLeft<S(10))Flag(IssueTablePadding);
            if(invalid)ImGui::TextUnformatted("—");
            else ImGui::Text(Tr(target?"%d  ·  目标":"%d"),rows[i].distance);
            ImGui::TableSetColumnIndex(1);
            if(invalid)ImGui::TextUnformatted("—");
            else ImGui::Text("%d",rows[i].mil);
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();ImGui::PopFont();
    Space(10);
    Muted("高亮行为当前目标，请以实际游戏版本及炮位为准。");
    CheckCardFit(height,IssueTableOverflow);
    EndCard();
}
}

void CalculatorPage(Action& action) {
    Title("火炮计算","手动输入目标距离，快速获取射击参数与邻近距离修正。");
    const auto available=ImGui::GetContentRegionAvail();
    const bool columns=available.x>=S(680);
    const float gap=S(12),cardHeight=S(500);
    const float leftWidth=columns?(available.x-gap)*0.49f:available.x;
    Inputs in{};
    InputCard(action,leftWidth,cardHeight,in);
    if(columns)ImGui::SameLine(0,gap);else Space(12);
    ReferenceCard(columns?available.x-leftWidth-gap:available.x,cardHeight,in);
}
}
