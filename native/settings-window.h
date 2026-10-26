#pragma once
#include "backend.h"
#include <memory>
#include <mutex>

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
    struct Browser;
    std::shared_ptr<Browser> browser;
    HWND hwnd=nullptr;
    HINSTANCE instance=nullptr;
    Preferences preferences;
    WindowCallbacks callbacks;
    UINT taskbarMessage=0;
    std::mutex statusMutex;
    int status=0;
    std::wstring statusText=L"正在启动…",statusDetail;
    bool ConfirmStartupLocation();
    LRESULT Message(UINT message,WPARAM wp,LPARAM lp);
    static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);
};
}
