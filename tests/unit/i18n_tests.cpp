// SPDX-License-Identifier: GPL-3.0-only
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "i18n/i18n.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <string>

namespace {
unsigned checks=0;
std::wstring ownedProfile;
void Cleanup() {
    if(ownedProfile.empty())return;
    wchar_t absolute[MAX_PATH]{};
    const DWORD length=GetFullPathNameW(ownedProfile.c_str(),MAX_PATH,absolute,nullptr);
    // Delete only the unique, absolute file this test itself allocated.
    if(length>0&&length<MAX_PATH&&_wcsicmp(absolute,ownedProfile.c_str())==0) {
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,ownedProfile.c_str());
        DeleteFileW(ownedProfile.c_str());
    }
}
void Check(bool condition,const char* message) {
    ++checks;
    if(!condition){std::cerr<<"FAIL: "<<message<<'\n';Cleanup();std::exit(1);}
}
std::wstring SavedLanguage() {
    wchar_t value[32]{};GetPrivateProfileStringW(L"Appearance",L"Language",L"",value,32,ownedProfile.c_str());return value;
}
}
int main() {
    using i18n::Language;
    wchar_t temporary[MAX_PATH]{},profile[MAX_PATH]{};
    Check(GetTempPathW(MAX_PATH,temporary)!=0,"temporary directory available");
    Check(GetTempFileNameW(temporary,L"HLI",0,profile)!=0,"allocate unique profile");
    ownedProfile=profile;
    HANDLE file=CreateFileW(ownedProfile.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(file!=INVALID_HANDLE_VALUE,"open owned profile");
    const WORD bom=0xfeff;DWORD written{};
    Check(WriteFile(file,&bom,sizeof(bom),&written,nullptr)!=FALSE&&written==sizeof(bom),"write Unicode profile marker");CloseHandle(file);

    std::string diagnostic="old";
    Check(i18n::EntryCount()>=200,"complete dictionary contains at least 200 entries");
    Check(i18n::ValidateDictionary(diagnostic)&&diagnostic.empty(),"all dictionary keys unique and printf placeholders unchanged");
    i18n::Initialize(ownedProfile);
    const auto expectedSystem=PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_CHINESE?Language::Chinese:Language::English;
    Check(i18n::Current()==expectedSystem,"absent preference follows Windows UI language");
    Check(SavedLanguage().empty(),"Initialize does not overwrite absent preference");

    Check(i18n::SetLanguage(Language::English),"switch to English and persist");
    Check(i18n::Current()==Language::English&&SavedLanguage()==L"en-US","current and saved English language");
    Check(std::strcmp(i18n::Tr("火炮计算"),"Artillery")==0,"navigation translation");
    Check(std::strcmp(i18n::Tr("准星颜色"),"Crosshair color")==0,"crosshair setting translation");
    Check(std::strcmp(i18n::Tr("当前版本"),"Current version")==0,"version setting translation");
    Check(std::strcmp(i18n::Tr("安装并重启"),"Install & restart")==0,"update action translation");
    Check(std::strcmp(i18n::Tr("确认安装更新"),"Confirm update installation")==0,"update confirmation title translation");
    Check(std::strcmp(i18n::Tr("语言设置保存失败"),"Unable to save the language preference")==0,"language save failure translation");
    Check(std::strcmp(i18n::Tr("当前快捷键：%s，呼出或收起计算器。"),"Current shortcut: %s, to show or hide the calculator.")==0,"format placeholders kept");
    char output[256]{};std::snprintf(output,sizeof(output),i18n::Tr("当前快捷键：%s，呼出或收起计算器。"),"Ctrl + F8");
    Check(std::string(output)=="Current shortcut: Ctrl + F8, to show or hide the calculator.","formatted English text uses original argument");
    Check(i18n::Translate("快捷键已保存：Ctrl + Alt + F9")=="Hotkey saved: Ctrl + Alt + F9","dynamic saved-hotkey prefix");
    Check(i18n::Translate("已取消录制。当前快捷键：\\")=="Recording canceled. Current hotkey: \\","longest dynamic prefix preserves backslash");
    Check(i18n::Translate("当前快捷键：F8")=="Current hotkey: F8","current-hotkey prefix");
    Check(i18n::Translate("小键盘 7")=="Numpad 7","dynamic numpad label");
    Check(i18n::Translate("1 - \\\\.\\DISPLAY1（主）")=="1 - \\\\.\\DISPLAY1 (Primary)","monitor suffix preserves device identifier");
    Check(i18n::Translate("HLLCalc · \\ 显示 / 隐藏")=="HLLCalc · \\ Show / hide","tray suffix preserves current hotkey");
    Check(i18n::Translate("发现新版本：v1.4.0-rc.1")=="New version available: v1.4.0-rc.1","update-version prefix");
    Check(i18n::Wide(L"隐藏计算器")==L"Hide calculator","native wide string translation");
    Check(i18n::Wide(L"当前快捷键：F8")==L"Current hotkey: F8","wide dynamic translation");
    const char unknown[]="Untranslated custom label";
    Check(i18n::Tr(unknown)==unknown,"unknown literal returns original pointer");
    Check(i18n::Translate("用户自定义刻度 100")=="用户自定义刻度 100","unknown user text remains unchanged");
    Check(i18n::Wide(L"用户自定义刻度 100")==L"用户自定义刻度 100","unknown Unicode wide text remains unchanged");
    Check(std::strcmp(i18n::Tr("##火炮计算"),"##火炮计算")==0,"internal ImGui ID never translated");
    Check(i18n::Translate("火炮计算##stable-id")=="火炮计算##stable-id","combined ImGui label left for caller to split safely");
    Check(std::strcmp(i18n::Tr(nullptr),"")==0&&i18n::Wide(nullptr).empty(),"null input is safe");
    const char* stable=i18n::Tr("火炮计算");
    Check(i18n::SetLanguage(Language::Chinese),"switch Chinese immediately");
    Check(i18n::Current()==Language::Chinese&&SavedLanguage()==L"zh-CN","Chinese language saved");
    Check(std::strcmp(i18n::Tr("火炮计算"),"火炮计算")==0,"Chinese uses original text");
    Check(i18n::Translate("快捷键已保存：F8")=="快捷键已保存：F8","Chinese dynamic text unchanged");
    Check(i18n::Wide(L"准星配置")==L"准星配置","Chinese wide text unchanged");
    Check(std::strcmp(stable,"Artillery")==0,"previous dictionary pointer remains valid after switching");
    i18n::Initialize(ownedProfile);
    Check(i18n::Current()==Language::Chinese,"reload persisted Chinese preference");
    Check(i18n::SetLanguage(Language::English),"persist English again");
    i18n::Initialize(ownedProfile);Check(i18n::Current()==Language::English,"reload persisted English preference");
    Check(!i18n::SetLanguage(static_cast<Language>(99))&&i18n::Current()==Language::English&&SavedLanguage()==L"en-US","invalid enum cannot mutate language or file");

    WritePrivateProfileStringW(L"Appearance",L"Language",L"unknown-language",ownedProfile.c_str());
    i18n::Initialize(ownedProfile);Check(i18n::Current()==expectedSystem,"unknown saved preference follows system default");
    Check(SavedLanguage()==L"unknown-language","initialization preserves unknown value for later user choice");
    WritePrivateProfileStringW(L"Appearance",L"Language",L"EN-us",ownedProfile.c_str());
    i18n::Initialize(ownedProfile);Check(i18n::Current()==Language::English,"saved language codes are case-insensitive");
    i18n::Initialize(ownedProfile+L"\\missing.ini");
    const auto beforeFailure=i18n::Current();
    Check(!i18n::SetLanguage(beforeFailure==Language::Chinese?Language::English:Language::Chinese)&&i18n::Current()==beforeFailure,"save failure leaves current language unchanged");
    i18n::Initialize(L"");
    Check(!i18n::SetLanguage(Language::English)&&i18n::Current()==expectedSystem,"missing profile path never silently claims a saved change");

    Cleanup();ownedProfile.clear();
    std::cout<<"PASS: "<<checks<<" i18n checks; "<<i18n::EntryCount()<<" dictionary entries validated.\n";
}
