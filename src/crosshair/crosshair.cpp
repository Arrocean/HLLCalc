// SPDX-License-Identifier: GPL-3.0-only
// UTF-8. Native reimplementation referencing AimerHLL/hud/crosshair_overlay.py.
// Original project's crosshair configuration and visual concepts are GPL.
// No settings UI, ImGui, Python, graphics hook or game-process dependency.
#include "crosshair/crosshair.h"
#include "i18n/i18n.h"
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <limits>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace crosshair {
namespace {
constexpr wchar_t kOverlayClass[] = L"HLLCalc.Crosshair.Overlay.1";
constexpr COLORREF kTransparent = RGB(1, 1, 1);
struct IntField { const wchar_t* name; int Config::*member; int low, high; };
constexpr IntField fields[] = {
    {L"Enabled", &Config::enabled,0,1}, {L"Dot", &Config::dot,0,1},
    {L"Shape", &Config::shape,0,2}, {L"Cross", &Config::cross,0,5},
    {L"Ring", &Config::ring,0,1}, {L"Length", &Config::length,0,500},
    {L"Thickness", &Config::thickness,1,100}, {L"Gap", &Config::gap,-500,500},
    {L"DotSize", &Config::dotSize,1,200}, {L"Radius", &Config::radius,1,1000},
    {L"RingThickness", &Config::ringThickness,1,100}, {L"Opacity", &Config::opacity,0,255},
    {L"Outline", &Config::outline,0,1}, {L"OutlineThickness", &Config::outlineThickness,1,20},
    {L"Rotation", &Config::rotation,0,359}, {L"ScalePercent", &Config::scale,10,500},
    {L"OffsetX", &Config::offsetX,-16000,16000}, {L"OffsetY", &Config::offsetY,-16000,16000},
    {L"Monitor", &Config::monitor,0,64}, {L"Red", &Config::red,0,255},
    {L"Green", &Config::green,0,255}, {L"Blue", &Config::blue,0,255},
    {L"Ruler", &Config::ruler,0,1}
};
struct Monitor { RECT rect; std::wstring name; bool primary; };
struct Tick { std::wstring label; int pixels; };
HINSTANCE instance = nullptr;
HWND overlay = nullptr;
Config config;
std::wstring iniFile;
std::vector<Monitor> monitors;
bool registeredOverlay = false;

bool ParseInt(const std::wstring& text, int& result) {
    const wchar_t* begin = text.c_str(); wchar_t* end = nullptr;
    const long long value = wcstoll(begin, &end, 10);
    if (begin == end) return false;
    while (*end && iswspace(*end)) ++end;
    if (*end) return false;
    result = static_cast<int>(std::clamp(value,
        static_cast<long long>((std::numeric_limits<int>::min)()),
        static_cast<long long>((std::numeric_limits<int>::max)())));
    return true;
}
void Normalize(Config& c) {
    for (const auto& f : fields) c.*(f.member) = std::clamp(c.*(f.member), f.low, f.high);
    if (!monitors.empty()) c.monitor = (std::min)(c.monitor, static_cast<int>(monitors.size())-1);
    if (c.rulerEntries.size() > 1024) c.rulerEntries.resize(1024);
    // Keep the single color-key RGB value reserved for transparent pixels.
    if (RGB(c.red,c.green,c.blue) == kTransparent) c.blue = 2;
}
bool Equal(const Config& a, const Config& b) {
    for (const auto& f : fields) if (a.*(f.member) != b.*(f.member)) return false;
    return a.rulerEntries == b.rulerEntries;
}
std::vector<Tick> ParseRuler(const std::wstring& text) {
    std::vector<Tick> ticks;
    size_t pos = 0;
    while (pos < text.size() && ticks.size() < 32) {
        auto end = text.find(L',',pos); if (end == std::wstring::npos) end = text.size();
        const auto sep = text.find(L':',pos);
        if (sep != std::wstring::npos && sep < end && sep > pos) {
            int pixels = 0;
            if (ParseInt(text.substr(sep+1,end-sep-1), pixels))
                ticks.push_back({text.substr(pos,(std::min)(sep-pos,size_t(24))),std::clamp(pixels,-2000,2000)});
        }
        pos = end + 1;
    }
    return ticks;
}
Config Load(const std::wstring& path) {
    Config next;
    wchar_t buffer[1100]{};
    for (const auto& f : fields) {
        const std::wstring def = std::to_wstring(next.*(f.member));
        GetPrivateProfileStringW(L"Crosshair",f.name,def.c_str(),buffer,1100,path.c_str());
        int v; if (ParseInt(buffer,v)) next.*(f.member) = v;
    }
    GetPrivateProfileStringW(L"Crosshair",L"RulerEntries",next.rulerEntries.c_str(),buffer,1100,path.c_str());
    next.rulerEntries = buffer; Normalize(next); return next;
}
bool WriteAll(HANDLE file, const void* bytes, DWORD length) {
    DWORD written = 0;
    return WriteFile(file,bytes,length,&written,nullptr) && written == length;
}
// Upgrade old ANSI INIs without discarding [Calculator], comments or other sections.
// Replacement is atomic; a failed conversion leaves the original file untouched.
bool EnsureUnicodeIni(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) return false;
        file = CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        const WORD bom = 0xfeff;
        const bool ok = WriteAll(file,&bom,sizeof(bom)); CloseHandle(file); return ok;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file,&size) || size.QuadPart > 16*1024*1024) {CloseHandle(file);return false;}
    std::vector<char> bytes(static_cast<size_t>(size.QuadPart)); DWORD read = 0;
    const bool readOK = bytes.empty() || (ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) && read == bytes.size());
    CloseHandle(file); if (!readOK) return false;
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xff && static_cast<unsigned char>(bytes[1]) == 0xfe) return true;
    // The original Win32 profile writer used system ANSI. Also accept UTF-8 BOM files.
    size_t offset = 0; UINT codepage = CP_ACP;
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xef && static_cast<unsigned char>(bytes[1]) == 0xbb && static_cast<unsigned char>(bytes[2]) == 0xbf) {offset=3;codepage=CP_UTF8;}
    std::wstring text;
    if (offset < bytes.size()) {
        const int length = static_cast<int>(bytes.size()-offset);
        const int count = MultiByteToWideChar(codepage,0,bytes.data()+offset,length,nullptr,0);
        if (!count) return false;
        text.resize(static_cast<size_t>(count));
        if (!MultiByteToWideChar(codepage,0,bytes.data()+offset,length,text.data(),count)) return false;
    }
    const std::wstring temporary = path + L".crosshair-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tmp";
    file = CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const WORD bom = 0xfeff;
    bool ok = WriteAll(file,&bom,sizeof(bom)) && (text.empty() || WriteAll(file,text.data(),static_cast<DWORD>(text.size()*sizeof(wchar_t))));
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (ok) {
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
        ok = MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!ok) DeleteFileW(temporary.c_str());
    return ok;
}
bool Save(const std::wstring& path, const Config& value) {
    if (path.empty() || !EnsureUnicodeIni(path)) return false;
    bool ok = true;
    for (const auto& f : fields) {
        const auto text = std::to_wstring(value.*(f.member));
        if (!WritePrivateProfileStringW(L"Crosshair",f.name,text.c_str(),path.c_str())) ok = false;
    }
    if (!WritePrivateProfileStringW(L"Crosshair",L"RulerEntries",value.rulerEntries.c_str(),path.c_str())) ok = false;
    if (!WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str())) ok = false;
    return ok;
}
void Error(HWND owner, const wchar_t* text) {
    MessageBoxW(owner,i18n::Wide(text).c_str(),i18n::Wide(L"准星配置").c_str(),MB_OK|MB_ICONERROR);
}
BOOL CALLBACK EnumMonitor(HMONITOR h, HDC, LPRECT, LPARAM) {
    MONITORINFOEXW mi{}; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(h,&mi)) monitors.push_back({mi.rcMonitor,mi.szDevice,(mi.dwFlags & MONITORINFOF_PRIMARY)!=0});
    return TRUE;
}
void EnumerateMonitors() {
    monitors.clear(); EnumDisplayMonitors(nullptr,nullptr,EnumMonitor,0);
    std::stable_sort(monitors.begin(),monitors.end(),[](const Monitor& a,const Monitor& b) { return a.primary && !b.primary; });
    if (monitors.empty()) monitors.push_back({{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)},L"主显示器",true});
    Normalize(config);
}
void UpdateOverlay() {
    if (!overlay || monitors.empty()) return;
    const RECT r = monitors[static_cast<size_t>(config.monitor)].rect;
    SetLayeredWindowAttributes(overlay,kTransparent,static_cast<BYTE>(config.opacity),LWA_COLORKEY|LWA_ALPHA);
    SetWindowPos(overlay,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,
        SWP_NOACTIVATE | (config.enabled ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    InvalidateRect(overlay,nullptr,FALSE);
}
void Draw(HDC dc, const RECT& bounds) {
    HBRUSH background = CreateSolidBrush(kTransparent);
    if (background) {FillRect(dc,&bounds,background);DeleteObject(background);}
    const int saved = SaveDC(dc);
    if (!saved) return;
    const double radians = config.rotation * 3.14159265358979323846 / 180.0;
    const double scale = config.scale / 100.0;
    XFORM xf{static_cast<FLOAT>(scale*std::cos(radians)),static_cast<FLOAT>(scale*std::sin(radians)),
        static_cast<FLOAT>(-scale*std::sin(radians)),static_cast<FLOAT>(scale*std::cos(radians)),
        static_cast<FLOAT>((bounds.right-bounds.left)/2.0+config.offsetX),
        static_cast<FLOAT>((bounds.bottom-bounds.top)/2.0+config.offsetY)};
    if (!SetGraphicsMode(dc,GM_ADVANCED) || !SetWorldTransform(dc,&xf)) {RestoreDC(dc,saved);return;}
    const COLORREF color = RGB(config.red,config.green,config.blue);
    HBRUSH foreground = CreateSolidBrush(color), black = CreateSolidBrush(RGB(0,0,0));
    if (!foreground || !black) {RestoreDC(dc,saved);if(foreground)DeleteObject(foreground);if(black)DeleteObject(black);return;}
    SelectObject(dc,foreground); SelectObject(dc,GetStockObject(NULL_PEN));
    auto box = [&](int x1,int y1,int x2,int y2) {
        if(config.outline) {const int o=config.outlineThickness;RECT r{x1-o,y1-o,x2+o,y2+o};FillRect(dc,&r,black);}
        RECT r{x1,y1,x2,y2};FillRect(dc,&r,foreground);
    };
    const int t = config.thickness, g = config.gap, len = config.length;
    if (config.cross != 5 && len > 0) {
        // 0 full; 1 T-down (no top); 2 T-up (no bottom); 3 horizontal; 4 vertical.
        if (config.cross != 1 && config.cross != 3) box(-t/2,-g-len,-t/2+t,-g);
        if (config.cross != 2 && config.cross != 3) box(-t/2,g,-t/2+t,g+len);
        if (config.cross != 4) {box(-g-len,-t/2,-g,-t/2+t);box(g,-t/2,g+len,-t/2+t);}
    }
    if (config.dot) {
        const int s = config.dotSize, lo = -s/2, hi = lo+s;
        if (config.shape == 0) box(lo,lo,hi,hi);
        else {
            auto dotShape = [&](int a,int b) {
                if(config.shape==1) Ellipse(dc,a,a,b,b);
                else {POINT p[]={{0,a},{b,0},{0,b},{a,0}};Polygon(dc,p,4);}
            };
            if(config.outline) {SelectObject(dc,black);dotShape(lo-config.outlineThickness,hi+config.outlineThickness);}
            SelectObject(dc,foreground);dotShape(lo,hi);
        }
    }
    if (config.ring) {
        SelectObject(dc,GetStockObject(NULL_BRUSH));
        const int r = config.radius;
        auto ring = [&](int width,COLORREF shade) {
            HPEN pen = CreatePen(PS_SOLID,width,shade); if(!pen)return;
            HGDIOBJ old = SelectObject(dc,pen);Ellipse(dc,-r,-r,r,r);SelectObject(dc,old);DeleteObject(pen);
        };
        if (config.outline) ring(config.ringThickness+2*config.outlineThickness,RGB(0,0,0));
        ring(config.ringThickness,color);
    }
    if (config.ruler) {
        const auto ticks = ParseRuler(config.rulerEntries);
        HPEN pen = CreatePen(PS_SOLID,1,color);
        HFONT font = CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
        HGDIOBJ oldPen = pen ? SelectObject(dc,pen) : nullptr;
        HGDIOBJ oldFont = font ? SelectObject(dc,font) : nullptr;
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
        int lo=0,hi=0;for(const auto& tick:ticks){lo=(std::min)(lo,tick.pixels);hi=(std::max)(hi,tick.pixels);}
        MoveToEx(dc,0,lo,nullptr);LineTo(dc,0,hi);
        for (const auto& tick:ticks) {
            MoveToEx(dc,-6,tick.pixels,nullptr);LineTo(dc,7,tick.pixels);
            if (config.outline) {
                SetTextColor(dc,RGB(0,0,0));
                for(int dx=-1;dx<=1;++dx) for(int dy=-1;dy<=1;++dy)
                    if(dx || dy) TextOutW(dc,11+dx,tick.pixels-7+dy,tick.label.c_str(),static_cast<int>(tick.label.size()));
                SetTextColor(dc,color);
            }
            TextOutW(dc,11,tick.pixels-7,tick.label.c_str(),static_cast<int>(tick.label.size()));
        }
        if(font){SelectObject(dc,oldFont);DeleteObject(font);}if(pen){SelectObject(dc,oldPen);DeleteObject(pen);}
    }
    // Restore world transform AND selected objects before deleting owned brushes.
    RestoreDC(dc,saved);DeleteObject(black);DeleteObject(foreground);
}
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);
        Draw(dc,r);EndPaint(hwnd,&ps);return 0;
    }
    case WM_DISPLAYCHANGE: EnumerateMonitors();UpdateOverlay();return 0;
    case WM_DPICHANGED: UpdateOverlay();return 0; // overlay geometry is physical pixels
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
bool ChooseFile(HWND owner, bool exporting, std::wstring& result) {
    wchar_t path[32768]{};
    OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=owner;
    ofn.lpstrFilter=i18n::Current()==i18n::Language::English?L"INI presets (*.ini)\0*.ini\0All files\0*.*\0\0":L"INI 配置 (*.ini)\0*.ini\0所有文件\0*.*\0\0";
    ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.lpstrDefExt=L"ini";
    ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(exporting?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if (!(exporting?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn))) {
        if (CommDlgExtendedError()!=0) Error(owner,L"无法打开文件选择对话框。");
        return false;
    }
    result=path;return true;
}
} // namespace

bool Initialize(HINSTANCE appInstance, const std::wstring& path) {
    if (overlay) return true;
    if (path.empty()) return false;
    instance=appInstance?appInstance:GetModuleHandleW(nullptr);
    const DWORD required=GetFullPathNameW(path.c_str(),0,nullptr,nullptr);
    if(!required)return false;
    std::wstring absolute(required,L'\0');
    const DWORD count=GetFullPathNameW(path.c_str(),required,absolute.data(),nullptr);
    if(!count || count>=required)return false;
    absolute.resize(count);iniFile=absolute;
    EnumerateMonitors();config=Load(iniFile);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=OverlayProc;wc.lpszClassName=kOverlayClass;
    registeredOverlay=RegisterClassExW(&wc)!=0;
    if(!registeredOverlay && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
        kOverlayClass,L"准星叠加层",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);
    if(!overlay){Shutdown();return false;}
    UpdateOverlay();return true;
}
Config GetConfig() { return config; }
void ApplyConfig(const Config& value, bool persist) {
    Config next=value;Normalize(next);
    if (!Equal(next,config)) {config=next;UpdateOverlay();}
    if (persist && !SaveConfig()) Error(nullptr,L"无法保存准星配置，请检查 INI 路径与写入权限。");
}
bool SaveConfig() { return Save(iniFile,config); }
std::vector<std::wstring> MonitorNames() {
    std::vector<std::wstring> names;names.reserve(monitors.size());
    for(size_t i=0;i<monitors.size();++i)
        names.push_back(std::to_wstring(i+1)+L" - "+monitors[i].name+(monitors[i].primary?i18n::Wide(L"（主）"):L""));
    return names;
}
int MonitorHeightPixels() {
    if(monitors.empty())return (std::max)(1,GetSystemMetrics(SM_CYSCREEN));
    const auto index=static_cast<size_t>(std::clamp(config.monitor,0,static_cast<int>(monitors.size())-1));
    return static_cast<int>(monitors[index].rect.bottom-monitors[index].rect.top);
}
bool ImportPreset(HWND owner) {
    std::wstring path;if(!ChooseFile(owner,false,path))return false;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){Error(owner,L"无法读取所选准星配置。");return false;}CloseHandle(file);
    wchar_t section[32768]{};
    const DWORD length=GetPrivateProfileSectionW(L"Crosshair",section,32768,path.c_str());
    if(length==0 || length>=32766){Error(owner,L"所选文件缺少有效的 [Crosshair] 配置，或配置过大。");return false;}
    const Config next=Load(path);
    if(!Save(iniFile,next)){Error(owner,L"无法保存导入的准星配置，当前准星保持不变。");return false;}
    ApplyConfig(next);return true;
}
bool ExportPreset(HWND owner) {
    std::wstring path;if(!ChooseFile(owner,true,path))return false;
    if(!Save(path,config)){Error(owner,L"导出准星配置失败，请检查目标文件权限。");return false;}
    return true;
}
void RefreshTopmost() {
    if(overlay && config.enabled) SetWindowPos(overlay,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOREDRAW);
}
void Shutdown() {
    if(overlay){SaveConfig();DestroyWindow(overlay);overlay=nullptr;}
    if(registeredOverlay){UnregisterClassW(kOverlayClass,instance);registeredOverlay=false;}
    monitors.clear();iniFile.clear();
}
HWND OverlayWindow() { return overlay; }
} // namespace crosshair
