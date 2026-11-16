// SPDX-License-Identifier: GPL-3.0-only
#include "input/hotkey.h"
#include "i18n/i18n.h"
#include <cwchar>
#include <string>
namespace hotkey {
namespace {
constexpr UINT AllowedMods=MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN;
constexpr int FirstId=0x51A0,SecondId=0x51A1;
HWND owner{};
std::wstring path;
UINT currentKey=VK_OEM_5,currentMods=0,suppressedKey=0;
int activeId=FirstId;
bool registered=false,capturing=false,error=false;
ULONGLONG quietUntil=0;
std::string status;
std::string Utf8(const std::wstring& text) {
    if(text.empty())return {};
    int size=WideCharToMultiByte(CP_UTF8,0,text.c_str(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(static_cast<size_t>(size),'\0');
    if(size)WideCharToMultiByte(CP_UTF8,0,text.c_str(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr);
    return result;
}
bool Fail(const char* message) {error=true;status=message;return false;}
bool Modifier(UINT key) {
    return key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU||key==VK_LSHIFT||key==VK_RSHIFT||
        key==VK_LCONTROL||key==VK_RCONTROL||key==VK_LMENU||key==VK_RMENU||key==VK_LWIN||key==VK_RWIN;
}
bool Valid(UINT key,UINT mods) {
    if(mods&~AllowedMods)return false;
    if(key==VK_F12||Modifier(key))return false;
    bool supported=(key>='A'&&key<='Z')||(key>='0'&&key<='9')||
        (key>=VK_F1&&key<=VK_F24)||(key>=VK_NUMPAD0&&key<=VK_DIVIDE)||
        (key>=VK_PRIOR&&key<=VK_DOWN)||key==VK_INSERT||key==VK_DELETE||key==VK_BACK||
        (key>=VK_OEM_1&&key<=VK_OEM_3)||(key>=VK_OEM_4&&key<=VK_OEM_8)||key==VK_OEM_102;
    if(!supported)return false;
    if((mods&MOD_ALT)&&key==VK_F4)return false;
    if((mods&MOD_CONTROL)&&(mods&MOD_ALT)&&key==VK_DELETE)return false;
    if((mods&MOD_WIN)&&key=='L')return false;
    return true;
}
bool Parse(const wchar_t* text,UINT& key,UINT& mods) {
    wchar_t* end{};
    if(!*text||*text==L'-')return false;
    unsigned long m=wcstoul(text,&end,10);
    if(end==text||*end!=L','||m>AllowedMods)return false;
    const wchar_t* second=end+1;
    if(!*second||*second==L'-')return false;
    unsigned long k=wcstoul(second,&end,10);
    if(end==second||*end||k>255)return false;
    if(!Valid(static_cast<UINT>(k),static_cast<UINT>(m)))return false;
    key=static_cast<UINT>(k);mods=static_cast<UINT>(m);return true;
}
UINT HeldModifiers() {
    UINT mods=0;
    if(GetKeyState(VK_CONTROL)&0x8000)mods|=MOD_CONTROL;
    if(GetKeyState(VK_MENU)&0x8000)mods|=MOD_ALT;
    if(GetKeyState(VK_SHIFT)&0x8000)mods|=MOD_SHIFT;
    if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)mods|=MOD_WIN;
    return mods;
}
void Captured(UINT key,UINT mods) {
    if(Set(key,mods)) {
        capturing=false;suppressedKey=key;quietUntil=GetTickCount64()+180;
        status="快捷键已保存："+Utf8(Label());
    }
}
}
std::wstring Label() {
    std::wstring label;
    if(currentMods&MOD_CONTROL)label+=L"Ctrl + ";
    if(currentMods&MOD_ALT)label+=L"Alt + ";
    if(currentMods&MOD_SHIFT)label+=L"Shift + ";
    if(currentMods&MOD_WIN)label+=L"Win + ";
    if(currentKey==VK_OEM_5)return label+L"\\";
    if(currentKey==VK_OEM_MINUS)return label+L"-";
    if(currentKey>=VK_F1&&currentKey<=VK_F24)return label+L"F"+std::to_wstring(currentKey-VK_F1+1);
    if((currentKey>='A'&&currentKey<='Z')||(currentKey>='0'&&currentKey<='9'))return label+static_cast<wchar_t>(currentKey);
    UINT scan=MapVirtualKeyW(currentKey,MAPVK_VK_TO_VSC);
    LONG packed=static_cast<LONG>(scan<<16);
    if(currentKey==VK_INSERT||currentKey==VK_DELETE||(currentKey>=VK_PRIOR&&currentKey<=VK_DOWN)||currentKey==VK_DIVIDE)packed|=1<<24;
    wchar_t name[80]{};
    if(GetKeyNameTextW(packed,name,80)>0)return label+name;
    return label+L"VK "+std::to_wstring(currentKey);
}
bool Initialize(HWND window,const std::wstring& iniPath) {
    Shutdown();owner=window;path=iniPath;currentKey=VK_OEM_5;currentMods=0;activeId=FirstId;
    if(!IsWindow(owner)||path.empty())return Fail("快捷键初始化失败。可从托盘重新打开设置。");
    wchar_t buffer[128]{};GetPrivateProfileStringW(L"Hotkey",L"Binding",L"",buffer,128,path.c_str());
    const bool malformed=*buffer&&!Parse(buffer,currentKey,currentMods);
    registered=RegisterHotKey(owner,activeId,currentMods|MOD_NOREPEAT,currentKey)!=FALSE;
    if(!registered)return Fail("快捷键注册失败，可能已被其他程序占用。请在快捷键设置中更换。");
    error=false;status=malformed?"原快捷键配置无效，本次已使用默认反斜杠键。":"当前快捷键："+Utf8(Label());
    return true;
}
bool Set(UINT key,UINT modifiers) {
    if(!Valid(key,modifiers))return Fail("不支持此按键或系统保留组合。可用字母、数字、标点、导航键或功能键（F12 除外）。");
    if(!IsWindow(owner)||path.empty())return Fail("快捷键尚未初始化。");
    const bool same=registered&&key==currentKey&&modifiers==currentMods;
    const int nextId=activeId==FirstId?SecondId:FirstId;
    if(!same&&!RegisterHotKey(owner,nextId,modifiers|MOD_NOREPEAT,key))
        return Fail("该快捷键无法注册，可能已被占用。原快捷键保持不变，请重试其他按键。");
    const auto value=std::to_wstring(modifiers)+L","+std::to_wstring(key);
    if(!WritePrivateProfileStringW(L"Hotkey",L"Binding",value.c_str(),path.c_str())) {
        if(!same)UnregisterHotKey(owner,nextId);
        return Fail("快捷键保存失败，请检查配置目录权限。原快捷键保持不变。");
    }
    if(!same) {
        if(registered)UnregisterHotKey(owner,activeId);
        activeId=nextId;registered=true;
    }
    currentKey=key;currentMods=modifiers;error=false;status="快捷键已保存："+Utf8(Label());
    return true;
}
bool ResetDefault() {const bool ok=Set(VK_OEM_5,0);if(ok){capturing=false;quietUntil=GetTickCount64()+180;}return ok;}
void BeginCapture() {capturing=true;error=false;status="请按新的快捷键；可同时按住 Ctrl / Alt / Shift / Win，Esc 取消。";}
void CancelCapture() {if(capturing){capturing=false;error=false;status="已取消录制。当前快捷键："+Utf8(Label());}}
bool HandleCaptureMessage(UINT message,WPARAM key,LPARAM data) {
    if((message==WM_KEYUP||message==WM_SYSKEYUP)&&suppressedKey==static_cast<UINT>(key)){suppressedKey=0;return true;}
    if(!capturing)return false;
    if(message==WM_HOTKEY) {
        if(registered&&static_cast<int>(key)==activeId){Captured(HIWORD(data),LOWORD(data)&AllowedMods);return true;}
        return false;
    }
    if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN) {
        const UINT vk=static_cast<UINT>(key);
        if(vk==VK_ESCAPE){CancelCapture();return true;}
        if(Modifier(vk)||(static_cast<ULONG_PTR>(data)&(static_cast<ULONG_PTR>(1)<<30)))return true;
        Captured(vk,HeldModifiers());return true;
    }
    if(message==WM_KEYUP||message==WM_SYSKEYUP||message==WM_CHAR||message==WM_SYSCHAR)return true;
    return false;
}
bool IsTrigger(WPARAM id,LPARAM packed) {
    if(!registered||capturing||static_cast<int>(id)!=activeId||HIWORD(packed)!=currentKey||(LOWORD(packed)&AllowedMods)!=currentMods)return false;
    if(GetTickCount64()<quietUntil)return false;
    if(suppressedKey){if(GetAsyncKeyState(static_cast<int>(suppressedKey))&0x8000)return false;suppressedKey=0;}
    return true;
}
void Shutdown() {
    if(registered&&owner)UnregisterHotKey(owner,activeId);
    owner=nullptr;path.clear();registered=false;capturing=false;error=false;
    suppressedKey=0;quietUntil=0;status.clear();
}
bool IsCapturing(){return capturing;}
bool IsRegistered(){return registered;}
UINT Key(){return currentKey;}
UINT Modifiers(){return currentMods;}
std::string Status(){return i18n::Translate(status);}
bool HasError(){return error;}
}
