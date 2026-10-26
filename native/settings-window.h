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
    bool ProcessDialogMessage(MSG&message);
private:
    HWND hwnd=nullptr,license=nullptr,popup=nullptr,popupAnchor=nullptr,popupBody=nullptr,popupClose=nullptr;
    HINSTANCE instance=nullptr;
    HFONT font=nullptr;
    HBRUSH backgroundBrush=nullptr,cardBrush=nullptr;
    Preferences preferences;
    WindowCallbacks callbacks;
    int width=560,height=408,popupTopic=0,popupHeight=180,hoveredHelpRow=-1;
    float scale=1;
    bool dark=false,highContrast=false,loading=false,arranging=false;
    UINT taskbarMessage=0;
    HWND startup=nullptr,enabled=nullptr,memory=nullptr,duration=nullptr,exitButton=nullptr,retry=nullptr,github=nullptr,notices=nullptr,about=nullptr,connection=nullptr;
    std::array<HWND,4> help{};
    std::wstring popupTitle,popupText;
    struct Palette{COLORREF background,card,text,secondary,line,accent,selection,danger;}colors{};
    std::mutex statusMutex;
    int status=0;
    std::wstring statusText=L"正在启动…",statusDetail;
    int Px(float value)const{return int(std::lround(value*scale));}
    void Theme();void Layout();void Paint(HDC dc);void Save();
    void DrawButton(const DRAWITEMSTRUCT&item);void DrawCombo(const DRAWITEMSTRUCT&item);
    void ShowLicenses();bool ConfirmStartupLocation();
    void ShowHelp(int topic,HWND anchor);void CloseHelp(bool restoreFocus=true);
    void UpdateHelp();void PaintHelp(HDC dc);void RefreshStatus();
    void SetHoveredHelpRow(int row);void TrackHelpHover(HWND control,LPARAM position);void RefreshHelpHover();
    HWND Button(const wchar_t*label,int id,DWORD style=BS_OWNERDRAW,HWND parent=nullptr);
    LRESULT Message(UINT message,WPARAM wp,LPARAM lp);
    static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK ToggleProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK ComboProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK MenuProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK SliderProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK ActionProcedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
    static LRESULT CALLBACK PopupProcedure(HWND,UINT,WPARAM,LPARAM);
};
}
