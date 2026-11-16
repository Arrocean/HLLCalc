// SPDX-License-Identifier: GPL-3.0-only
// Desktop integration test only. Sends bounded test keystrokes to a dedicated test session.
#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <cmath>
#include "resource.h"
namespace {
int failures = 0, clicks = 0;
std::wstring executablePath=L"build\\HLLCalc.exe";
bool testEnglish=false;
LRESULT CALLBACK Target(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_LBUTTONDOWN) { ++clicks; return 0; }
    return DefWindowProcW(h,m,w,l);
}
void Pump(DWORD ms) {
    const auto end = GetTickCount64() + ms;
    do {
        MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg); DispatchMessageW(&msg);}
        MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    } while (GetTickCount64() < end);
}
bool Await(const std::function<bool()>& predicate, DWORD ms=3000) {
    const auto end=GetTickCount64()+ms;
    do { if(predicate()) return true; Pump(20); } while(GetTickCount64()<end);
    return predicate();
}
void Check(bool value,const char* name) {
    std::cout << (value?"PASS: ":"FAIL: ") << name << std::endl;
    if(!value) ++failures;
}
void Key(WORD vk,bool up) { INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=vk;i.ki.dwFlags=up?KEYEVENTF_KEYUP:0;Check(SendInput(1,&i,sizeof(i))==1,"test key delivered"); }
void Tap(WORD vk) {Key(vk,false);Key(vk,true);}
HWND Find(const wchar_t* cls) {return FindWindowW(cls,nullptr);}
void CheckLayout(HWND main,const char* name) {
    Pump(160);
    const auto issues=SendMessageW(main,WM_APP+20,12,0);
    if(issues)std::cout<<"Layout issue mask: "<<issues<<std::endl;
    Check(issues==0,name);
}
bool ClickWidget(HWND main,int id) {
    const auto x=SendMessageW(main,WM_APP+20,10,id),y=SendMessageW(main,WM_APP+20,11,id);
    if(x<0||y<0)return false;
    POINT point{static_cast<LONG>(x),static_cast<LONG>(y)};ClientToScreen(main,&point);SetCursorPos(point.x,point.y);Pump(50);
    INPUT inputs[2]{};inputs[0].type=INPUT_MOUSE;inputs[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;inputs[1].type=INPUT_MOUSE;inputs[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    const bool ok=SendInput(2,inputs,sizeof(INPUT))==2;Pump(120);return ok;
}
void TypeCode(HWND main,const wchar_t* text) {
    Check(ClickWidget(main,15),"focus CS share-code input");
    Key(VK_CONTROL,false);Tap('A');Key(VK_CONTROL,true);
    std::vector<INPUT> inputs;
    for(const wchar_t ch:std::wstring(text)) {
        INPUT down{};down.type=INPUT_KEYBOARD;down.ki.wScan=ch;down.ki.dwFlags=KEYEVENTF_UNICODE;
        inputs.push_back(down);down.ki.dwFlags|=KEYEVENTF_KEYUP;inputs.push_back(down);
    }
    Check(SendInput(static_cast<UINT>(inputs.size()),inputs.data(),sizeof(INPUT))==inputs.size(),"deliver share code without taking over the clipboard");
    Pump(160);
}
void Chord(WORD key) {
    Key(VK_CONTROL,false);Key(VK_MENU,false);Tap(key);Key(VK_MENU,true);Key(VK_CONTROL,true);
}
void CheckCustomHotkeys(HWND main,HWND overlay,HWND other) {
    Check(ClickWidget(main,19),"open hotkey settings page");
    Check(Await([&]{return SendMessageW(main,WM_APP+20,2,0)==3;}),"hotkey settings displayed");
    Check(ClickWidget(main,20),"start recording hotkey");
    Check(Await([&]{return SendMessageW(main,WM_APP+20,43,0)!=0;}),"recording active");
    Key(VK_SHIFT,false);Key(VK_SHIFT,true);Pump(100);
    Check(SendMessageW(main,WM_APP+20,43,0)!=0,"modifier alone does not finish recording");
    Tap(VK_F8);
    Check(Await([&]{return SendMessageW(main,WM_APP+20,40,0)==VK_F8&&SendMessageW(main,WM_APP+20,43,0)==0;}),"record F8 as global hotkey");
    Pump(240);Check(IsWindowVisible(main),"recording keystroke does not hide calculator");
    Tap(VK_OEM_5);Pump(120);Check(IsWindowVisible(main),"previous backslash binding released");
    Tap(VK_F8);Check(Await([&]{return !IsWindowVisible(main);}),"custom single key hides calculator");
    Check(IsWindowVisible(overlay),"custom hotkey keeps crosshair visible");
    Tap(VK_F8);Check(Await([&]{return IsWindowVisible(main);}),"custom single key restores calculator");
    Check(ClickWidget(main,19),"reopen hotkey settings");
    Check(RegisterHotKey(other,77,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F9)!=FALSE,"reserve conflicting combination");
    Check(ClickWidget(main,24)&&ClickWidget(main,25),"select Ctrl and Alt modifiers manually");
    Check(ClickWidget(main,28),"open manual main-key list");
    Check(Await([&]{return SendMessageW(main,WM_APP+20,10,26)>=0;}),"F9 key option visible");
    Check(ClickWidget(main,26)&&ClickWidget(main,23),"apply occupied Ctrl Alt F9 shortcut");
    Check(Await([&]{return SendMessageW(main,WM_APP+20,44,0)!=0;}),"show shortcut conflict error");
    Check(SendMessageW(main,WM_APP+20,40,0)==VK_F8&&SendMessageW(main,WM_APP+20,42,0)!=0,"conflict preserves registered F8 shortcut");
    UnregisterHotKey(other,77);
    Check(ClickWidget(main,23),"retry free combination");
    Check(Await([&]{return SendMessageW(main,WM_APP+20,40,0)==VK_F9&&SendMessageW(main,WM_APP+20,41,0)==(MOD_CONTROL|MOD_ALT);}),"manual combination applies successfully");
    Chord(VK_F9);Check(Await([&]{return !IsWindowVisible(main);}),"custom combination hides calculator");
    Chord(VK_F9);Check(Await([&]{return IsWindowVisible(main);}),"custom combination restores calculator");
    Check(ClickWidget(main,19)&&ClickWidget(main,20),"record another shortcut for cancellation test");
    Tap(VK_ESCAPE);Pump(100);
    Check(IsWindowVisible(main)&&SendMessageW(main,WM_APP+20,43,0)==0&&SendMessageW(main,WM_APP+20,40,0)==VK_F9,"Escape cancels recording without hiding or changing shortcut");
    Check(ClickWidget(main,22),"restore default shortcut through UI");
    Check(Await([&]{return SendMessageW(main,WM_APP+20,40,0)==VK_OEM_5&&SendMessageW(main,WM_APP+20,41,0)==0;}),"default is unmodified backslash");
    Pump(240);Tap(VK_OEM_5);Check(Await([&]{return !IsWindowVisible(main);}),"restored default backslash works");
    Tap(VK_OEM_5);Check(Await([&]{return IsWindowVisible(main);}),"restored default brings calculator back");
}
void CheckRestartedHotkey(HWND target) {
    SetForegroundWindow(target);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    std::wstring command=L"\""+executablePath+L"\" --ui-test --keep-hotkey"+(testEnglish?L" --english":L"");
    const bool created=CreateProcessW(executablePath.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=FALSE;
    Check(created,"restart application with saved hotkey profile");if(!created)return;
    HWND main=nullptr;
    Check(Await([&]{main=Find(L"HLLCalcNativeWindow");return main&&SendMessageW(main,WM_APP+20,3,0)>5;}),"restarted application ready");
    if(main) {
        Check(SendMessageW(main,WM_APP+20,40,0)==VK_F8&&SendMessageW(main,WM_APP+20,41,0)==0&&SendMessageW(main,WM_APP+20,42,0)!=0,"recorded F8 survives real process restart");
        Check(ClickWidget(main,19)&&ClickWidget(main,22),"restore default in restarted application");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,40,0)==VK_OEM_5;}),"restarted application saves default backslash again");
        PostMessageW(main,WM_COMMAND,107,0);
    }
    const bool ended=Await([&]{return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;});
    Check(ended,"restarted application exits cleanly");
    if(!ended)TerminateProcess(process.hProcess,9);
    DWORD code=0;GetExitCodeProcess(process.hProcess,&code);Check(code==0,"restart test has no runtime errors");
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
}
void CheckIconResource(HWND main) {
    Check(GetClassLongPtrW(main,GCLP_HICON)!=0&&GetClassLongPtrW(main,GCLP_HICONSM)!=0,"window exposes resource-backed large and small icons");
    HMODULE module=LoadLibraryExW(executablePath.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    HRSRC resource=module?FindResourceW(module,MAKEINTRESOURCEW(IDI_HLLCALC),RT_GROUP_ICON):nullptr;
    const auto size=resource?SizeofResource(module,resource):0;
    HGLOBAL loaded=resource?LoadResource(module,resource):nullptr;
    const auto* bytes=loaded?static_cast<const unsigned char*>(LockResource(loaded)):nullptr;
    Check(bytes&&size>=6&&static_cast<unsigned>(bytes[4]|(bytes[5]<<8))>=6,"executable embeds a multi-resolution application icon");
    if(module)FreeLibrary(module);
}
}
int wmain(int argc,wchar_t* argv[]) {
    if(argc>1)executablePath=argv[1];
    if(argc>2)testEnglish=std::wstring(argv[2])==L"--english";
    wchar_t resolved[32768]{};const DWORD length=GetFullPathNameW(executablePath.c_str(),32768,resolved,nullptr);
    if(!length||length>=32768||GetFileAttributesW(resolved)==INVALID_FILE_ATTRIBUTES){std::cerr<<"Application executable not found. Pass its path as the first argument.\n";return 4;}
    executablePath.assign(resolved);
    if(Find(L"HLLCalcNativeWindow")) {std::cerr<<"Close existing HLLCalc before desktop tests.\n";return 3;}
    SetProcessDPIAware(); POINT originalCursor{};GetCursorPos(&originalCursor);
    WNDCLASSW wc{};wc.lpfnWndProc=Target;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"HLLCalc.TestTarget";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
    int cx=GetSystemMetrics(SM_CXSCREEN)/2,cy=GetSystemMetrics(SM_CYSCREEN)/2;
    HWND target=CreateWindowW(wc.lpszClassName,L"HLLCalc dedicated input-test target",WS_OVERLAPPEDWINDOW|WS_VISIBLE,cx-200,cy-150,400,300,nullptr,nullptr,wc.hInstance,nullptr);
    SetWindowPos(target,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
    SetForegroundWindow(target);SetCursorPos(cx,cy);Pump(100);
    INPUT activate[2]{};activate[0].type=INPUT_MOUSE;activate[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;activate[1].type=INPUT_MOUSE;activate[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    SendInput(2,activate,sizeof(INPUT));
    if(!Await([&]{return GetForegroundWindow()==target;})) {std::cerr<<"Cannot activate dedicated target; no keyboard input sent.\n";DestroyWindow(target);SetCursorPos(originalCursor.x,originalCursor.y);return 5;}
    clicks=0;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    std::wstring command=L"\""+executablePath+L"\" --ui-test"+(testEnglish?L" --english":L"");
    if(!CreateProcessW(executablePath.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)){DestroyWindow(target);return 4;}
    HWND main=nullptr,overlay=nullptr;
    Check(Await([&]{main=Find(L"HLLCalcNativeWindow");overlay=Find(L"HLLCalc.Crosshair.Overlay.1");return main&&overlay&&IsWindowVisible(main);}),"application and independent overlay created");
    if(main&&overlay) {
        Check(Await([&]{return GetForegroundWindow()==main;}),"startup foreground acquired");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,3,0)>8;}),"initial layout stabilized");
        RECT outer{},client{};POINT origin{};GetWindowRect(main,&outer);GetClientRect(main,&client);ClientToScreen(main,&origin);
        Check(origin.x==outer.left&&origin.y==outer.top&&client.right==outer.right-outer.left&&client.bottom==outer.bottom-outer.top,"client fills borderless window without native selection frame");
        CheckIconResource(main);
        Check(SendMessageW(main,WM_APP+20,40,0)==VK_OEM_5&&SendMessageW(main,WM_APP+20,42,0)!=0,"default backslash is registered");
        CheckLayout(main,"default layout has padding, aligned baselines and no card clipping");
        Check(SendMessageW(main,WM_APP+20,10,8)>=0,"hide action is visible without scrolling at default size");
        const auto returnX=SendMessageW(main,WM_APP+20,45,0),returnY=SendMessageW(main,WM_APP+20,46,0);
        Check(SendMessageW(main,WM_APP+20,47,0)!=0,"capture actual pre-show cursor snapshot");
        Tap(VK_OEM_5);
        Check(Await([&]{return !IsWindowVisible(main);}),"physical main backslash hides calculator");
        Check(IsWindowVisible(overlay),"overlay remains visible when hidden");
        Check(Await([&]{return GetForegroundWindow()==target;}),"foreground returned to test target");
        POINT p{};Check(Await([&]{GetCursorPos(&p);return p.x==returnX&&p.y==returnY;}),"cursor position restored to actual pre-show snapshot");
        if(p.x!=returnX||p.y!=returnY)std::cout<<"Expected cursor "<<returnX<<','<<returnY<<"; actual "<<p.x<<','<<p.y<<std::endl;
        // At screen center the overlay contains visible pixels, but input must reach target.
        SetForegroundWindow(target);SetCursorPos(cx,cy);
        INPUT click[2]{};click[0].type=INPUT_MOUSE;click[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;click[1].type=INPUT_MOUSE;click[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        SendInput(2,click,sizeof(INPUT));Check(Await([&]{return clicks>0;}),"overlay center is mouse click-through");
        Key(VK_OEM_5,false);
        Check(Await([&]{return IsWindowVisible(main)&&GetForegroundWindow()==main;}),"physical backslash shows and focuses calculator");
        Key(VK_OEM_5,false);Pump(120);Check(IsWindowVisible(main),"held backslash does not toggle repeatedly");Key(VK_OEM_5,true);
        // ImGui inputs are native-rendered widgets, not child EDIT HWNDs.
        // Read-only diagnostics are exposed only with --ui-test / --smoke-test.
        Check(Await([&]{return SendMessageW(main,WM_APP+20,1,0)!=0;}),"keyboard focus goes to ImGui distance input");
        Tap('6');Tap('0');Tap('0');
        Check(Await([&]{return SendMessageW(main,WM_APP+20,0,0)==600;}),"typing replaces selected distance");
        if(SendMessageW(main,WM_APP+20,0,0)!=600)std::cout<<"Actual distance: "<<SendMessageW(main,WM_APP+20,0,0)<<std::endl;
        Tap(VK_TAB);CheckLayout(main,"keyboard selection retains control-level focus and valid layout");
        Check(ClickWidget(main,9),"open padded nation menu");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,10,10)>=0;}),"nation options are visible");
        CheckLayout(main,"dropdown selection rows preserve inner margins");
        Check(ClickWidget(main,11),"select US option through real mouse input");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,4,0)==1;}),"nation selection updates calculator");
        CheckLayout(main,"selected calculator layout remains aligned");
        Check(ClickWidget(main,1),"click crosshair sidebar navigation");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,2,0)==1;}),"integrated crosshair settings page opened");
        Check(ClickWidget(main,3),"click live crosshair enable switch");
        Check(Await([&]{return !IsWindowVisible(overlay);}),"UI switch immediately disables independent overlay");
        Check(ClickWidget(main,3),"click live crosshair switch again");
        Check(Await([&]{return IsWindowVisible(overlay);}),"UI switch immediately restores independent overlay");
        Check(ClickWidget(main,4),"save configuration through real ImGui button");
        const auto beforeLength=SendMessageW(main,WM_APP+20,25,0);
        Check(ClickWidget(main,14),"open static CS share-code importer");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,5,0)!=0;}),"CS import dialog visible");
        TypeCode(main,L"CSGO-AAAAA-AAAAA-AAAAA-AAAAA-AAAAA");
        Check(ClickWidget(main,17),"attempt invalid share code import");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,6,0)!=0;}),"invalid code reports an error");
        Check(SendMessageW(main,WM_APP+20,25,0)==beforeLength,"invalid code leaves current crosshair unchanged");
        TypeCode(main,L"CSGO-Cn37R-YE7vo-pLCAL-aURmZ-z6zkG");
        Check(ClickWidget(main,17),"import known legacy CS crosshair code");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,5,0)==0&&SendMessageW(main,WM_APP+20,20,0)==175;}),"legacy code applies and closes dialog");
        const auto height=SendMessageW(main,WM_APP+20,31,0);
        Check(SendMessageW(main,WM_APP+20,21,0)==81&&SendMessageW(main,WM_APP+20,22,0)==213&&SendMessageW(main,WM_APP+20,23,0)==137,"legacy RGB and alpha applied");
        Check(SendMessageW(main,WM_APP+20,24,0)==1&&SendMessageW(main,WM_APP+20,32,0)==1,"legacy center dot and T shape applied statically");
        Check(SendMessageW(main,WM_APP+20,25,0)==std::lround(4.6*static_cast<double>(height)/480.0),"legacy geometry scales to the selected display");
        Check(SendMessageW(main,WM_APP+20,28,0)==0&&SendMessageW(main,WM_APP+20,29,0)==0,"share code does not retain unrelated ruler or ring");
        Check(ClickWidget(main,14),"reopen importer for new CS2 format");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,5,0)!=0;}),"new-format dialog ready");
        TypeCode(main,L"CSvbPubOq37zTGqtsPTP5QTrp5CB4xFXiKRLfzJsm49ZRe");
        Check(ClickWidget(main,17),"import new-format CS2 share code");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,5,0)==0&&SendMessageW(main,WM_APP+20,20,0)==255;}),"new CS2 code applies");
        Check(SendMessageW(main,WM_APP+20,25,0)==std::lround(5.0*static_cast<double>(height)/768.0)&&SendMessageW(main,WM_APP+20,27,0)==0,"new pixel format uses authored height, without legacy gap offset");
        Check(ClickWidget(main,14),"open importer before hotkey toggle");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,5,0)!=0;}),"dialog ready for hotkey test");
        Tap(VK_OEM_5);Check(Await([&]{return !IsWindowVisible(main);}),"backslash hides calculator while import modal is open");
        Check(IsWindowVisible(overlay),"import modal does not affect independent overlay");
        Tap(VK_OEM_5);
        Check(Await([&]{return IsWindowVisible(main)&&SendMessageW(main,WM_APP+20,5,0)==0&&SendMessageW(main,WM_APP+20,1,0)!=0;}),"reopening closes stale modal and restores distance focus");
        Check(ClickWidget(main,30),"click sidebar version to open application settings");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,2,0)==4;}),"version links to settings and updates page");
        Check(SendMessageW(main,WM_APP+20,48,0)==(testEnglish?1:0),"startup language follows isolated test preference");
        Check(ClickWidget(main,31),"open interface language selector");
        Check(ClickWidget(main,testEnglish?40:41),"select alternate interface language");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,48,0)==(testEnglish?0:1);}),"interface language changes immediately");
        Check(ClickWidget(main,0),"return to calculator in alternate language");CheckLayout(main,"alternate-language calculator layout remains valid");
        Check(ClickWidget(main,29)&&ClickWidget(main,31),"reopen language settings");
        Check(ClickWidget(main,testEnglish?41:40),"restore requested test language");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,48,0)==(testEnglish?1:0);}),"language preference restored");
        CheckCustomHotkeys(main,overlay,target);
        Check(ClickWidget(main,1),"return to crosshair after import and hotkey tests");
        Check(ClickWidget(main,2),"click help sidebar navigation");
        Check(Await([&]{return SendMessageW(main,WM_APP+20,2,0)==2;}),"help page rendered");
        Check(ClickWidget(main,1),"return to crosshair settings");
        const auto oldFrames=SendMessageW(main,WM_APP+20,3,0);
        SetWindowPos(main,nullptr,0,0,900,620,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        Check(Await([&]{return SendMessageW(main,WM_APP+20,3,0)>oldFrames+3;}),"DirectX renderer survives resizing on settings page");
        Tap(VK_OEM_5);Check(Await([&]{return !IsWindowVisible(main);}),"configured key hides entire settings interface");
        Check(IsWindowVisible(overlay),"settings hide does not hide crosshair");
        Tap(VK_OEM_5);Check(Await([&]{return IsWindowVisible(main);}),"calculator reopens after settings");
        CheckLayout(main,"narrow stacked calculator keeps card content within padding");
        Check(ClickWidget(main,5),"click custom titlebar close button");
        Check(Await([&]{return !IsWindowVisible(main)&&IsWindowVisible(overlay);}),"titlebar close only hides UI, not crosshair");
        Tap(VK_OEM_5);Check(Await([&]{return IsWindowVisible(main);}),"reopen after titlebar close");
        Check(ClickWidget(main,19)&&ClickWidget(main,20),"record F8 before persistence restart");
        Tap(VK_F8);
        Check(Await([&]{return SendMessageW(main,WM_APP+20,40,0)==VK_F8&&!SendMessageW(main,WM_APP+20,43,0);}),"saved F8 binding ready for process restart");
        PostMessageW(main,WM_COMMAND,107,0);
        Check(Await([&]{return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;}),"normal exit finishes process");
        DWORD exitCode=0;GetExitCodeProcess(process.hProcess,&exitCode);Check(exitCode==0,"application exits without renderer or runtime errors");
        Check(!IsWindow(overlay),"overlay destroyed on exit");
        Check(Await([&]{return GetForegroundWindow()==target;}),"normal exit restores previous foreground");
    }
    if(WaitForSingleObject(process.hProcess,0)!=WAIT_OBJECT_0) { if(main) PostMessageW(main,WM_COMMAND,107,0); Await([&]{return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;}); }
    if(WaitForSingleObject(process.hProcess,0)!=WAIT_OBJECT_0) TerminateProcess(process.hProcess,9);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    if(failures==0)CheckRestartedHotkey(target);
    DestroyWindow(target);SetCursorPos(originalCursor.x,originalCursor.y);
    std::cout << "Desktop integration failures: " << failures << '\n';return failures?1:0;
}
