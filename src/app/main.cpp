// SPDX-License-Identifier: GPL-3.0-only
// Native Win32 host: Dear ImGui / DirectX 11 UI and an independent GDI overlay.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <algorithm>
#include <string>
#include <cwchar>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ballistics/ballistics.h"
#include "crosshair/crosshair.h"
#include "app/ui.h"
#include "resource.h"
#include "input/hotkey.h"
#include "app/motion.h"
#include "i18n/i18n.h"
#include "update/updater.h"
#include "version.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
constexpr UINT TrayMessage=WM_APP+1, TestQuery=WM_APP+20;
constexpr int IdSettings=105, IdHide=106, IdQuit=107, IdShow=109;
HWND window{}, previousWindow{};
HICON appLargeIcon{},appSmallIcon{}; // Shared handles from this module's resources.
ID3D11Device* device{}; ID3D11DeviceContext* context{};
IDXGISwapChain* swapChain{}; ID3D11RenderTargetView* renderTarget{};
UINT resizeWidth{},resizeHeight{},taskbarCreated{};
float scale=1.0f;
bool ready=false,testing=false,testProfile=false,smokeOK=true;
std::wstring trayHotkeyLabel;
i18n::Language trayLanguage=i18n::Language::Chinese;
int smokePhase=0,cursorIncrements=0;
unsigned frames=0;
POINT savedCursor{}; bool savedCursorValid=false;
std::wstring iniPath;
motion::Tween windowMotion;
bool hidePending=false,animationTest=false;
constexpr UINT AnimationTimer=3;
double MotionTime(){return static_cast<double>(GetTickCount64())/1000.0;}
int S(int n) {return static_cast<int>(n*scale+0.5f);}
void ReleaseTarget() {if(renderTarget){renderTarget->Release();renderTarget=nullptr;}}
bool CreateTarget() {
    ID3D11Texture2D* buffer{};
    if(FAILED(swapChain->GetBuffer(0,IID_PPV_ARGS(&buffer))))return false;
    const HRESULT result=device->CreateRenderTargetView(buffer,nullptr,&renderTarget);buffer->Release();return SUCCEEDED(result);
}
void CleanupGraphics() {
    ReleaseTarget();if(swapChain){swapChain->Release();swapChain=nullptr;}
    if(context){context->Release();context=nullptr;}if(device){device->Release();device=nullptr;}
}
bool CreateGraphics() {
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL feature{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,2,D3D11_SDK_VERSION,&desc,&swapChain,&device,&feature,&context);
    if(FAILED(result)) {
        CleanupGraphics();
        result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,2,D3D11_SDK_VERSION,&desc,&swapChain,&device,&feature,&context);
    }
    if(FAILED(result))return false;
    return CreateTarget();
}
void EnsureCursor() {
    if(cursorIncrements)return;
    int value;do{value=ShowCursor(TRUE);++cursorIncrements;}while(value<0&&cursorIncrements<32);
    SetCursor(LoadCursorW(nullptr,IDC_ARROW));
}
void RestoreCursor() {while(cursorIncrements>0){ShowCursor(FALSE);--cursorIncrements;}}
void CaptureReturnTarget() {
    HWND foreground=GetForegroundWindow();DWORD pid{};GetWindowThreadProcessId(foreground,&pid);
    if(foreground&&pid!=GetCurrentProcessId()){previousWindow=foreground;savedCursorValid=GetCursorPos(&savedCursor)!=FALSE;}
}
void RestoreReturnTarget() {
    RestoreCursor();
    if(IsWindow(previousWindow)&&previousWindow!=window&&previousWindow!=crosshair::OverlayWindow()) {
        SetForegroundWindow(previousWindow);
        if(savedCursorValid&&GetForegroundWindow()==previousWindow)SetCursorPos(savedCursor.x,savedCursor.y);
    }
    previousWindow=nullptr;savedCursorValid=false;
}
void CompleteHide() {
    hidePending=false;windowMotion.Snap(0);KillTimer(window,AnimationTimer);
    ShowWindow(window,SW_HIDE);RestoreReturnTarget();crosshair::RefreshTopmost();
    if(ImGui::GetCurrentContext()){ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearInputMouse();}
}
void TickWindowMotion() {
    const float alpha=windowMotion.Tick(MotionTime());
    SetLayeredWindowAttributes(window,0,static_cast<BYTE>(std::clamp(alpha,0.0f,1.0f)*255.0f+0.5f),LWA_ALPHA);
    if(!windowMotion.active){KillTimer(window,AnimationTimer);if(hidePending)CompleteHide();}
}
void HideCalculator() {
    hotkey::CancelCapture();if(ready)ui::Save();
    if(!IsWindowVisible(window))return;
    if(ready&&(!testProfile||animationTest)&&ui::AnimationsEnabled()) {
        hidePending=true;windowMotion.Start(0,MotionTime(),0.16);SetTimer(window,AnimationTimer,10,nullptr);
    } else CompleteHide();
}
void ShowCalculator() {
    const bool wasVisible=IsWindowVisible(window)!=FALSE;
    if(!wasVisible)CaptureReturnTarget();hidePending=false;
    const bool animate=ready&&(!testProfile||animationTest)&&ui::AnimationsEnabled();
    if(!wasVisible)windowMotion.Snap(animate?0.0f:1.0f);
    if(animate){windowMotion.Start(1,MotionTime(),0.20);SetTimer(window,AnimationTimer,10,nullptr);}
    else {windowMotion.Snap(1);KillTimer(window,AnimationTimer);}
    SetLayeredWindowAttributes(window,0,static_cast<BYTE>(windowMotion.value*255.0f+0.5f),LWA_ALPHA);
    ShowWindow(window,SW_RESTORE);
    SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
    SetForegroundWindow(window);SetActiveWindow(window);
    if(GetForegroundWindow()==window) {
        ClipCursor(nullptr);ReleaseCapture();EnsureCursor();SetFocus(window);
        if(ready)ui::FocusDistance();
        RECT rect{};GetWindowRect(window,&rect);SetCursorPos(rect.left+S(350),rect.top+S(230));
    } else {FLASHWINFO flash{sizeof(flash),window,FLASHW_TRAY|FLASHW_CAPTION,3,0};FlashWindowEx(&flash);}
    crosshair::RefreshTopmost();
}
void Toggle() {if(hidePending||!IsWindowVisible(window))ShowCalculator();else HideCalculator();}
void Tray(bool add,bool update=false) {
    NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;
    data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=TrayMessage;data.hIcon=appSmallIcon?appSmallIcon:appLargeIcon;
    trayHotkeyLabel=hotkey::Label();trayLanguage=i18n::Current();
    const auto tip=L"HLLCalc · "+trayHotkeyLabel+L" "+i18n::Wide(L"显示 / 隐藏");
    wcsncpy_s(data.szTip,tip.c_str(),_TRUNCATE);Shell_NotifyIconW(add?(update?NIM_MODIFY:NIM_ADD):NIM_DELETE,&data);
}
void TrayMenu() {
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,IdHide,i18n::Wide(IsWindowVisible(window)?L"隐藏计算器":L"显示计算器").c_str());
    AppendMenuW(menu,MF_STRING,IdSettings,i18n::Wide(L"自定义准星").c_str());AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,IdQuit,i18n::Wide(L"退出程序").c_str());
    POINT p{};GetCursorPos(&p);if(!IsWindowVisible(window))CaptureReturnTarget();SetForegroundWindow(window);
    int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);DestroyMenu(menu);PostMessageW(window,WM_NULL,0,0);
    if(chosen==IdHide)Toggle();else if(chosen)PostMessageW(window,WM_COMMAND,chosen,0);else if(!IsWindowVisible(window))RestoreReturnTarget();
}
LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    if(taskbarCreated&&msg==taskbarCreated){Tray(true);return 0;}
    if(hotkey::HandleCaptureMessage(msg,wp,lp))return 0;
    if(msg==WM_HOTKEY){if(hotkey::IsTrigger(wp,lp))Toggle();return 0;}
    if(msg==TestQuery&&testProfile&&ready) {
        switch(wp){case 0:return ui::DistanceValue();case 1:return ui::DistanceFocused();case 2:return ui::CurrentPage();case 3:return frames;case 4:return ui::NationValue();case 5:return ui::CsDialogVisible();case 6:return ui::CsImportFailed();case 12:return ui::LayoutIssues();case 40:return hotkey::Key();case 41:return hotkey::Modifiers();case 42:return hotkey::IsRegistered();case 43:return hotkey::IsCapturing();case 44:return hotkey::HasError();case 45:return savedCursor.x;case 46:return savedCursor.y;case 47:return savedCursorValid;case 48:return i18n::Current()==i18n::Language::English;case 49:return ui::AnimationsEnabled();case 50:return static_cast<LRESULT>(updater::GetSnapshot().phase);case 51:return windowMotion.active;case 52:return static_cast<LRESULT>(windowMotion.value*255.0f);}
        if(wp>=20&&wp<=32){const auto cfg=crosshair::GetConfig();switch(wp){case 20:return cfg.red;case 21:return cfg.green;case 22:return cfg.blue;case 23:return cfg.opacity;case 24:return cfg.cross;case 25:return cfg.length;case 26:return cfg.thickness;case 27:return cfg.gap;case 28:return cfg.ruler;case 29:return cfg.ring;case 30:return cfg.enabled;case 31:return crosshair::MonitorHeightPixels();case 32:return cfg.dot;}}
        if(wp==10||wp==11){int x=0,y=0;if(!ui::TestWidgetPoint(static_cast<int>(lp),x,y))return -1;return wp==10?x:y;}
        return 0;
    }
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(h,msg,wp,lp))return 1;
    switch(msg) {
    // Both NCCALCSIZE forms keep the client area equal to the whole window.
    // Falling through for wParam==FALSE leaves the initial thick system frame.
    case WM_NCCALCSIZE:return 0;
    case WM_NCACTIVATE:return DefWindowProcW(h,msg,wp,-1);
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(h,&point);RECT r{};GetClientRect(h,&r);
        int edge=S(6);bool left=point.x<edge,right=point.x>=r.right-edge,top=point.y<edge,bottom=point.y>=r.bottom-edge;
        if(top&&left)return HTTOPLEFT;if(top&&right)return HTTOPRIGHT;if(bottom&&left)return HTBOTTOMLEFT;if(bottom&&right)return HTBOTTOMRIGHT;
        if(left)return HTLEFT;if(right)return HTRIGHT;if(top)return HTTOP;if(bottom)return HTBOTTOM;
        if(point.y<S(44)&&point.x<r.right-S(100))return HTCAPTION;
        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={S(800),S(560)};
        MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&mi);
        info->ptMaxPosition={mi.rcWork.left-mi.rcMonitor.left,mi.rcWork.top-mi.rcMonitor.top};
        info->ptMaxSize={mi.rcWork.right-mi.rcWork.left,mi.rcWork.bottom-mi.rcWork.top};return 0;
    }
    case WM_SIZE:if(wp!=SIZE_MINIMIZED){resizeWidth=LOWORD(lp);resizeHeight=HIWORD(lp);}return 0;
    case WM_SYSCOMMAND:if((wp&0xFFF0)==SC_MINIMIZE){HideCalculator();return 0;}if((wp&0xFFF0)==SC_KEYMENU)return 0;break;
    case WM_CLOSE:HideCalculator();return 0;
    case WM_COMMAND:
        if(wp==IdQuit){DestroyWindow(h);return 0;}
        if(wp==IdHide){HideCalculator();return 0;}
        if(wp==IdShow){ShowCalculator();return 0;}
        if(wp==IdSettings&&ready){if(!IsWindowVisible(h))ShowCalculator();ui::OpenCrosshair();return 0;}break;
    case WM_KEYDOWN:if(wp==VK_ESCAPE&&GetForegroundWindow()==window){HideCalculator();return 0;}break;
    case WM_ACTIVATEAPP:if(!wp){hotkey::CancelCapture();RestoreCursor();}else if(ready&&IsWindowVisible(h))EnsureCursor();break;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}
    case WM_TIMER:
        if(wp==AnimationTimer){TickWindowMotion();return 0;}
        if(wp==1){crosshair::RefreshTopmost();if(hotkey::Label()!=trayHotkeyLabel||trayLanguage!=i18n::Current())Tray(true,true);return 0;}
        if(wp==2&&testing&&ready) {
            HWND overlay=crosshair::OverlayWindow();auto check=[](bool x){smokeOK=smokeOK&&x;};
            if(smokePhase==0){
                check(frames>0&&IsWindowVisible(overlay));check(GetWindow(overlay,GW_OWNER)==nullptr);
                LONG_PTR flags=GetWindowLongPtrW(overlay,GWL_EXSTYLE);
                check((flags&(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOPMOST))==(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOPMOST));
                Toggle();check(!IsWindowVisible(h)&&IsWindowVisible(overlay));
            }else if(smokePhase==1){Toggle();check(IsWindowVisible(h));check(ui::DistanceValue()==500);ui::OpenCrosshair();}
            else if(smokePhase==2){check(ui::CurrentPage()==1);Toggle();check(!IsWindowVisible(h)&&IsWindowVisible(overlay));DestroyWindow(h);}
            ++smokePhase;return 0;
        }break;
    case TrayMessage:if(lp==WM_LBUTTONUP)Toggle();else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)TrayMenu();return 0;
    case WM_DESTROY:
        if(ready)ui::Save();KillTimer(h,1);KillTimer(h,2);KillTimer(h,AnimationTimer);
        hotkey::Shutdown();
        Tray(false);RestoreReturnTarget();PostQuitMessage(testing&&!smokeOK?2:0);return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}
bool LoadFont() {
    wchar_t windows[MAX_PATH]{};GetWindowsDirectoryW(windows,MAX_PATH);
    for(const auto* name:{L"msyh.ttc",L"msyh.ttf",L"simhei.ttf",L"msjh.ttc"}) {
        const auto path=std::wstring(windows)+L"\\Fonts\\"+name;
        if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)continue;
        char utf8[MAX_PATH*4]{};WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,utf8,sizeof(utf8),nullptr,nullptr);
        ImFontConfig config{};config.OversampleH=2;config.OversampleV=1;
        if(auto font=ImGui::GetIO().Fonts->AddFontFromFileTTF(utf8,18.0f*scale,&config)){ImGui::GetIO().FontDefault=font;return true;}
    }
    return false;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR arguments,int) {
    int helperExit=0;if(updater::HandleHelperCommandLine(helperExit))return helperExit;
    animationTest=wcsstr(arguments,L"--animation-test")!=nullptr;
    testing=wcsstr(arguments,L"--smoke-test")!=nullptr;testProfile=testing||animationTest||wcsstr(arguments,L"--ui-test")!=nullptr;
    HANDLE mutex=CreateMutexW(nullptr,FALSE,testProfile?L"Local\\HLLCalc.Native.Smoke":L"Local\\HLLCalc.Native");
    if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(HWND old=FindWindowW(L"HLLCalcNativeWindow",nullptr))PostMessageW(old,WM_COMMAND,IdShow,0);CloseHandle(mutex);return 0;}
    SetProcessDPIAware();HDC dc=GetDC(nullptr);scale=GetDeviceCaps(dc,LOGPIXELSX)/96.0f;ReleaseDC(nullptr,dc);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    scale=std::max(0.6f,std::min({scale,(work.right-work.left-40)/1000.0f,(work.bottom-work.top-40)/700.0f}));
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    PWSTR local{};if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&local))){CloseHandle(mutex);CoUninitialize();return 1;}
    std::wstring folder=std::wstring(local)+L"\\HLLCalc";CoTaskMemFree(local);
    if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS){CloseHandle(mutex);CoUninitialize();return 1;}
    iniPath=folder+(testProfile?L"\\smoke-test.ini":L"\\settings.ini");
    HANDLE ini=CreateFileW(iniPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(ini!=INVALID_HANDLE_VALUE){WORD bom=0xfeff;DWORD written{};WriteFile(ini,&bom,2,&written,nullptr);CloseHandle(ini);}
    if(testProfile){
        WritePrivateProfileStringW(L"Calculator",L"Nation",L"0",iniPath.c_str());WritePrivateProfileStringW(L"Calculator",L"Mode",L"0",iniPath.c_str());
        WritePrivateProfileStringW(L"Calculator",L"Distance",L"500",iniPath.c_str());WritePrivateProfileStringW(L"Calculator",L"Inclination",L"0",iniPath.c_str());
        WritePrivateProfileStringW(L"Crosshair",L"Enabled",L"1",iniPath.c_str());WritePrivateProfileStringW(L"Crosshair",L"Opacity",L"255",iniPath.c_str());
        WritePrivateProfileStringW(L"Appearance",L"Language",wcsstr(arguments,L"--english")?L"en-US":L"zh-CN",iniPath.c_str());
        if(animationTest)WritePrivateProfileStringW(L"Appearance",L"Animations",L"1",iniPath.c_str());
        if(!wcsstr(arguments,L"--keep-hotkey"))WritePrivateProfileStringW(L"Hotkey",L"Binding",L"0,220",iniPath.c_str());
    }
    i18n::Initialize(iniPath);
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    updater::Initialize(HLLCALC_VERSION_STRING,executable,"Arrocean/HLLCalc");
    appLargeIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_HLLCALC),IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));
    appSmallIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_HLLCALC),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    if(!appLargeIcon||!appSmallIcon){MessageBoxW(nullptr,i18n::Wide(L"程序图标资源缺失，请重新构建。").c_str(),L"HLLCalc",MB_ICONERROR);CoUninitialize();CloseHandle(mutex);return 1;}
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.style=CS_CLASSDC;cls.lpfnWndProc=WindowProc;cls.hInstance=instance;
    cls.lpszClassName=L"HLLCalcNativeWindow";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=appLargeIcon;cls.hIconSm=appSmallIcon;RegisterClassExW(&cls);
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,L"HLLCalc — 火炮计算与准星",WS_POPUP|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX,
        work.left+(work.right-work.left-S(1000))/2,work.top+(work.bottom-work.top-S(700))/2,S(1000),S(700),nullptr,nullptr,instance,nullptr);
    bool imguiWin32=false,imguiDX=false,uiReady=false;int exitCode=0;
    if(!window||!CreateGraphics())exitCode=1;
    if(!exitCode){
        MARGINS margins{1,1,1,1};DwmExtendFrameIntoClientArea(window,&margins);
        int corner=2;DwmSetWindowAttribute(window,static_cast<DWMWINDOWATTRIBUTE>(33),&corner,sizeof(corner));
        BOOL dark=TRUE;DwmSetWindowAttribute(window,static_cast<DWMWINDOWATTRIBUTE>(20),&dark,sizeof(dark));
        const COLORREF noFrame=0xFFFFFFFE;
        DwmSetWindowAttribute(window,static_cast<DWMWINDOWATTRIBUTE>(34),&noFrame,sizeof(noFrame));
        DwmSetWindowAttribute(window,static_cast<DWMWINDOWATTRIBUTE>(35),&noFrame,sizeof(noFrame));
        SetWindowPos(window,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        if(!LoadFont())exitCode=1;
        if(!exitCode){imguiWin32=ImGui_ImplWin32_Init(window);imguiDX=ImGui_ImplDX11_Init(device,context);if(!imguiWin32||!imguiDX)exitCode=1;}
        if(!exitCode&&!crosshair::Initialize(instance,iniPath))exitCode=1;
        if(!exitCode){uiReady=ui::Initialize(iniPath,scale);if(!uiReady)exitCode=1;else ui::SetAnimationSuppressed(testProfile&&!animationTest);}
    }
    if(exitCode){MessageBoxW(window,i18n::Wide(L"无法初始化界面。请检查 DirectX 11 和系统中文字体。").c_str(),L"HLLCalc",MB_ICONERROR);}
    else {
        // A conflict must not make the settings UI inaccessible. The user can
        // record another key, or reach the UI through its tray icon.
        hotkey::Initialize(window,iniPath);
            ready=true;Tray(true);SetTimer(window,1,1000,nullptr);ShowCalculator();
            if(testing)SetTimer(window,2,700,nullptr);
            bool done=false;ULONGLONG nextFrame=0;
            while(!done){
                MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                    if(message.message==WM_QUIT){exitCode=static_cast<int>(message.wParam);done=true;break;}
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                if(done)break;
                if(!IsWindowVisible(window)||IsIconic(window)){WaitMessage();continue;}
                const auto now=GetTickCount64();
                if(now<nextFrame){MsgWaitForMultipleObjects(0,nullptr,FALSE,static_cast<DWORD>(nextFrame-now),QS_ALLINPUT);continue;}
                nextFrame=now+16;
                if(resizeWidth&&resizeHeight){
                    ReleaseTarget();HRESULT result=swapChain->ResizeBuffers(0,resizeWidth,resizeHeight,DXGI_FORMAT_UNKNOWN,0);resizeWidth=resizeHeight=0;
                    if(FAILED(result)||!CreateTarget()){exitCode=1;break;}
                }
                ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
                const auto action=ui::Frame();ImGui::Render();++frames;
                const float clear[]{0.039f,0.039f,0.039f,1.0f};context->OMSetRenderTargets(1,&renderTarget,nullptr);context->ClearRenderTargetView(renderTarget,clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());HRESULT present=swapChain->Present(1,0);
                if(FAILED(present)){exitCode=1;break;}
                if(frames==1)updater::AcknowledgeReady();
                if(action==ui::Action::Hide)HideCalculator();
                else if(action==ui::Action::Quit)DestroyWindow(window);
                else if(action==ui::Action::InstallUpdate){
                    if(!ui::Save())MessageBoxW(window,i18n::Wide(L"保存失败，请检查配置目录写入权限").c_str(),L"HLLCalc",MB_ICONERROR);
                    else if(updater::Install())DestroyWindow(window);
                }
            }
            if(exitCode==1)MessageBoxW(window,i18n::Wide(L"图形设备不可用，程序将保存配置并退出。").c_str(),L"HLLCalc",MB_ICONERROR);
    }
    if(IsWindow(window))DestroyWindow(window);
    ready=false;if(uiReady)ui::Shutdown();crosshair::Shutdown();updater::Shutdown();
    if(imguiDX)ImGui_ImplDX11_Shutdown();if(imguiWin32)ImGui_ImplWin32_Shutdown();if(ImGui::GetCurrentContext())ImGui::DestroyContext();
    CleanupGraphics();UnregisterClassW(cls.lpszClassName,instance);CoUninitialize();CloseHandle(mutex);return exitCode;
}
