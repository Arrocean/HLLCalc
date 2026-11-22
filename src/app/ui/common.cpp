// SPDX-License-Identifier: GPL-3.0-only
#include "app/ui/common.h"
#include "i18n/i18n.h"
#include "input/hotkey.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace ui::detail {
float S(float n) {return n*app.scale;}
const char* Tr(const char* key) {return i18n::Tr(key);}
std::string VisibleLabel(const char* source) {
    const char* hidden=std::strstr(source,"##");
    std::string visible(source,hidden?static_cast<size_t>(hidden-source):std::strlen(source));
    return i18n::Translate(Tr(visible.c_str()));
}
std::string StableLabel(const char* source,const char* id) {
    return VisibleLabel(source)+"###"+(id?id:source);
}
bool MotionEnabled() {return app.animations&&!app.suppressed;}
ImVec4 Mix(ImVec4 a,ImVec4 b,float t) {
    return ImVec4(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t);
}
ImVec4 WithAlpha(ImVec4 c,float alpha) {c.w*=alpha;return c;}
ImU32 Color(ImVec4 c) {c.w*=ImGui::GetStyle().Alpha;return ImGui::ColorConvertFloat4ToU32(c);}
ImU32 Tint(ImU32 c) {
    const auto alpha=static_cast<unsigned>(static_cast<float>((c>>24)&255u)*ImGui::GetStyle().Alpha);
    return (c&0x00ffffffu)|(alpha<<24);
}
float EaseSince(double started,double seconds) {
    if(!MotionEnabled())return 1.0f;
    return motion::EaseOutCubic(static_cast<float>((ImGui::GetTime()-started)/seconds));
}
float Glide::Update(float target,float response) {
    if(!ready||!MotionEnabled()){spring.Snap(target);ready=true;}
    else spring.Step(target,ImGui::GetIO().DeltaTime,response);
    return spring.value;
}
std::string Utf8(const std::wstring& value) {
    if(value.empty())return {};
    const int n=WideCharToMultiByte(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(static_cast<size_t>(n),'\0');
    if(n)WideCharToMultiByte(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),result.data(),n,nullptr,nullptr);
    return result;
}
std::wstring Wide(const char* value) {
    const int length=static_cast<int>(std::strlen(value));
    const int n=MultiByteToWideChar(CP_UTF8,0,value,length,nullptr,0);
    std::wstring result(static_cast<size_t>(n),L'\0');
    if(n)MultiByteToWideChar(CP_UTF8,0,value,length,result.data(),n);
    return result;
}
void Copy(char* output,size_t capacity,const std::string& value) {
    if(!capacity)return;
    const auto n=std::min(capacity-1,value.size());
    std::memcpy(output,value.data(),n);
    output[n]=0;
}
bool Parse(const char* value,int& result,int low,int high,bool emptyZero) {
    if(!*value){result=0;return emptyZero;}
    char* end=nullptr;
    const long parsed=std::strtol(value,&end,10);
    if(end==value||*end||parsed<low||parsed>high)return false;
    result=static_cast<int>(parsed);
    return true;
}
void RecordPoint(int id) {
    if(id<0||id>=TestIdCount)return;
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    app.testPoints[id]={static_cast<int>((a.x+b.x)*0.5f),static_cast<int>((a.y+b.y)*0.5f),ImGui::IsItemVisible()};
}
void Flag(int issue) {app.layoutIssues|=issue;}
void Notify(const char* message) {shell.status=message;shell.statusUntil=ImGui::GetTime()+kStatusSeconds;}
void RefreshMonitors() {
    xhair.monitors.clear();
    for(const auto& name:crosshair::MonitorNames())xhair.monitors.push_back(Utf8(name));
    if(xhair.monitors.empty())xhair.monitors.emplace_back("主显示器");
}
void SyncConfig() {
    xhair.config=crosshair::GetConfig();
    Copy(xhair.ruler,sizeof(xhair.ruler),Utf8(xhair.config.rulerEntries));
    RefreshMonitors();
}
HWND Owner() {
    auto* viewport=ImGui::GetMainViewport();
    return viewport&&viewport->PlatformHandleRaw?static_cast<HWND>(viewport->PlatformHandleRaw):GetActiveWindow();
}
void Navigate(int page) {
    if(shell.page!=page)hotkey::CancelCapture();
    shell.page=page;
    calc.distanceActive=false;
}
}
