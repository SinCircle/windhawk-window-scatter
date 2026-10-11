#pragma once
#include "protocol.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace native {
struct Handle {
    HANDLE value=nullptr;
    Handle()=default;explicit Handle(HANDLE h):value(h){}
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;Handle&operator=(const Handle&)=delete;
    Handle(Handle&&other)noexcept:value(other.value){other.value=nullptr;}
    explicit operator bool()const{return value&&value!=INVALID_HANDLE_VALUE;}
};
std::wstring ErrorText(DWORD code);
std::wstring ExecutablePath();
std::filesystem::path DataDirectory();
void Log(const std::wstring&text);
bool ReadResource(HINSTANCE instance,int id,std::vector<BYTE>&bytes);
bool ExtractBridge(HINSTANCE instance,std::filesystem::path&path,std::wstring&error);
bool ResolveSymbols(const std::filesystem::path&directory,SymbolTable&table,HANDLE stop,
                    const std::function<void(const std::wstring&)>&progress,std::wstring&error,bool allowNetwork=true);
bool AttachBridge(const std::filesystem::path&dll,const BridgeConfig&config,DWORD shellProcess,
                  HANDLE stop,std::wstring&error);
std::wstring SymbolIdentityText(const ModuleIdentity&identity);

struct Preferences {
    bool enabled=true;
    int duration=666,radius=12,memory=0;
};
Preferences LoadPreferences(const wchar_t*key=kRegistry);
bool SavePreferences(const Preferences&prefs,const wchar_t*key=kRegistry);
bool StartupEnabled(std::wstring*registeredPath=nullptr,const wchar_t*key=kRunKey,const wchar_t*value=kRunValue);
bool SetStartup(bool enabled,const wchar_t*key=kRunKey,const wchar_t*value=kRunValue);
}
