// SPDX-License-Identifier: GPL-3.0-only
#define UNICODE
#define _UNICODE
#include "input/hotkey.h"
#include <cstdlib>
#include <cwchar>
#include <iostream>
void Check(bool value,const char* message){if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
std::wstring Binding(const std::wstring& path){wchar_t text[128]{};GetPrivateProfileStringW(L"Hotkey",L"Binding",L"",text,128,path.c_str());return text;}
int main(){
    wchar_t temp[MAX_PATH]{},file[MAX_PATH]{};
    Check(GetTempPathW(MAX_PATH,temp)!=0,"temporary directory");
    Check(GetTempFileNameW(temp,L"HKT",0,file)!=0,"unique test-owned file");
    const std::wstring path=file;
    HANDLE handle=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(handle!=INVALID_HANDLE_VALUE,"open test-owned file");WORD bom=0xfeff;DWORD written{};WriteFile(handle,&bom,2,&written,nullptr);CloseHandle(handle);
    const auto instance=GetModuleHandleW(nullptr);
    const auto className=L"HLLCalc.HotkeyTest."+std::to_wstring(GetCurrentProcessId());
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=className.c_str();
    Check(RegisterClassW(&wc)!=0,"register hidden test class");
    HWND owner=CreateWindowW(className.c_str(),L"",0,0,0,10,10,nullptr,nullptr,instance,nullptr);
    HWND other=CreateWindowW(className.c_str(),L"",0,0,0,10,10,nullptr,nullptr,instance,nullptr);
    Check(owner&&other,"hidden test windows");
    Check(hotkey::Initialize(owner,path),"default backslash registration");
    Check(hotkey::Key()==VK_OEM_5&&hotkey::Modifiers()==0&&hotkey::Label()==L"\\","default binding and label");
    Check(!hotkey::Set(VK_CONTROL,0)&&!hotkey::Set(VK_F12,0)&&!hotkey::Set(VK_F4,MOD_ALT),"reject modifier and reserved shortcuts");
    Check(hotkey::Key()==VK_OEM_5&&hotkey::IsRegistered(),"invalid keys preserve old binding");
    Check(hotkey::Set(VK_F14,MOD_CONTROL|MOD_ALT),"set combination key");
    Check(Binding(path)==std::to_wstring(MOD_CONTROL|MOD_ALT)+L","+std::to_wstring(VK_F14),"persist a single binding value");
    Check(hotkey::Label()==L"Ctrl + Alt + F14","combination display label");
    Check(RegisterHotKey(other,9,MOD_NOREPEAT,VK_OEM_5)!=FALSE,"old backslash released after successful switch");UnregisterHotKey(other,9);
    Check(RegisterHotKey(other,9,MOD_NOREPEAT,VK_F15)!=FALSE,"reserve conflict key in other window");
    const auto before=Binding(path);
    Check(!hotkey::Set(VK_F15,0)&&hotkey::HasError(),"report registration conflict");
    Check(hotkey::Key()==VK_F14&&hotkey::Modifiers()==(MOD_CONTROL|MOD_ALT)&&Binding(path)==before,"conflict preserves binding and file");UnregisterHotKey(other,9);
    hotkey::BeginCapture();Check(hotkey::IsCapturing(),"capture begins");
    Check(hotkey::HandleCaptureMessage(WM_KEYDOWN,VK_LCONTROL,0)&&hotkey::IsCapturing(),"modifier alone does not commit");
    Check(hotkey::HandleCaptureMessage(WM_KEYDOWN,VK_ESCAPE,0)&&!hotkey::IsCapturing(),"escape cancels capture");
    Check(hotkey::Key()==VK_F14&&Binding(path)==before,"cancel preserves binding");
    hotkey::BeginCapture();Check(hotkey::HandleCaptureMessage(WM_KEYDOWN,VK_F16,0)&&!hotkey::IsCapturing()&&hotkey::Key()==VK_F16,"record supported key");
    const UINT recordedMods=hotkey::Modifiers();
    hotkey::Shutdown();Check(hotkey::Initialize(owner,path)&&hotkey::Key()==VK_F16&&hotkey::Modifiers()==recordedMods,"reload recorded binding");
    Check(hotkey::ResetDefault()&&Binding(path)==L"0,220","restore and persist default backslash");
    hotkey::Shutdown();WritePrivateProfileStringW(L"Hotkey",L"Binding",L"999,999",path.c_str());
    Check(hotkey::Initialize(owner,path)&&hotkey::Key()==VK_OEM_5,"invalid file falls back safely");
    hotkey::Shutdown();Check(RegisterHotKey(other,9,MOD_NOREPEAT,VK_F17)!=FALSE,"reserve startup conflict");
    WritePrivateProfileStringW(L"Hotkey",L"Binding",(L"0,"+std::to_wstring(VK_F17)).c_str(),path.c_str());
    Check(!hotkey::Initialize(owner,path)&&!hotkey::IsRegistered()&&hotkey::Key()==VK_F17,"startup conflict remains recoverable");
    Check(hotkey::Set(VK_F18,0)&&hotkey::IsRegistered(),"repair startup conflict with another key");UnregisterHotKey(other,9);
    hotkey::Shutdown();
    Check(hotkey::Initialize(owner,path+L"\\missing.ini"),"register while using intentionally unwritable profile path");
    Check(!hotkey::Set(VK_F19,0)&&hotkey::IsRegistered()&&hotkey::Key()==VK_OEM_5,"failed save rolls back new registration");
    Check(RegisterHotKey(other,9,MOD_NOREPEAT,VK_F19)!=FALSE,"failed save does not leak new key");UnregisterHotKey(other,9);
    hotkey::Shutdown();DestroyWindow(owner);DestroyWindow(other);UnregisterClassW(className.c_str(),instance);
    // Only remove the exact absolute file allocated by GetTempFileName for this test.
    wchar_t resolved[MAX_PATH]{};GetFullPathNameW(path.c_str(),MAX_PATH,resolved,nullptr);
    Check(_wcsicmp(resolved,path.c_str())==0&&path.find(temp)==0,"verify owned cleanup target");
    Check(DeleteFileW(path.c_str())!=FALSE,"remove owned test profile");
    std::cout<<"All custom hotkey registration, persistence and rollback tests passed.\n";
}
