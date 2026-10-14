#define SCATTER_STANDALONE
#define SCATTER_TEST
#ifndef SCATTER_SOURCE
#define SCATTER_SOURCE "../window-scatter.wh.cpp"
#endif
#include SCATTER_SOURCE
#include <psapi.h>
using namespace scatter;
struct Sample {ULONGLONG cpu,cycles;IO_COUNTERS io;PROCESS_MEMORY_COUNTERS_EX memory;};
static ULONGLONG FileTime(FILETIME f){return (ULONGLONG(f.dwHighDateTime)<<32)|f.dwLowDateTime;}
static Sample Read(){Sample s{};FILETIME c,e,k,u;GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);s.cpu=FileTime(k)+FileTime(u);QueryProcessCycleTime(GetCurrentProcess(),&s.cycles);GetProcessIoCounters(GetCurrentProcess(),&s.io);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&s.memory),sizeof(s.memory));return s;}
static DWORD WINAPI SampleIdle(void*){
    if(WaitForSingleObject(g_stopEvent,65000)!=WAIT_TIMEOUT)return 0;
    auto a=Read();ULONGLONG begin=GetTickCount64();
    if(WaitForSingleObject(g_stopEvent,20000)!=WAIT_TIMEOUT)return 0;
    auto b=Read();printf("idle_ms=%llu cpu_ms=%.4f cycles=%llu io_operations=%llu io_bytes=%llu private_kb=%zu working_kb=%zu\n",GetTickCount64()-begin,(b.cpu-a.cpu)/10000.,b.cycles-a.cycles,
        b.io.ReadOperationCount+b.io.WriteOperationCount+b.io.OtherOperationCount-a.io.ReadOperationCount-a.io.WriteOperationCount-a.io.OtherOperationCount,
        b.io.ReadTransferCount+b.io.WriteTransferCount+b.io.OtherTransferCount-a.io.ReadTransferCount-a.io.WriteTransferCount-a.io.OtherTransferCount,
        size_t(b.memory.PrivateUsage/1024),size_t(b.memory.WorkingSetSize/1024));fflush(stdout);SetEvent(g_stopEvent);return 0;
}
int main(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitialize(nullptr);g_instance=GetModuleHandle(nullptr);g_probe=true;
    g_stopEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);App app;App::instance=&app;
    if(!app.Initialize())return 1;app.taskbarOrder.Start(app.controller);app.Warmup();auto warm=Read();printf("warm_private_kb=%zu warm_working_kb=%zu desktops=%zu\n",size_t(warm.memory.PrivateUsage/1024),size_t(warm.memory.WorkingSetSize/1024),app.desktopCache.size());fflush(stdout);
    HANDLE sampler=CreateThread(nullptr,0,SampleIdle,nullptr,0,nullptr);app.Loop();WaitForSingleObject(sampler,1000);CloseHandle(sampler);
    printf("after_idle_desktops=%zu graphics_device_retained=%d timer_pending=%d taskbar_worker_running=%d cached_taskbar_entries=%zu\n",app.desktopCache.size(),app.device!=nullptr,app.idleTimer,app.taskbarOrder.Running(),app.taskbarOrder.Read().size());
    app.settings.memoryMode=MemoryMode::Saver;app.ReleaseInactiveGraphics();auto saver=Read();
    printf("saver_private_kb=%zu saver_working_kb=%zu graphics_device_released=%d\n",size_t(saver.memory.PrivateUsage/1024),size_t(saver.memory.WorkingSetSize/1024),!app.device);
    app.Cleanup();CloseHandle(g_stopEvent);g_stopEvent=nullptr;CoUninitialize();return 0;
}
