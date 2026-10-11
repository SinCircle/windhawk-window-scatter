// The only code loaded into Explorer. Rendering, settings, network access and
// symbol resolution stay in WindowScatter.exe. MinHook is statically linked.
#define NOMINMAX
#define SCATTER_STANDALONE
#include "protocol.h"
#include "vendor/minhook/include/MinHook.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <utility>
#include <memory>

namespace scatter {
// Generated from the mod's IPC constants, so both packages use the same protocol.
#include "renderer-protocol.inc"
static std::atomic<bool> g_winTab{true};
static std::atomic<HWND> g_nativeTarget{nullptr};
}
#include "shell-hooks.inc"

namespace {
HMODULE moduleHandle;
std::atomic<bool> running{false};
HWND endpoint=nullptr,taskbar=nullptr;
native::BridgeConfig config;
bool hooksCreated=false,hooksEnabled=false;
HANDLE ownerProcess=nullptr;
DWORD lastErrorCode=0;

bool ValidOwner(const native::BridgeConfig&c){
    if(c.protocol!=native::kProtocol||c.size!=sizeof(c)||!c.process||!c.controller||!c.manager)return false;
    DWORD owner=0;GetWindowThreadProcessId(reinterpret_cast<HWND>(c.controller),&owner);
    if(owner!=c.process)return false;
    GetWindowThreadProcessId(reinterpret_cast<HWND>(c.manager),&owner);
    return owner==c.process;
}
void Notify(DWORD code){
    PostMessage(reinterpret_cast<HWND>(config.manager),native::kStatusChanged,code,reinterpret_cast<LPARAM>(endpoint));
}
bool Connect(){
    taskbar=FindWindow(L"Shell_TrayWnd",nullptr);DWORD shell=0;
    if(!taskbar||!GetWindowThreadProcessId(taskbar,&shell)||shell!=GetCurrentProcessId()){lastErrorCode=ERROR_INVALID_OWNER;return false;}
    // Do not place our detours on top of the Windhawk edition.
    if(GetProp(taskbar,L"WindowScatter.NativeBridge.v4")){lastErrorCode=ERROR_ALREADY_EXISTS;return false;}
    HMODULE twinui=GetModuleHandle(L"twinui.pcshell.dll");
    if(!twinui)twinui=LoadLibraryEx(L"twinui.pcshell.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    native::ModuleIdentity id;
    if(!native::ReadModuleIdentity(twinui,id)||!(id==config.symbols.module)||config.symbols.format!=native::kProtocol){lastErrorCode=ERROR_REVISION_MISMATCH;return false;}
    for(DWORD rva:config.symbols.rva)if(!native::ExecutableAddress(twinui,rva)){lastErrorCode=ERROR_INVALID_ADDRESS;return false;}
    auto address=[&](native::SymbolIndex i){return reinterpret_cast<BYTE*>(twinui)+config.symbols.rva[i];};
    using Determine=int(WINAPI*)(int,bool,int);
    auto determine=reinterpret_cast<Determine>(address(native::Determine));
    if(determine(1,false,0)!=1||determine(1,true,0)!=4){lastErrorCode=ERROR_NOT_SUPPORTED;return false;}
    using namespace scatter::shell;
    if(!hooksCreated){
        MH_STATUS init=MH_Initialize();
        if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED){lastErrorCode=0x2000+init;return false;}
        struct Hook{native::SymbolIndex symbol;void*replacement;void**original;};
        Hook hooks[]={
            {native::Toggle,reinterpret_cast<void*>(ToggleHook),reinterpret_cast<void**>(&originalToggle)},
            {native::Gesture,reinterpret_cast<void*>(GestureHook),reinterpret_cast<void**>(&originalGesture)},
            {native::Start,reinterpret_cast<void*>(StartHook),reinterpret_cast<void**>(&originalStart)},
            {native::Continue,reinterpret_cast<void*>(ContinueHook),reinterpret_cast<void**>(&originalContinue)},
            {native::Finish,reinterpret_cast<void*>(FinishHook),reinterpret_cast<void**>(&originalFinish)},
            {native::Cancel,reinterpret_cast<void*>(CancelHook),reinterpret_cast<void**>(&originalCancel)}
        };
        for(auto&hook:hooks){
            MH_STATUS status=MH_CreateHook(address(hook.symbol),hook.replacement,hook.original);
            if(status!=MH_OK&&!(status==MH_ERROR_ALREADY_CREATED&&*hook.original)){lastErrorCode=0x2000+status;return false;}
        }
        hooksCreated=true;
    }
    scatter::g_nativeTarget.store(reinterpret_cast<HWND>(config.controller));
    scatter::g_winTab.store(config.enabled!=0);stopping=false;
    MH_STATUS enabled=MH_EnableHook(MH_ALL_HOOKS);
    if(enabled!=MH_OK){lastErrorCode=0x2000+enabled;return false;}
    hooksEnabled=true;
    SetProp(taskbar,native::kBridgeProperty,endpoint);
    return true;
}
void Disconnect(){
    scatter::shell::stopping=true;scatter::g_winTab=false;
    if(hooksEnabled){MH_DisableHook(MH_ALL_HOOKS);hooksEnabled=false;}
    // Leave disabled trampolines and this small DLL mapped until Explorer exits.
    // An in-flight callback can safely finish; no code is unmapped under it.
    {std::lock_guard lock(scatter::shell::swipeMutex);for(auto&[self,swipe]:scatter::shell::swipes)scatter::shell::EndSwipe(swipe,false);scatter::shell::swipes.clear();}
    scatter::g_nativeTarget=nullptr;
    if(taskbar&&GetProp(taskbar,native::kBridgeProperty)==endpoint)RemoveProp(taskbar,native::kBridgeProperty);
}
LRESULT CALLBACK BridgeProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    if(message==native::kBridgePing)return config.process;
    if(message==native::kBridgeEnable){scatter::g_winTab=wp!=0;return native::kProtocol;}
    if(message==native::kBridgeStop){PostQuitMessage(0);return native::kProtocol;}
    return DefWindowProc(hwnd,message,wp,lp);
}
DWORD WINAPI Run(void*parameter){
    std::unique_ptr<native::BridgeConfig> owned(static_cast<native::BridgeConfig*>(parameter));config=*owned;
    ownerProcess=OpenProcess(SYNCHRONIZE,FALSE,config.process);
    if(!ownerProcess){lastErrorCode=GetLastError();Notify(lastErrorCode+0x10000);running=false;return 0;}
    WNDCLASSEX wc{sizeof(wc)};wc.hInstance=moduleHandle;wc.lpfnWndProc=BridgeProc;wc.lpszClassName=native::kBridgeClass;
    RegisterClassEx(&wc);
    endpoint=CreateWindowEx(WS_EX_TOOLWINDOW,native::kBridgeClass,L"Window Scatter Native Host",0,0,0,0,0,nullptr,nullptr,moduleHandle,nullptr);
    if(endpoint&&Connect()){
        Notify(1);
        bool quit=false;
        while(!quit){
            DWORD wait=MsgWaitForMultipleObjectsEx(1,&ownerProcess,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(wait==WAIT_OBJECT_0||wait==WAIT_FAILED)break;
            MSG message;while(PeekMessage(&message,nullptr,0,0,PM_REMOVE)){
                if(message.message==WM_QUIT){quit=true;break;}TranslateMessage(&message);DispatchMessage(&message);
            }
        }
    }else Notify((lastErrorCode?lastErrorCode:GetLastError())+0x10000);
    Disconnect();if(endpoint)DestroyWindow(endpoint);endpoint=nullptr;
    UnregisterClass(native::kBridgeClass,moduleHandle);CloseHandle(ownerProcess);ownerProcess=nullptr;
    running=false;return 0;
}
}
extern "C" __declspec(dllexport) DWORD WINAPI WindowScatterStart(void*parameter){
    if(!parameter)return ERROR_INVALID_PARAMETER;
    auto*requested=static_cast<const native::BridgeConfig*>(parameter);
    if(!ValidOwner(*requested))return ERROR_INVALID_PARAMETER;
    bool expected=false;if(!running.compare_exchange_strong(expected,true))return ERROR_BUSY;
    auto*copy=new(std::nothrow) native::BridgeConfig(*requested);
    if(!copy){running=false;return ERROR_NOT_ENOUGH_MEMORY;}
    HMODULE pinned=nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&WindowScatterStart),&pinned);
    HANDLE thread=CreateThread(nullptr,0,Run,copy,0,nullptr);
    if(!thread){DWORD error=GetLastError();delete copy;running=false;return error;}
    CloseHandle(thread);return ERROR_SUCCESS;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH){moduleHandle=instance;DisableThreadLibraryCalls(instance);}return TRUE;
}
