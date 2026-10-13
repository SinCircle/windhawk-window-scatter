#include "backend.h"
#include <shlobj.h>
#include <dbghelp.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <cstring>

namespace native {
std::wstring ErrorText(DWORD code){
    wchar_t*message=nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr,code,0,reinterpret_cast<wchar_t*>(&message),0,nullptr);
    std::wstring result=message?message:L"Windows 错误 "+std::to_wstring(code);if(message)LocalFree(message);
    while(!result.empty()&&(result.back()==L'\r'||result.back()==L'\n'||result.back()==L' '))result.pop_back();
    return result;
}
std::wstring ExecutablePath(){
    std::wstring path(32768,L'\0');DWORD count=GetModuleFileName(nullptr,path.data(),DWORD(path.size()));
    if(!count||count==path.size())return {};path.resize(count);return path;
}
std::filesystem::path DataDirectory(){
    PWSTR path=nullptr;std::filesystem::path result;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&path))){result=std::filesystem::path(path)/L"WindowScatter";CoTaskMemFree(path);}
    if(!result.empty()){std::error_code error;std::filesystem::create_directories(result,error);}
    return result;
}
void Log(const std::wstring&text){
    static std::mutex mutex;std::lock_guard lock(mutex);
    auto directory=DataDirectory();if(directory.empty())return;
    auto file=directory/L"diagnostics.log";std::error_code error;
    if(std::filesystem::file_size(file,error)>1024*1024&&!error)std::filesystem::remove(file,error);
    FILE*out=nullptr;_wfopen_s(&out,file.c_str(),L"a, ccs=UTF-8");if(!out)return;
    SYSTEMTIME time{};GetLocalTime(&time);
    fwprintf(out,L"%04u-%02u-%02u %02u:%02u:%02u %ls\n",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,text.c_str());fclose(out);
}
bool ReadResource(HINSTANCE instance,int id,std::vector<BYTE>&bytes){
    HRSRC resource=FindResource(instance,MAKEINTRESOURCE(id),RT_RCDATA);if(!resource)return false;
    HGLOBAL loaded=LoadResource(instance,resource);DWORD size=SizeofResource(instance,resource);
    const BYTE*data=static_cast<const BYTE*>(LockResource(loaded));if(!data||!size)return false;
    bytes.assign(data,data+size);return true;
}
static std::wstring Hash(const std::vector<BYTE>&data){
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    BYTE digest[32]{};NTSTATUS result=BCryptHash(algorithm,nullptr,0,const_cast<BYTE*>(data.data()),ULONG(data.size()),digest,sizeof(digest));
    BCryptCloseAlgorithmProvider(algorithm,0);if(result<0)return {};
    std::wostringstream out;out<<std::hex<<std::setfill(L'0');for(BYTE b:digest)out<<std::setw(2)<<unsigned(b);return out.str();
}
static bool ResolveBridgePath(std::filesystem::path&path,std::wstring&error){
    // A packaged launcher can virtualize LocalAppData. Explorer must receive
    // the actual on-disk path, rather than the launcher's redirected alias.
    Handle file(CreateFile(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr));
    if(!file){error=L"无法打开内置系统组件："+ErrorText(GetLastError());return false;}
    std::wstring physical(32768,L'\0');
    DWORD length=GetFinalPathNameByHandle(file.value,physical.data(),DWORD(physical.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!length||length>=physical.size()){error=L"无法解析内置系统组件的位置。";return false;}
    physical.resize(length);
    if(physical.rfind(L"\\\\?\\UNC\\",0)==0)physical=L"\\\\"+physical.substr(8);
    else if(physical.rfind(L"\\\\?\\",0)==0)physical.erase(0,4);
    path=std::move(physical);return true;
}
bool ExtractBridge(HINSTANCE instance,std::filesystem::path&path,std::wstring&error){
    std::vector<BYTE>payload;if(!ReadResource(instance,201,payload)){error=L"内置系统组件缺失。";return false;}
    std::wstring hash=Hash(payload);if(hash.empty()){error=L"无法校验内置系统组件。";return false;}
    auto root=DataDirectory();if(root.empty()){error=L"无法读取本地应用数据目录。";return false;}
    auto directory=root/L"Runtime"/kVersion;std::error_code ec;std::filesystem::create_directories(directory,ec);
    if(ec){error=L"无法创建运行目录："+std::to_wstring(ec.value());return false;}
    path=directory/(L"bridge-"+hash.substr(0,20)+L".dll");
    std::ifstream current(path,std::ios::binary);
    if(current){std::vector<BYTE>existing{std::istreambuf_iterator<char>(current),{}};if(existing==payload){current.close();return ResolveBridgePath(path,error);}}
    current.close();auto temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(payload.data()),payload.size());if(!output){error=L"无法写入内置系统组件。";return false;}}
    if(!MoveFileEx(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=L"无法部署内置系统组件："+ErrorText(GetLastError());DeleteFile(temporary.c_str());return false;}
    return ResolveBridgePath(path,error);
}
std::wstring SymbolIdentityText(const ModuleIdentity&identity){
    const auto&g=identity.guid;wchar_t text[80];
    swprintf_s(text,L"%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%lX",g.Data1,g.Data2,g.Data3,
               g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7],identity.age);
    return text;
}
static bool Cancelled(HANDLE stop){return stop&&WaitForSingleObject(stop,0)==WAIT_OBJECT_0;}
static bool ValidSymbols(HMODULE module,const ModuleIdentity&id,const SymbolTable&table){
    if(table.format!=kProtocol||!(id==table.module))return false;
    return std::all_of(table.rva.begin(),table.rva.end(),[&](DWORD rva){return rva&&ExecutableAddress(module,rva);});
}
static bool BundledSymbols(const ModuleIdentity&id,SymbolTable&table){
    // Exact PDB identity, not a Windows build-number guess. The RVAs were checked
    // against Microsoft's matching public PDB and the native gesture mapping.
    if(SymbolIdentityText(id)!=L"09A465FCB1668CB766F6031877BFD6831")return false;
    table.module=id;table.rva={0x4CD5E0,0x170E54,0x2C5CE4,0x170850,0x170C50,0x170770,0x4D0710};return true;
}
struct InternetHandle{HINTERNET h=nullptr;~InternetHandle(){if(h)WinHttpCloseHandle(h);}};
static bool DownloadPdb(const std::wstring&id,const std::filesystem::path&path,HANDLE stop,std::wstring&error){
    InternetHandle session{WinHttpOpen(L"WindowScatter/0.5.4",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h){error=ErrorText(GetLastError());return false;}
    WinHttpSetTimeouts(session.h,5000,5000,5000,5000);
    InternetHandle connection{WinHttpConnect(session.h,L"msdl.microsoft.com",INTERNET_DEFAULT_HTTPS_PORT,0)};
    std::wstring route=L"/download/symbols/twinui.pcshell.pdb/"+id+L"/twinui.pcshell.pdb";
    InternetHandle request{connection.h?WinHttpOpenRequest(connection.h,L"GET",route.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
    if(!request.h||Cancelled(stop)||!WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr)){error=ErrorText(GetLastError());return false;}
    DWORD status=0,length=sizeof(status);WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX);
    if(status!=200){error=L"微软符号服务器返回 HTTP "+std::to_wstring(status);return false;}
    auto temporary=path;temporary+=L".download";bool success=true;UINT64 total=0;
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output){error=L"无法写入符号缓存。";return false;}
        BYTE buffer[65536];DWORD count=0;
        for(;;){
            if(Cancelled(stop)){error=L"已取消。";success=false;break;}
            if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&count)){error=ErrorText(GetLastError());success=false;break;}
            if(!count)break;total+=count;
            if(total>512ull*1024*1024){error=L"符号文件超过大小限制。";success=false;break;}
            output.write(reinterpret_cast<const char*>(buffer),count);if(!output){error=L"符号缓存写入失败。";success=false;break;}
        }
    }
    if(!success||total<4096){DeleteFile(temporary.c_str());if(error.empty())error=L"符号文件不完整。";return false;}
    if(!MoveFileEx(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=ErrorText(GetLastError());DeleteFile(temporary.c_str());return false;}
    return true;
}
bool ResolveSymbols(const std::filesystem::path&directory,SymbolTable&table,HANDLE stop,
                    const std::function<void(const std::wstring&)>&progress,std::wstring&error,bool allowNetwork){
    wchar_t system[MAX_PATH];if(!GetSystemDirectory(system,ARRAYSIZE(system))){error=ErrorText(GetLastError());return false;}
    std::filesystem::path library=std::filesystem::path(system)/L"twinui.pcshell.dll";
    HMODULE module=LoadLibraryEx(library.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);ModuleIdentity identity;
    if(!ReadModuleIdentity(module,identity)){if(module)FreeLibrary(module);error=L"此系统的任务视图接口尚不受支持。";return false;}
    struct ReleaseModule{HMODULE h;~ReleaseModule(){FreeLibrary(h);}}release{module};
    auto cache=directory/SymbolIdentityText(identity);std::error_code ec;std::filesystem::create_directories(cache,ec);
    if(ec){error=L"无法创建符号缓存目录。";return false;}
    auto offsets=cache/L"addresses.bin";
    {std::ifstream input(offsets,std::ios::binary);if(input.read(reinterpret_cast<char*>(&table),sizeof(table))&&ValidSymbols(module,identity,table))return true;}
    table=SymbolTable{};
    if(BundledSymbols(identity,table)&&ValidSymbols(module,identity,table)){
        std::ofstream output(offsets,std::ios::binary);output.write(reinterpret_cast<const char*>(&table),sizeof(table));return true;
    }
    auto pdb=cache/L"twinui.pcshell.pdb";
    if(!std::filesystem::exists(pdb)){
        if(!allowNetwork){error=L"此系统需要联网完成首次适配。";return false;}
        progress(L"正在完成首次系统适配…");
        if(!DownloadPdb(SymbolIdentityText(identity),pdb,stop,error))return false;
    }
    if(Cancelled(stop)){error=L"已取消。";return false;}
    progress(L"正在连接系统手势…");
    // DbgHelp is called only by this worker, with a private, unique session key.
    int sessionToken=0;HANDLE session=reinterpret_cast<HANDLE>(&sessionToken);
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_EXACT_SYMBOLS);
    if(!SymInitializeW(session,cache.c_str(),FALSE)){error=L"无法读取系统符号："+ErrorText(GetLastError());return false;}
    struct Cleanup{HANDLE h;~Cleanup(){SymCleanup(h);}}cleanup{session};
    DWORD64 base=SymLoadModuleExW(session,nullptr,library.c_str(),nullptr,0,0,nullptr,0);
    if(!base){error=L"无法匹配系统符号："+ErrorText(GetLastError());return false;}
    table=SymbolTable{};table.module=identity;
    struct Search{DWORD64 base;SymbolTable*table;}search{base,&table};
    BOOL enumerated=SymEnumSymbols(session,base,nullptr,[](PSYMBOL_INFO symbol,ULONG,void*context)->BOOL{
        auto&search=*static_cast<Search*>(context);
        for(size_t i=0;i<kSymbolCount;++i)if(!strcmp(symbol->Name,kSymbolNames[i])&&symbol->Address>=search.base&&symbol->Address-search.base<=MAXDWORD)search.table->rva[i]=DWORD(symbol->Address-search.base);
        return TRUE;
    },&search);
    if(!enumerated||!ValidSymbols(module,identity,table)){error=L"当前 Windows 版本未提供所需的任务视图接口。";return false;}
    {std::ofstream output(offsets,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(&table),sizeof(table));}
    return true;
}
static BYTE* RemoteModule(DWORD process,const std::wstring&path){
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,process));if(!snapshot)return nullptr;
    MODULEENTRY32 entry{sizeof(entry)};
    if(Module32First(snapshot.value,&entry))do{if(!_wcsicmp(entry.szExePath,path.c_str())||!_wcsicmp(entry.szModule,path.c_str()))return entry.modBaseAddr;}while(Module32Next(snapshot.value,&entry));
    return nullptr;
}
static bool RemoteCall(HANDLE process,void*function,const void*data,size_t size,HANDLE stop,DWORD&result,std::wstring&error){
    void*remote=VirtualAllocEx(process,nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote){error=ErrorText(GetLastError());return false;}
    if(!WriteProcessMemory(process,remote,data,size,nullptr)){error=ErrorText(GetLastError());VirtualFreeEx(process,remote,0,MEM_RELEASE);return false;}
    Handle thread(CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(function),remote,0,nullptr));
    if(!thread){error=ErrorText(GetLastError());VirtualFreeEx(process,remote,0,MEM_RELEASE);return false;}
    HANDLE waits[]{thread.value,stop};DWORD count=stop?2:1;
    DWORD wait=WaitForMultipleObjects(count,waits,FALSE,10000);
    if(wait!=WAIT_OBJECT_0){
        // The call may still access its argument. Do not free it or terminate an
        // Explorer thread. A late completion is harmless and the next retry finds it.
        error=wait==WAIT_OBJECT_0+1?L"已取消。":L"系统入口响应超时。";return false;
    }
    bool ok=GetExitCodeThread(thread.value,&result)!=FALSE;
    if(!ok)error=ErrorText(GetLastError());VirtualFreeEx(process,remote,0,MEM_RELEASE);return ok;
}
bool AttachBridge(const std::filesystem::path&dll,const BridgeConfig&config,DWORD shellProcess,HANDLE stop,std::wstring&error){
    Handle process(OpenProcess(PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,shellProcess));
    if(!process){error=L"无法连接 Explorer："+ErrorText(GetLastError());return false;}
    USHORT processMachine=0,nativeMachine=0;
    if(!IsWow64Process2(process.value,&processMachine,&nativeMachine)||nativeMachine!=IMAGE_FILE_MACHINE_AMD64||processMachine!=IMAGE_FILE_MACHINE_UNKNOWN){error=L"独立版目前支持 Windows 11 x64。";return false;}
    BYTE*remoteBase=RemoteModule(shellProcess,dll.wstring());
    if(!remoteBase){
        auto loader=reinterpret_cast<void*>(GetProcAddress(GetModuleHandle(L"kernel32.dll"),"LoadLibraryW"));HMODULE owner=nullptr;
        if(!loader||!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(loader),&owner)){error=L"无法定位系统加载器。";return false;}
        wchar_t ownerPath[MAX_PATH]{};GetModuleFileName(owner,ownerPath,ARRAYSIZE(ownerPath));
        BYTE*remoteOwner=RemoteModule(shellProcess,ownerPath);if(!remoteOwner){error=L"无法匹配系统加载器。";return false;}
        auto remoteLoader=remoteOwner+(static_cast<BYTE*>(loader)-reinterpret_cast<BYTE*>(owner));
        DWORD ignored=0;std::wstring path=dll.wstring();
        if(!RemoteCall(process.value,remoteLoader,path.c_str(),(path.size()+1)*sizeof(wchar_t),stop,ignored,error))return false;
        remoteBase=RemoteModule(shellProcess,path);if(!remoteBase){error=L"系统未能加载内置组件。";return false;}
    }
    HMODULE local=LoadLibraryEx(dll.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!local){error=ErrorText(GetLastError());return false;}
    auto entry=reinterpret_cast<BYTE*>(GetProcAddress(local,"WindowScatterStart"));
    if(!entry){FreeLibrary(local);error=L"内置组件入口缺失。";return false;}
    BYTE*remoteEntry=remoteBase+(entry-reinterpret_cast<BYTE*>(local));FreeLibrary(local);
    DWORD result=0;
    if(!RemoteCall(process.value,remoteEntry,&config,sizeof(config),stop,result,error))return false;
    if(result!=ERROR_SUCCESS){error=L"系统组件启动失败："+ErrorText(result);return false;}
    return true;
}
static DWORD ReadDword(HKEY key,const wchar_t*name,DWORD fallback){DWORD value=0,size=sizeof(value);return RegGetValue(key,nullptr,name,RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS?value:fallback;}
Preferences LoadPreferences(const wchar_t*path){
    Preferences prefs;HKEY key=nullptr;if(RegOpenKeyEx(HKEY_CURRENT_USER,path,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return prefs;
    prefs.enabled=ReadDword(key,L"Enabled",1)!=0;
    prefs.duration=std::clamp<int>(ReadDword(key,L"AnimationDuration",666),0,900);
    prefs.memory=std::clamp<int>(ReadDword(key,L"MemoryMode",0),0,2);RegCloseKey(key);return prefs;
}
bool SavePreferences(const Preferences&prefs,const wchar_t*path){
    HKEY key=nullptr;if(RegCreateKeyEx(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    auto write=[&](const wchar_t*name,DWORD value){return RegSetValueEx(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value))==ERROR_SUCCESS;};
    bool success=write(L"Enabled",prefs.enabled)&&write(L"AnimationDuration",std::clamp(prefs.duration,0,900))&&write(L"MemoryMode",std::clamp(prefs.memory,0,2));RegCloseKey(key);return success;
}
bool StartupEnabled(std::wstring*registeredPath,const wchar_t*key,const wchar_t*value){
    wchar_t command[32768]{};DWORD bytes=sizeof(command);
    if(RegGetValue(HKEY_CURRENT_USER,key,value,RRF_RT_REG_SZ,nullptr,command,&bytes)!=ERROR_SUCCESS)return false;
    if(registeredPath)*registeredPath=command;
    std::wstring expected=L"\""+ExecutablePath()+L"\" --startup";
    return expected==command;
}
bool SetStartup(bool enabled,const wchar_t*path,const wchar_t*value){
    HKEY key=nullptr;if(RegCreateKeyEx(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    LONG status;
    if(enabled){std::wstring command=L"\""+ExecutablePath()+L"\" --startup";status=RegSetValueEx(key,value,0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t)));}
    else {status=RegDeleteValue(key,value);if(status==ERROR_FILE_NOT_FOUND)status=ERROR_SUCCESS;}
    RegCloseKey(key);return status==ERROR_SUCCESS;
}
}
