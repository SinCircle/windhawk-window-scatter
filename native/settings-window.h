#pragma once
#include "backend.h"
#include <commctrl.h>
#include <gdiplus.h>
#include <mutex>
#include <cmath>

namespace native {
struct WindowCallbacks {
    std::function<bool(const Preferences&)> save;
    std::function<void()> exit,reconnect;
    std::function<void(WPARAM,LPARAM)> bridgeStatus;
};
class SettingsWindow {
public:
    bool Create(HINSTANCE instance,const Preferences&preferences,WindowCallbacks callbacks);
    void Show();
    void SetStatus(int state,const std::wstring&text,const std::wstring&detail=L"");
    void Destroy();
    HWND Handle()const{return hwnd;}
    const Preferences&Values()const{return preferences;}
    bool IsVisible()const{return hwnd&&IsWindowVisible(hwnd);}
private:
    HWND hwnd=nullptr,license=nullptr;
    HINSTANCE instance=nullptr;
    HFONT font=nullptr;
    HBRUSH backgroundBrush=nullptr;
    Preferences preferences;
    WindowCallbacks callbacks;
    int width=600,height=524;
    float scale=1;
    bool dark=false,highContrast=false,loading=false,arranging=false;
    UINT taskbarMessage=0;
    HWND startup=nullptr,enabled=nullptr,memory=nullptr,duration=nullptr,radius=nullptr,exitButton=nullptr,retry=nullptr,github=nullptr,notices=nullptr;
    struct Palette{COLORREF background,text,secondary,line,accent,selection,danger;}colors{};
    std::mutex statusMutex;
    int status=0;
    std::wstring statusText=L"正在启动…",statusDetail;
    int Px(float value)const{return int(std::lround(value*scale));}
    void Theme();void Layout();void Paint(HDC dc);void Save();
    void DrawButton(const DRAWITEMSTRUCT&item);void DrawCombo(const DRAWITEMSTRUCT&item);
    void ShowLicenses();
    HWND Button(const wchar_t*label,int id,DWORD style=BS_OWNERDRAW);
    LRESULT Message(UINT message,WPARAM wp,LPARAM lp);
    static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK ToggleProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK ComboProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK MenuProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
};
}
