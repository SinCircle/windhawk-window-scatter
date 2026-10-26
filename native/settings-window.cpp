#include "settings-window.h"
#include <WebView2.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <deque>
#include <sstream>

namespace native {
namespace {
constexpr UINT kRefresh=WM_APP+104,kCommand=WM_APP+140,kFailure=WM_APP+141;
constexpr UINT_PTR kLoadTimer=140;
template<class I>const IID&InterfaceId();
#define WEBVIEW_IID(I) template<>const IID&InterfaceId<I>(){return IID_##I;}
WEBVIEW_IID(ICoreWebView2WebMessageReceivedEventHandler)
WEBVIEW_IID(ICoreWebView2NavigationStartingEventHandler)
WEBVIEW_IID(ICoreWebView2NewWindowRequestedEventHandler)
WEBVIEW_IID(ICoreWebView2PermissionRequestedEventHandler)
WEBVIEW_IID(ICoreWebView2ProcessFailedEventHandler)
WEBVIEW_IID(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)
WEBVIEW_IID(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)
#undef WEBVIEW_IID
template<class T>struct Com {
    T*value=nullptr;
    ~Com(){Reset();}
    Com()=default;Com(const Com&)=delete;Com&operator=(const Com&)=delete;
    void Reset(){if(value){auto*old=value;value=nullptr;old->Release();}}
    void Set(T*next){Reset();value=next;if(value)value->AddRef();}
    T**Out(){Reset();return &value;}
    T*operator->()const{return value;}explicit operator bool()const{return value!=nullptr;}
};
template<class I,class...Args>class Callback final:public I {
    std::atomic<ULONG>references{1};std::function<HRESULT(Args...)>function;
public:
    explicit Callback(std::function<HRESULT(Args...)>fn):function(std::move(fn)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void**out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown||id==InterfaceId<I>()){*out=static_cast<I*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++references;}
    ULONG STDMETHODCALLTYPE Release()override{auto count=--references;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE Invoke(Args...args)override{return function(args...);}
};
template<class I,class...Args,class F>HRESULT WithCallback(F&&fn,const std::function<HRESULT(I*)>&use){
    auto*callback=new Callback<I,Args...>(std::forward<F>(fn));HRESULT hr=use(callback);callback->Release();return hr;
}
std::wstring Utf8(const std::vector<BYTE>&bytes){
    int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(bytes.data()),int(bytes.size()),nullptr,0);
    if(!length)return {};std::wstring result(length,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(bytes.data()),int(bytes.size()),result.data(),length);return result;
}
std::wstring Json(const std::wstring&text){
    std::wstring out=L"\"";wchar_t escaped[8];
    for(wchar_t c:text){
        if(c==L'"'||c==L'\\'){out+=L'\\';out+=c;}
        else if(c<32||c==L'<'||c==0x2028||c==0x2029){swprintf_s(escaped,L"\\u%04x",unsigned(c));out+=escaped;}
        else out+=c;
    }
    return out+L'"';
}
std::wstring HexColor(COLORREF color){wchar_t text[8];swprintf_s(text,L"#%02x%02x%02x",GetRValue(color),GetGValue(color),GetBValue(color));return text;}
struct Command {unsigned sequence=0;std::wstring action;int value=0;};
bool ParseCommand(const std::wstring&text,Command&command){
    if(text.empty()||text.size()>96)return false;
    auto first=text.find(L'|'),last=text.find(L'|',first==text.npos?0:first+1);
    if(first==text.npos||last==text.npos||text.find(L'|',last+1)!=text.npos)return false;
    auto number=[](const std::wstring&s,unsigned&n){if(s.empty()||s.size()>10)return false;n=0;for(auto c:s){if(c<L'0'||c>L'9'||n>(2147483647u-unsigned(c-L'0'))/10)return false;n=n*10+unsigned(c-L'0');}return true;};
    unsigned value=0;if(!number(text.substr(0,first),command.sequence)||!command.sequence||!number(text.substr(last+1),value))return false;
    command.action=text.substr(first+1,last-first-1);command.value=int(value);
    if(command.action==L"enabled"||command.action==L"startup")return value<=1;
    if(command.action==L"memory")return value<=2;
    if(command.action==L"duration")return value<=900;
    return value==0&&(command.action==L"ready"||command.action==L"reconnect"||command.action==L"exit"||command.action==L"close"||command.action==L"github"||command.action==L"licenses");
}
}

// COM callbacks hold only a weak reference. Closing the panel invalidates the
// generation before releasing its controller, including an in-flight creation.
struct SettingsWindow::Browser:std::enable_shared_from_this<Browser> {
    SettingsWindow*owner=nullptr;
    Com<ICoreWebView2Environment>environment;
    Com<ICoreWebView2Controller>controller;
    Com<ICoreWebView2>web;
    unsigned generation=0,ack=0;
    bool opening=false,ready=false,dark=false,contrast=false,initialNavigationPending=false;
    COLORREF background=RGB(245,245,247);
    std::wstring failure;
    std::deque<Command>commands;
    explicit Browser(SettingsWindow*window):owner(window){}
    void Close(){
        ++generation;opening=false;ready=false;commands.clear();ack=0;
        if(owner&&owner->hwnd)KillTimer(owner->hwnd,kLoadTimer);
        if(controller)controller->Close();web.Reset();controller.Reset();environment.Reset();
    }
    void Fail(HRESULT hr){
        std::wostringstream detail;detail<<L"无法打开设置界面（0x"<<std::hex<<unsigned(hr)<<L"）。\n\n请确认已安装 Microsoft Edge WebView2 Runtime，然后重新打开程序。";
        failure=detail.str();Log(failure);PostMessage(owner->hwnd,kFailure,generation,0);
    }
    void Bounds(){if(controller){RECT rect{};GetClientRect(owner->hwnd,&rect);controller->put_Bounds(rect);}}
    void Theme(){
        DWORD light=1,size=sizeof(light);RegGetValue(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);dark=!light;
        HIGHCONTRAST high{sizeof(high)};SystemParametersInfo(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);contrast=(high.dwFlags&HCF_HIGHCONTRASTON)!=0;
        background=contrast?GetSysColor(COLOR_WINDOW):dark?RGB(29,29,31):RGB(245,245,247);
        BOOL immersive=dark&&!contrast;DwmSetWindowAttribute(owner->hwnd,20,&immersive,sizeof(immersive));
        if(controller){Com<ICoreWebView2Controller2>v2;
            if(SUCCEEDED(controller->QueryInterface(IID_ICoreWebView2Controller2,reinterpret_cast<void**>(v2.Out()))))
                v2->put_DefaultBackgroundColor({255,GetRValue(background),GetGValue(background),GetBValue(background)});
        }
    }
    std::wstring Snapshot(const std::wstring&error=L""){
        int status;std::wstring label,detail;
        {std::lock_guard lock(owner->statusMutex);status=owner->status;label=owner->statusText;detail=owner->statusDetail;}
        const auto&p=owner->preferences;
        return L"{\"type\":\"state\",\"ack\":"+std::to_wstring(ack)+L",\"enabled\":"+(p.enabled?L"true":L"false")+
            L",\"startup\":"+(StartupEnabled()?L"true":L"false")+L",\"memory\":"+std::to_wstring(p.memory)+L",\"duration\":"+std::to_wstring(p.duration)+
            L",\"status\":"+std::to_wstring(status)+L",\"statusText\":"+Json(label)+L",\"statusDetail\":"+Json(detail)+L",\"version\":"+Json(kVersion)+
            L",\"theme\":"+Json(contrast?L"contrast":dark?L"dark":L"light")+L",\"systemBackground\":"+Json(HexColor(GetSysColor(COLOR_WINDOW)))+
            L",\"systemText\":"+Json(HexColor(GetSysColor(COLOR_WINDOWTEXT)))+L",\"systemAccent\":"+Json(HexColor(GetSysColor(COLOR_HIGHLIGHT)))+
            L",\"systemAccentText\":"+Json(HexColor(GetSysColor(COLOR_HIGHLIGHTTEXT)))+L",\"error\":"+Json(error)+L"}";
    }
    void Send(const std::wstring&error=L""){if(ready&&web)web->PostWebMessageAsJson(Snapshot(error).c_str());}
    void Reveal(){
        if(!ready||!controller)return;
        if(IsIconic(owner->hwnd))ShowWindow(owner->hwnd,SW_RESTORE);
        controller->put_IsVisible(TRUE);Bounds();
        // SWP_SHOWWINDOW also works after a first launch with STARTUPINFO SW_HIDE.
        SetWindowPos(owner->hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
        SetForegroundWindow(owner->hwnd);controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
    void Configure(){
        HRESULT hr=controller->get_CoreWebView2(web.Out());if(FAILED(hr)){Fail(hr);return;}
        Theme();Bounds();controller->put_IsVisible(FALSE);controller->put_ZoomFactor(1.0);
        Com<ICoreWebView2Settings>settings;web->get_Settings(settings.Out());
        if(settings){settings->put_AreHostObjectsAllowed(FALSE);settings->put_AreDefaultScriptDialogsEnabled(FALSE);settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);settings->put_IsZoomControlEnabled(FALSE);settings->put_AreDevToolsEnabled(FALSE);
            Com<ICoreWebView2Settings3>s3;if(SUCCEEDED(settings->QueryInterface(IID_ICoreWebView2Settings3,reinterpret_cast<void**>(s3.Out()))))s3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
        }
        auto weak=weak_from_this();unsigned current=generation;EventRegistrationToken token{};
        WithCallback<ICoreWebView2WebMessageReceivedEventHandler,ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs*>(
            [weak,current](auto*,auto*args){
                auto self=weak.lock();if(!self||self->generation!=current)return S_OK;
                LPWSTR source=nullptr,message=nullptr;args->get_Source(&source);args->TryGetWebMessageAsString(&message);
                Command command;bool valid=source&&std::wstring(source)==L"about:blank"&&message&&ParseCommand(message,command);
                if(source)CoTaskMemFree(source);if(message)CoTaskMemFree(message);
                if(valid&&self->commands.size()<256){self->commands.push_back(std::move(command));PostMessage(self->owner->hwnd,kCommand,current,0);}
                return S_OK;
            },[&](auto*callback){return web->add_WebMessageReceived(callback,&token);});
        WithCallback<ICoreWebView2NavigationStartingEventHandler,ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs*>(
            [weak,current](auto*,auto*args){LPWSTR uri=nullptr;args->get_Uri(&uri);auto self=weak.lock();BOOL userInitiated=TRUE;args->get_IsUserInitiated(&userInitiated);
                // NavigateToString reports a data URI in NavigationStarting;
                // the resulting document and WebMessage source are about:blank.
                bool local=uri&&(std::wstring(uri)==L"about:blank"||std::wstring(uri).rfind(L"data:text/html;charset=utf-8;base64,",0)==0);
                bool allowed=self&&self->generation==current&&self->initialNavigationPending&&!userInitiated&&local;
                if(allowed)self->initialNavigationPending=false;
                else Log(L"Blocked unexpected settings navigation.");
                if(uri)CoTaskMemFree(uri);args->put_Cancel(!allowed);return S_OK;
            },[&](auto*callback){return web->add_NavigationStarting(callback,&token);});
        WithCallback<ICoreWebView2NewWindowRequestedEventHandler,ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs*>(
            [](auto*,auto*args){args->put_Handled(TRUE);return S_OK;},[&](auto*callback){return web->add_NewWindowRequested(callback,&token);});
        WithCallback<ICoreWebView2PermissionRequestedEventHandler,ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs*>(
            [](auto*,auto*args){args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);return S_OK;},[&](auto*callback){return web->add_PermissionRequested(callback,&token);});
        WithCallback<ICoreWebView2ProcessFailedEventHandler,ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*>(
            [weak,current](auto*,auto*){auto self=weak.lock();if(self&&self->generation==current)self->Fail(E_FAIL);return S_OK;},[&](auto*callback){return web->add_ProcessFailed(callback,&token);});
        std::vector<BYTE>bytes;if(!ReadResource(owner->instance,203,bytes)){Fail(E_FAIL);return;}
        auto html=Utf8(bytes);auto marker=html.find(L"/*NATIVE_STATE*/null");
        if(marker==html.npos){Fail(E_FAIL);return;}html.replace(marker,20,Snapshot());
        initialNavigationPending=true;hr=web->NavigateToString(html.c_str());if(FAILED(hr))Fail(hr);
    }
    void Open(){
        if(ready){Send();Reveal();return;}if(opening)return;opening=true;++generation;Theme();
        std::filesystem::path path;std::wstring error;
        if(!ExtractFrontendLoader(owner->instance,path,error)){failure=error;PostMessage(owner->hwnd,kFailure,generation,0);return;}
        // Keep the small official loader mapped until process exit: an async COM
        // completion may still be returning after a panel has been closed.
        static HMODULE loader=LoadLibraryEx(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        using CreateEnvironment=HRESULT(STDAPICALLTYPE*)(PCWSTR,PCWSTR,ICoreWebView2EnvironmentOptions*,ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
        auto create=loader?reinterpret_cast<CreateEnvironment>(GetProcAddress(loader,"CreateCoreWebView2EnvironmentWithOptions")):nullptr;
        if(!create){Fail(HRESULT_FROM_WIN32(GetLastError()));return;}
        auto profile=DataDirectory()/L"Frontend"/L"Profile";unsigned current=generation;auto weak=weak_from_this();
        SetTimer(owner->hwnd,kLoadTimer,20000,nullptr);
        auto hr=WithCallback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,HRESULT,ICoreWebView2Environment*>(
            [weak,current](HRESULT result,ICoreWebView2Environment*env){
                auto self=weak.lock();if(!self||self->generation!=current)return S_OK;
                if(FAILED(result)||!env){self->Fail(FAILED(result)?result:E_FAIL);return S_OK;}self->environment.Set(env);
                auto created=WithCallback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,HRESULT,ICoreWebView2Controller*>(
                    [weak,current](HRESULT result,ICoreWebView2Controller*control){
                        auto self=weak.lock();if(!self||self->generation!=current){if(control)control->Close();return S_OK;}
                        if(FAILED(result)||!control){self->Fail(FAILED(result)?result:E_FAIL);return S_OK;}self->controller.Set(control);self->Configure();return S_OK;
                    },[&](auto*callback){return env->CreateCoreWebView2Controller(self->owner->hwnd,callback);});
                if(FAILED(created))self->Fail(created);return S_OK;
            },[&](auto*callback){return create(nullptr,profile.c_str(),nullptr,callback);});
        if(FAILED(hr))Fail(hr);
    }
    // Dispatched after WebMessageReceived returns. Native dialogs must never
    // start a nested message loop from inside a WebView2 callback.
    void Execute(){
        if(commands.empty())return;auto command=std::move(commands.front());commands.pop_front();
        if(command.sequence<=ack)return;ack=command.sequence;
        if(command.action==L"ready"){ready=true;opening=false;KillTimer(owner->hwnd,kLoadTimer);Send();Reveal();return;}
        if(!ready)return;
        if(command.action==L"close"){ShowWindow(owner->hwnd,SW_HIDE);Close();return;}
        if(command.action==L"exit"){if(owner->callbacks.exit)owner->callbacks.exit();return;}
        if(command.action==L"github"){ShellExecute(owner->hwnd,L"open",L"https://github.com/SinCircle/windhawk-window-scatter",nullptr,nullptr,SW_SHOWNORMAL);return;}
        if(command.action==L"reconnect"){if(owner->callbacks.reconnect)owner->callbacks.reconnect();Send();return;}
        if(command.action==L"licenses"){
            std::vector<BYTE>bytes;ReadResource(owner->instance,202,bytes);
            web->PostWebMessageAsJson((L"{\"type\":\"licenses\",\"text\":"+Json(Utf8(bytes))+L"}").c_str());return;
        }
        std::wstring error;
        if(command.action==L"startup"){
            if((!command.value||owner->ConfirmStartupLocation())&&!SetStartup(command.value!=0))error=L"无法更改开机启动设置。";
        }else{
            auto previous=owner->preferences;auto next=previous;
            if(command.action==L"enabled")next.enabled=command.value!=0;
            else if(command.action==L"memory")next.memory=command.value;
            else if(command.action==L"duration")next.duration=command.value;
            if(owner->callbacks.save&&owner->callbacks.save(next))owner->preferences=next;
            else error=L"设置未能保存，请检查当前账户的写入权限。";
        }
        Send(error);
    }
};

bool SettingsWindow::Create(HINSTANCE handle,const Preferences&values,WindowCallbacks actions){
    instance=handle;preferences=values;callbacks=std::move(actions);browser=std::make_shared<Browser>(this);
    WNDCLASSEX wc{sizeof(wc)};wc.hInstance=instance;wc.lpfnWndProc=Procedure;wc.lpszClassName=kManagerClass;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hIcon=reinterpret_cast<HICON>(LoadImage(instance,MAKEINTRESOURCE(101),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));wc.hIconSm=wc.hIcon;
    if(!RegisterClassEx(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    hwnd=CreateWindowEx(WS_EX_APPWINDOW,kManagerClass,L"Window Scatter 设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,560,408,nullptr,nullptr,instance,this);
    if(!hwnd)return false;taskbarMessage=RegisterWindowMessage(L"TaskbarCreated");
    UINT dpi=GetDpiForWindow(hwnd);RECT bounds{0,0,MulDiv(560,dpi,96),MulDiv(408,dpi,96)};
    AdjustWindowRectExForDpi(&bounds,GetWindowLong(hwnd,GWL_STYLE),FALSE,GetWindowLong(hwnd,GWL_EXSTYLE),dpi);
    POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfo(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY),&monitor);
    int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
    SetWindowPos(hwnd,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-w)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-h)/2,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    DWORD corner=2;DwmSetWindowAttribute(hwnd,33,&corner,sizeof(corner));browser->Theme();return true;
}
void SettingsWindow::Show(){if(browser)browser->Open();}
void SettingsWindow::SetStatus(int value,const std::wstring&text,const std::wstring&detail){
    {std::lock_guard lock(statusMutex);status=value;statusText=text;statusDetail=detail;}
    if(hwnd){SetProp(hwnd,L"WindowScatter.ConnectionState.v1",reinterpret_cast<HANDLE>(ULONG_PTR(value)));PostMessage(hwnd,kRefresh,0,0);}
}
void SettingsWindow::Destroy(){
    if(browser){browser->Close();browser->owner=nullptr;browser.reset();}
    if(hwnd)DestroyWindow(hwnd);hwnd=nullptr;
}
bool SettingsWindow::ProcessDialogMessage(MSG&){return false;}
bool SettingsWindow::ConfirmStartupLocation(){
    auto location=CurrentStartupLocation();if(location==StartupLocation::Permanent)return true;
    std::wstring message=location==StartupLocation::Downloads?L"程序还在下载目录，清理下载文件后，开机启动可能失效。":L"程序还在临时目录，系统清理后，开机启动可能失效。";
    message+=L"\n\n建议先移到固定文件夹，再开启自启。";std::wstring path=ExecutablePath();
    const TASKDIALOG_BUTTON buttons[]={{IDOK,L"仍然开启"},{IDCANCEL,L"取消"}};
    TASKDIALOGCONFIG config{sizeof(config)};config.hwndParent=hwnd;
    config.dwFlags=TDF_POSITION_RELATIVE_TO_WINDOW|TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT;
    config.pszWindowTitle=L"Window Scatter";config.pszMainIcon=TD_WARNING_ICON;config.pszMainInstruction=L"先把程序移到固定位置？";config.pszContent=message.c_str();
    config.pszExpandedInformation=path.c_str();config.pszExpandedControlText=L"收起位置";config.pszCollapsedControlText=L"查看位置";
    config.pButtons=buttons;config.cButtons=ARRAYSIZE(buttons);config.nDefaultButton=IDCANCEL;int selected=IDCANCEL;
    if(FAILED(TaskDialogIndirect(&config,&selected,nullptr,nullptr)))selected=MessageBox(hwnd,(message+L"\n\n仍要开启吗？").c_str(),L"Window Scatter",MB_ICONWARNING|MB_OKCANCEL|MB_DEFBUTTON2);
    return selected==IDOK;
}
LRESULT SettingsWindow::Message(UINT message,WPARAM wp,LPARAM lp){
    if(taskbarMessage&&message==taskbarMessage){if(callbacks.reconnect)callbacks.reconnect();return 0;}
    switch(message){
    case kOpenSettings:Show();return 1;
    case kExitApplication:if(callbacks.exit)callbacks.exit();return 1;
    case kStatusChanged:if(callbacks.bridgeStatus)callbacks.bridgeStatus(wp,lp);return 0;
    case kRefresh:if(browser)browser->Send();return 0;
    case kCommand:if(browser&&wp==browser->generation)browser->Execute();return 0;
    case kFailure:if(browser&&wp==browser->generation){auto error=browser->failure;ShowWindow(hwnd,SW_HIDE);browser->Close();MessageBox(hwnd,error.c_str(),L"Window Scatter",MB_OK|MB_ICONERROR);}return 0;
    case WM_TIMER:if(wp==kLoadTimer&&browser&&browser->opening)browser->Fail(HRESULT_FROM_WIN32(ERROR_TIMEOUT));return 0;
    case WM_CLOSE:ShowWindow(hwnd,SW_HIDE);if(browser)browser->Close();return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);RECT rect{};GetClientRect(hwnd,&rect);HBRUSH brush=CreateSolidBrush(browser?browser->background:RGB(245,245,247));FillRect(dc,&rect,brush);DeleteObject(brush);EndPaint(hwnd,&paint);return 0;}
    case WM_SIZE:if(browser&&wp!=SIZE_MINIMIZED)browser->Bounds();return 0;
    case WM_MOVE:if(browser&&browser->controller)browser->controller->NotifyParentWindowPositionChanged();break;
    case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);if(browser)browser->Bounds();return 0;}
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:if(browser){browser->Theme();browser->Send();}return 0;
    case WM_SETFOCUS:if(browser&&browser->controller)browser->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);return 0;
    }
    return DefWindowProc(hwnd,message,wp,lp);
}
LRESULT CALLBACK SettingsWindow::Procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    auto*self=reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtr(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    return self?self->Message(message,wp,lp):DefWindowProc(hwnd,message,wp,lp);
}
}
