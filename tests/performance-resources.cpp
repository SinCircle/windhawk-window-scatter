#define SCATTER_STANDALONE
#define SCATTER_TEST
#ifndef SCATTER_SOURCE
#define SCATTER_SOURCE "../window-scatter.wh.cpp"
#endif
#include SCATTER_SOURCE
#include <psapi.h>
#include <chrono>
using namespace scatter;
static void Pump(){MSG m;while(PeekMessage(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}}
static LRESULT CALLBACK Fixture(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProc(w,m,a,b);}
int main(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitialize(nullptr);g_instance=GetModuleHandle(nullptr);g_probe=true;
    App app;App::instance=&app;if(!app.Initialize()||!app.EnsureDevice())return 1;
    WNDCLASS wc{};wc.hInstance=g_instance;wc.lpfnWndProc=Fixture;wc.lpszClassName=L"ScatterPerformanceFixture";wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);RegisterClass(&wc);
    puts("count,monitors,run,items_ms,layout_ms,titles_ms,same_titles_ms,wallpaper_ms,cached_wallpaper_ms,zorder_ms,same_zorder_ms,animate_ms,end_ms");fflush(stdout);
    for(int count:{16,64,128})for(int monitorCount:{1,2})for(int run=0;run<3;++run){
        EnumDisplayMonitors(nullptr,nullptr,App::AddMonitor,reinterpret_cast<LPARAM>(&app));while(app.views.size()>size_t(monitorCount))app.views.pop_back();
        std::vector<HWND> sources;std::vector<double> durations;
        for(int n=0;n<count;++n)sources.push_back(CreateWindowEx(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Fictional performance window",WS_POPUP,0,0,640+n%4*100,400+n%3*100,nullptr,nullptr,g_instance,nullptr));
        double t=app.Now();auto stamp=[&]{double next=app.Now();durations.push_back(1000*(next-t));t=next;};
        int sourceIndex=0;
        for(auto&v:app.views){if(!app.CreateView(*v))return 2;
            for(int n=0;n<count/int(app.views.size());++n){HWND w=sources[sourceIndex++];
                Item i;i.source=w;i.original={double(n%8*300),double(n%5*150),double(640+n%4*100),double(400+n%3*100)};
                if(!app.CreateItem(*v,i)){fprintf(stderr,"item failed %d\n",n);return 3;}v->items.push_back(std::move(i));}}
        stamp();
        for(auto&v:app.views){auto layout=App::Arrange(*v,false);if(layout.size()!=v->items.size())return 4;for(size_t n=0;n<layout.size();++n)v->items[n].target=layout[n];}stamp();
        for(auto&v:app.views)for(auto&i:v->items)if(!app.CreateTitle(*v,i))return 5;stamp();
        for(auto&v:app.views)for(auto&i:v->items)if(!app.CreateTitle(*v,i))return 6;stamp();
        for(auto&v:app.views)if(!app.CreateDesktop(*v))return 7;stamp();
        for(auto&v:app.views)if(!app.PrepareDesktop(*v))return 8;stamp();
        app.FollowNativeZOrder();stamp();app.FollowNativeZOrder();stamp();
        app.settings.duration=500;app.progress=0;app.Animate(1);stamp();
        app.End(false,false);stamp();Pump();for(HWND w:sources)DestroyWindow(w);
        printf("%d,%d,%d",count,monitorCount,run);for(double d:durations)printf(",%.3f",d);puts("");fflush(stdout);
    }
    app.Cleanup();CoUninitialize();return 0;
}
