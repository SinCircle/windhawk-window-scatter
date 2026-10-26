// Self-contained Windows application. The shared renderer is compiled directly
// into this EXE; no Windhawk library or service is used in this build.
#define SCATTER_STANDALONE
#define SCATTER_TEST
#include "../window-scatter.wh.cpp"
#include "settings-window.h"
#include <thread>
#include <fstream>
#include <shellapi.h>

namespace native {
class Application {
public:
    explicit Application(HINSTANCE module):module(module),stop(CreateEvent(nullptr,TRUE,FALSE,nullptr)),reconnect(CreateEvent(nullptr,FALSE,FALSE,nullptr)),bridgeReady(CreateEvent(nullptr,FALSE,FALSE,nullptr)){}
    int Run(bool showSettings){
        if(!stop||!reconnect||!bridgeReady)return 1;
        preferences=LoadPreferences();ApplyValues(preferences);scatter::g_instance=module;
        WindowCallbacks callbacks;
        callbacks.save=[&](const Preferences&values){if(!SavePreferences(values))return false;ApplyValues(values);return true;};
        callbacks.exit=[&]{SetEvent(stop.value);ShowWindow(window.Handle(),SW_HIDE);PostQuitMessage(0);};
        callbacks.reconnect=[&]{SetEvent(reconnect.value);};
        callbacks.bridgeStatus=[&](WPARAM code,LPARAM handle){
            if(code==1){endpoint=reinterpret_cast<HWND>(handle);bridgeFailure=0;window.SetStatus(1,L"已连接系统手势",L"Win+Tab 与三指手势已就绪。关闭此面板后，程序继续在后台运行。");}
            else {bridgeFailure=DWORD(code);window.SetStatus(3,L"系统入口暂不可用",code>=0x10000?ErrorText(DWORD(code)-0x10000):L"请稍后重试连接。");}
            SetProp(window.Handle(),L"WindowScatter.ConnectionState.v1",reinterpret_cast<HANDLE>(code==1?1:3));
            SetEvent(bridgeReady.value);
        };
        if(!window.Create(module,preferences,std::move(callbacks)))return 2;
        SetProp(window.Handle(),kOwnerProperty,reinterpret_cast<HANDLE>(ULONG_PTR(GetCurrentProcessId())));
        Handle ready(CreateEvent(nullptr,TRUE,TRUE,L"Local\\WindowScatter.Standalone.Ready.v1"));if(ready)SetEvent(ready.value);
        worker=std::thread([&]{Worker();});
        if(showSettings)window.Show();
        MSG message;
        while(GetMessage(&message,nullptr,0,0)>0){
            if(window.ProcessDialogMessage(message))continue;
            TranslateMessage(&message);DispatchMessage(&message);
        }
        SetEvent(stop.value);if(worker.joinable())worker.join();
        HWND bridge=endpoint.load();
        if(!bridge){HWND taskbar=FindWindow(L"Shell_TrayWnd",nullptr);bridge=reinterpret_cast<HWND>(GetProp(taskbar,kBridgeProperty));}
        if(bridge){DWORD_PTR owner=0;if(SendMessageTimeout(bridge,kBridgePing,0,0,SMTO_ABORTIFHUNG,500,&owner)&&owner==GetCurrentProcessId()){
            SendMessageTimeout(bridge,kBridgeStop,0,0,SMTO_ABORTIFHUNG,500,nullptr);
            for(int n=0;n<50&&IsWindow(bridge);++n)Sleep(20);
        }}
        if(rendererStarted)scatter::Stop();
        RemoveProp(window.Handle(),kOwnerProperty);window.Destroy();return 0;
    }
private:
    HINSTANCE module;
    Handle stop,reconnect,bridgeReady;
    SettingsWindow window;
    Preferences preferences;
    std::thread worker;
    std::atomic<HWND> endpoint{nullptr};
    std::atomic<DWORD> bridgeFailure{0};
    bool rendererStarted=false;
    void ApplyValues(const Preferences&values){
        preferences=values;scatter::g_winTab=values.enabled;scatter::g_duration=values.duration;scatter::g_memoryMode=values.memory;
        if(HWND controller=scatter::g_controller.load())PostMessage(controller,scatter::kSettings,0,0);
        if(HWND bridge=endpoint.load())PostMessage(bridge,kBridgeEnable,values.enabled,0);
    }
    bool Stopped(){return WaitForSingleObject(stop.value,0)==WAIT_OBJECT_0;}
    void Failure(const std::wstring&detail){Log(detail);window.SetStatus(3,L"系统入口暂不可用",detail);SetProp(window.Handle(),L"WindowScatter.ConnectionState.v1",reinterpret_cast<HANDLE>(3));}
    void Worker(){
        std::filesystem::path payload;SymbolTable table;bool symbolsReady=false;
        HANDLE shellHandle=nullptr;DWORD attachedShell=0;
        for(;;){
            if(Stopped())break;
            if(!rendererStarted){
                HWND other=FindWindow(scatter::kController,L"Window Scatter Controller");
                if(other){Failure(L"请停用 Windhawk 同名模组，或退出其他独立版程序。");}
                else {
                    if(scatter::Start()){
                        rendererStarted=true;SetProp(scatter::g_controller.load(),kOwnerProperty,reinterpret_cast<HANDLE>(ULONG_PTR(GetCurrentProcessId())));
                    }else {scatter::Stop();Failure(L"此系统暂时无法创建窗口总览。");}
                }
            }
            if(rendererStarted){
                HWND taskbar=FindWindow(L"Shell_TrayWnd",nullptr);DWORD shell=0;if(taskbar)GetWindowThreadProcessId(taskbar,&shell);
                if(!shell){window.SetStatus(0,L"等待资源管理器…",L"资源管理器恢复后会自动连接。");}
                else if(GetProp(taskbar,L"WindowScatter.NativeBridge.v4")){Failure(L"Windhawk 版仍在接管系统手势。请先停用同名模组，再重新连接。");}
                else {
                    HWND current=reinterpret_cast<HWND>(GetProp(taskbar,kBridgeProperty));DWORD_PTR owner=0;
                    if(current&&SendMessageTimeout(current,kBridgePing,0,0,SMTO_ABORTIFHUNG,500,&owner)&&owner==GetCurrentProcessId()){
                        endpoint=current;window.SetStatus(1,L"已连接系统手势",L"Win+Tab 与三指手势已就绪。关闭此面板后，程序继续在后台运行。");
                    }else{
                        window.SetStatus(0,L"正在连接系统…",L"首次适配某些 Windows 版本时需要联网，之后使用本地缓存。");
                        std::wstring error;
                        if(payload.empty()&&!ExtractBridge(module,payload,error))Failure(error);
                        else {
                            if(!symbolsReady)symbolsReady=ResolveSymbols(DataDirectory()/L"Symbols",table,stop.value,[&](const std::wstring&text){window.SetStatus(0,text,L"仅从微软获取匹配当前 Windows 版本的符号文件。");},error);
                            if(!symbolsReady){if(!Stopped())Failure(error);}
                            else if(!Stopped()){
                                BridgeConfig config;config.process=GetCurrentProcessId();config.enabled=scatter::g_winTab.load();
                                config.controller=reinterpret_cast<UINT64>(scatter::g_controller.load());config.manager=reinterpret_cast<UINT64>(window.Handle());config.symbols=table;
                                ResetEvent(bridgeReady.value);bridgeFailure=0;
                                if(!AttachBridge(payload,config,shell,stop.value,error)){if(!Stopped())Failure(error);}
                                else {
                                    HANDLE waits[]{stop.value,bridgeReady.value};DWORD wait=WaitForMultipleObjects(2,waits,FALSE,15000);
                                    if(wait==WAIT_TIMEOUT)Failure(L"连接系统入口超时，请重新连接。");
                                    else if(wait==WAIT_OBJECT_0+1&&bridgeFailure.load())Log(L"Bridge initialization failed: "+std::to_wstring(bridgeFailure.load()));
                                }
                            }
                        }
                    }
                    if(shell!=attachedShell){if(shellHandle)CloseHandle(shellHandle);shellHandle=OpenProcess(SYNCHRONIZE,FALSE,shell);attachedShell=shell;}
                }
            }
            HANDLE waits[]{stop.value,reconnect.value,shellHandle};DWORD count=shellHandle?3:2;
            DWORD wait=WaitForMultipleObjects(count,waits,FALSE,INFINITE);
            if(wait==WAIT_OBJECT_0||wait==WAIT_FAILED)break;
            if(wait==WAIT_OBJECT_0+2){CloseHandle(shellHandle);shellHandle=nullptr;attachedShell=0;endpoint=nullptr;window.SetStatus(0,L"等待资源管理器…",L"资源管理器恢复后会自动连接。");
                HANDLE resume[]{stop.value,reconnect.value};if(WaitForMultipleObjects(2,resume,FALSE,INFINITE)==WAIT_OBJECT_0)break;
            }
        }
        if(shellHandle)CloseHandle(shellHandle);
    }
};

int SelfTest(const std::wstring&report){
    std::ofstream output(std::filesystem::path(report),std::ios::out|std::ios::trunc);
    auto check=[&](bool ok,const char*text){if(output)output<<(ok?"PASS ":"FAIL ")<<text<<"\n";return ok;};
    std::wstring key=std::wstring(kRegistry)+L"\\SelfTest-"+std::to_wstring(GetCurrentProcessId());
    auto defaults=LoadPreferences(key.c_str());
    bool success=check(defaults.duration==666&&defaults.memory==0&&defaults.enabled,"fresh defaults use 666 ms, automatic memory and system gestures");
    Preferences prefs;prefs.enabled=false;prefs.duration=840;prefs.memory=2;
    bool saved=SavePreferences(prefs,key.c_str());auto loaded=LoadPreferences(key.c_str());
    success=check(saved&&loaded.enabled==prefs.enabled&&loaded.duration==prefs.duration&&loaded.memory==prefs.memory,"preferences persist and reload")&&success;
    std::wstring run=key+L"\\Run";
    success=check(SetStartup(true,run.c_str(),L"Entry")&&StartupEnabled(nullptr,run.c_str(),L"Entry"),"autostart command uses quoted EXE path and --startup")&&success;
    success=check(SetStartup(false,run.c_str(),L"Entry")&&!StartupEnabled(nullptr,run.c_str(),L"Entry"),"autostart can be disabled without changing other values")&&success;
    const std::filesystem::path downloads=L"Z:\\ScatterTest\\Downloads";
    const std::vector<std::filesystem::path>temporary={L"Z:\\ScatterTest\\Temp",L"Z:\\Windows\\Temp"};
    const std::pair<const wchar_t*,StartupLocation>locations[]={
        {L"z:\\scattertest\\DOWNLOADS\\WindowScatter.exe",StartupLocation::Downloads},
        {L"Z:\\ScatterTest\\Downloads\\Unpacked\\WindowScatter.exe",StartupLocation::Downloads},
        {L"Z:\\ScatterTest\\Temp\\archive\\WindowScatter.exe",StartupLocation::Temporary},
        {L"Z:\\Windows\\Temp\\WindowScatter.exe",StartupLocation::Temporary},
        {L"Z:\\ScatterTest\\Downloads-old\\WindowScatter.exe",StartupLocation::Permanent},
        {L"Z:\\ScatterTest\\Templates\\WindowScatter.exe",StartupLocation::Permanent},
        {L"Z:\\ScatterTest\\Downloads\\..\\Apps\\WindowScatter.exe",StartupLocation::Permanent},
        {L"Z:\\Apps\\WindowScatter.exe",StartupLocation::Permanent},
    };
    bool locationsCorrect=true;for(const auto&[path,expected]:locations)
        locationsCorrect=locationsCorrect&&ClassifyStartupLocation(path,downloads,temporary)==expected;
    locationsCorrect=locationsCorrect&&ClassifyStartupLocation(L"Z:\\Apps\\WindowScatter.exe",L"",{})==StartupLocation::Permanent;
    success=check(locationsCorrect,"startup location warnings respect folder boundaries, case, nesting and normalized paths")&&success;
    RegDeleteTree(HKEY_CURRENT_USER,key.c_str());
    std::filesystem::path dll;std::wstring error;
    success=check(ExtractBridge(GetModuleHandle(nullptr),dll,error),"embedded bridge extraction and hash verification")&&success;
    SymbolTable symbols;
    bool resolved=ResolveSymbols(DataDirectory()/L"Symbols",symbols,nullptr,[](const std::wstring&){},error,false);
    success=check(resolved,"exact system symbol identity resolves offline")&&success;
    if(output&&resolved){std::wstring identity=SymbolIdentityText(symbols.module);output<<"SYMBOLS "<<std::string(identity.begin(),identity.end())<<"\n";}
    if(output&&!error.empty())output<<"ERROR "<<std::string(error.begin(),error.end())<<"\n";
    return success?0:1;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    bool settings=false,startup=false,exit=false,selfTest=false;std::wstring report;
    int count=0;LPWSTR*arguments=CommandLineToArgvW(GetCommandLine(),&count);
    for(int i=1;i<count;++i){
        if(!wcscmp(arguments[i],L"--settings"))settings=true;
        else if(!wcscmp(arguments[i],L"--startup"))startup=true;
        else if(!wcscmp(arguments[i],L"--exit"))exit=true;
        else if(!wcscmp(arguments[i],L"--self-test"))selfTest=true;
        else if(!wcscmp(arguments[i],L"--report")&&i+1<count)report=arguments[++i];
    }
    LocalFree(arguments);
    if(selfTest)return native::SelfTest(report.empty()?(native::DataDirectory()/L"self-test.txt").wstring():report);
    native::Handle mutex(CreateMutex(nullptr,FALSE,native::kInstanceMutex));if(!mutex)return 1;
    bool existing=GetLastError()==ERROR_ALREADY_EXISTS;
    native::Handle ready(CreateEvent(nullptr,TRUE,FALSE,L"Local\\WindowScatter.Standalone.Ready.v1"));
    if(!existing&&ready)ResetEvent(ready.value);
    if(existing||exit){
        if(startup&&!settings&&!exit)return 0;
        if(existing&&ready)WaitForSingleObject(ready.value,5000);
        HWND manager=FindWindow(native::kManagerClass,nullptr);
        if(manager){DWORD process=0;GetWindowThreadProcessId(manager,&process);AllowSetForegroundWindow(process);SendMessageTimeout(manager,exit?native::kExitApplication:native::kOpenSettings,0,0,SMTO_ABORTIFHUNG,3000,nullptr);}
        return manager||exit?0:2;
    }
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok){if(SUCCEEDED(com))CoUninitialize();return 3;}
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
    native::Application app(instance);int result=app.Run(settings||!startup);
    Gdiplus::GdiplusShutdown(token);if(SUCCEEDED(com))CoUninitialize();return result;
}
