#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstdint>

namespace native {
inline constexpr wchar_t kVersion[]=L"0.5.5";
inline constexpr wchar_t kManagerClass[]=L"WindowScatter.Settings.v1";
inline constexpr wchar_t kInstanceMutex[]=L"Local\\WindowScatter.Standalone.v1";
inline constexpr wchar_t kBridgeClass[]=L"WindowScatter.NativeHost.v1";
inline constexpr wchar_t kBridgeProperty[]=L"WindowScatter.StandaloneBridge.v1";
inline constexpr wchar_t kOwnerProperty[]=L"WindowScatter.StandaloneOwner.v1";
inline constexpr wchar_t kRegistry[]=L"Software\\SinCircle\\WindowScatter";
inline constexpr wchar_t kRunKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t kRunValue[]=L"WindowScatter";
inline constexpr UINT kOpenSettings=WM_APP+101,kStatusChanged=WM_APP+102,kExitApplication=WM_APP+105;
inline constexpr UINT kBridgeStop=WM_APP+111,kBridgeEnable=WM_APP+112,kBridgePing=WM_APP+113;
inline constexpr DWORD kProtocol=0x57530101;
inline constexpr size_t kSymbolCount=7;
enum SymbolIndex { Toggle,Gesture,Determine,Start,Continue,Finish,Cancel };
inline constexpr const char* kSymbolNames[kSymbolCount]={
    "?ToggleAllUpView@CAllUpViewService@@UEAAJPEAUHMONITOR__@@W4ALL_UP_VIEW_FLAGS@@@Z",
    "?ActivateGestureAction@MultitaskingViewGestureHandler@@AEAAJW4GestureAction@MultitaskingViewGestureHandlerHelpers@@PEBQEAUHWND__@@@Z",
    "?DetermineTouchpadGestureAction@MultitaskingViewGestureHandlerHelpers@@YA?AW4GestureAction@1@W4GestureDirection@1@_NW4MULTITASKING_VIEW_TYPES@@@Z",
    "?StartSwipe@MultitaskingViewGestureHandler@@UEAAJHHIUTOUCHPAD_GESTURE_CONTEXT@@@Z",
    "?ContinueSwipe@MultitaskingViewGestureHandler@@UEAAJHHHHI@Z",
    "?FinishSwipe@MultitaskingViewGestureHandler@@UEAAJHHHHI@Z",
    "?CancelSwipe@MultitaskingViewGestureHandler@@UEAAJXZ"
};
struct ModuleIdentity {
    GUID guid{};
    DWORD age=0,timestamp=0,imageSize=0;
    bool operator==(const ModuleIdentity&other)const{
        return InlineIsEqualGUID(guid,other.guid)&&age==other.age&&timestamp==other.timestamp&&imageSize==other.imageSize;
    }
};
struct SymbolTable {
    DWORD format=kProtocol;
    ModuleIdentity module;
    std::array<DWORD,kSymbolCount> rva{};
};
struct BridgeConfig {
    DWORD protocol=kProtocol,size=sizeof(BridgeConfig),process=0,enabled=1;
    UINT64 controller=0,manager=0;
    SymbolTable symbols;
};
inline bool ReadModuleIdentity(HMODULE module,ModuleIdentity&out){
    if(!module)return false;
    const BYTE*base=reinterpret_cast<const BYTE*>(module);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0x100000)return false;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;
    DWORD size=nt->OptionalHeader.SizeOfImage;
    const auto&dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if(!dir.VirtualAddress||dir.VirtualAddress>=size||dir.Size>size-dir.VirtualAddress)return false;
    auto entries=reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base+dir.VirtualAddress);
    struct CodeView{DWORD signature;GUID guid;DWORD age;};
    for(DWORD i=0;i<dir.Size/sizeof(*entries);++i){
        const auto&e=entries[i];
        if(e.Type!=IMAGE_DEBUG_TYPE_CODEVIEW||e.SizeOfData<sizeof(CodeView)||e.AddressOfRawData>=size||e.SizeOfData>size-e.AddressOfRawData)continue;
        auto cv=reinterpret_cast<const CodeView*>(base+e.AddressOfRawData);
        if(cv->signature!=0x53445352)continue;
        out.guid=cv->guid;out.age=cv->age;out.timestamp=nt->FileHeader.TimeDateStamp;out.imageSize=size;return true;
    }
    return false;
}
inline bool ExecutableAddress(HMODULE module,DWORD rva){
    auto base=reinterpret_cast<const BYTE*>(module);
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto*section=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
        DWORD begin=section[i].VirtualAddress,length=section[i].Misc.VirtualSize;
        if((section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)&&rva>=begin&&rva-begin<length)return true;
    }
    return false;
}
}
