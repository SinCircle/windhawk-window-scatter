// ==WindhawkMod==
// @id              window-scatter
// @name            Window Scatter
// @description     Win+Tab persistent overview with natural packing, rounded windows and compositor animations.
// @version         0.3.6
// @author          SinCircle
// @include         windhawk.exe
// @compilerOptions -ld3d11 -ldxgi -ldcomp -ldwmapi -ld2d1 -ldwrite -lwindowscodecs -lole32 -lshell32 -lgdi32 -luser32 -luuid
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Window Scatter

Win+Tab toggles the overview. Releasing the keys leaves the overview open.
Click a window to switch; Escape or a background click cancels. Tab does not
cycle candidates. Shift+Win+Tab opens the original Windows Task View, even when
this overview is already open. Alt+Tab retains its native behavior. Ctrl+Alt+Space and
the tray icon are alternative triggers. Window titles are shown, with a system-accent
selection outline with a 4 DIP transparent gap. Preview brightness is unchanged.
No instruction footer is drawn.

Windows use one common scale per monitor, preserving their relative dimensions.
The layout compares compact horizontal and vertical stacks, centers each smaller
window inside its actual group, and centers the entire arrangement. All windows
share one scale. Size, compactness, visual balance and the original left/right
and top/bottom relationships determine the arrangement. There are no equal-sized cells.

DirectComposition renders complete live window surfaces, C2-continuous corners,
linear bitmap filtering, antialiased clip edges, and non-linear critically damped
motion. Source rectangles use visible-frame coordinates relative to the outer
window, preventing invisible resize margins from shifting or clipping previews.
Animations are submitted once; there is
no application frame loop, screenshot loop or timer-resolution change. DWM runs
at display refresh. One-shot timers finish transitions and release the cached GPU
device after 15 seconds of inactivity.
One cached 64x64 corner mask is shared across windows. GPU transforms resize and
place its four copies; no masks or window screenshots are redrawn during animation.
The outline follows the same C2 path at a constant 4 DIP distance and tracks the
Windows accent color when opening the overview or receiving a theme notification.

An opaque wallpaper background covers the original windows during the overview.
The wallpaper is beneath all window previews, with the selection outline behind
its preview and titles above it. This is a wallpaper-backed fullscreen overview.
Overlays cover only monitor work areas, leaving the actual
taskbar visible and interactive. Original windows are not moved, resized, hidden
or made transparent. Return animation follows the actual native Z order after
activating the selected window. The last valid preview stays attached until all
overlays have been hidden and DWM has presented the handoff. Overlay windows have
native show/hide transitions disabled; DirectComposition owns the animation.

Requires Windows 11 and the private DWM shared-visual entry points (147,162).
They are checked at runtime; API failures abort the overview without changing
source windows. Windows updates can require adaptation. Protected surfaces may
be unavailable. Minimized windows and virtual-desktop management are out of scope.

Shared-visual API research: ADeltaX
https://gist.github.com/ADeltaX/aea6aac248604d0cb7d423a61b06e247
Windhawk 1.7.3 tool-process launcher: Ramen Software (copied below)
https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- replaceWinTab: true
  $name: Use Win+Tab
  $description: Win+Tab toggles the persistent overview. Click to switch. Shift+Win+Tab opens Windows Task View. Alt+Tab is unchanged.
- durationMs: 320
  $name: Animation duration (ms)
  $description: Non-linear motion executed by DirectComposition. 0 disables animation.
- cornerRadius: 12
  $name: Overview corner radius (DIP)
*/
// ==/WindhawkModSettings==

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dcomp.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace scatter {
using Microsoft::WRL::ComPtr;
constexpr wchar_t kController[]=L"WindowScatter.Controller.v2";
constexpr wchar_t kOverlay[]=L"WindowScatter.Overview.v2";
constexpr UINT kToggle=WM_APP+1,kSettings=WM_APP+2,kDismiss=WM_APP+3,kTray=WM_APP+4,kNativeTaskView=WM_APP+5;
constexpr ULONG_PTR kInputMarker=0x57534354;
struct Box {
    double x=0,y=0,w=0,h=0;
    double right()const{return x+w;} double bottom()const{return y+h;}
    double cx()const{return x+w*.5;} double cy()const{return y+h*.5;}
};
static bool Overlap(const Box&a,const Box&b){return a.x<b.right()-.01&&a.right()>b.x+.01&&a.y<b.bottom()-.01&&a.bottom()>b.y+.01;}
static bool Contains(const Box&a,const Box&b){return b.x>=a.x-.01&&b.y>=a.y-.01&&b.right()<=a.right()+.01&&b.bottom()<=a.bottom()+.01;}
static Box Mix(const Box&a,const Box&b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.w+(b.w-a.w)*t,a.h+(b.h-a.h)*t};}

// Compare compact, centered stacks in both orientations. Each stack sizes itself
// to its actual windows; there are no fixed cells or per-window scale changes.
// Closed-form fit avoids binary-searching a greedy packing feasibility test.
static std::vector<Box> Layout(const std::vector<Box>&src,Box area,double gap,double caption=0) {
    if(src.empty()||area.w<=0||area.h<=0)return {};
    const int count=int(src.size());
    for(const auto&b:src)if(b.w<=0||b.h<=0)return {};
    gap=std::min(gap,std::sqrt(area.w*area.h/count)*.10);
    std::vector<int> canonical(count);std::iota(canonical.begin(),canonical.end(),0);
    std::stable_sort(canonical.begin(),canonical.end(),[&](int a,int b){
        const auto&x=src[a];const auto&y=src[b];
        if(x.x!=y.x)return x.x<y.x;if(x.y!=y.y)return x.y<y.y;
        if(x.w!=y.w)return x.w>y.w;return x.h>y.h;
    });
    std::vector<int> rank(count);for(int n=0;n<count;++n)rank[canonical[n]]=n;
    double bestScore=-std::numeric_limits<double>::infinity();std::vector<Box> best;
    int limit=std::min(count,std::max(12,int(std::ceil(std::sqrt(double(count))*2))));
    for(bool columns:{true,false})for(int groups=1;groups<=limit;++groups)for(int strategy=0;strategy<3;++strategy){
        std::vector<int> order=canonical;
        std::stable_sort(order.begin(),order.end(),[&](int a,int b){
            if(strategy==2){double x=columns?src[a].cx():src[a].cy(),y=columns?src[b].cx():src[b].cy();return x!=y?x<y:rank[a]<rank[b];}
            double x=strategy?src[a].w*src[a].h:(columns?src[a].h:src[a].w);
            double y=strategy?src[b].w*src[b].h:(columns?src[b].h:src[b].w);
            return x!=y?x>y:rank[a]<rank[b];
        });
        std::vector<std::vector<int>> stack(groups);
        std::vector<double> load(groups),cross(groups);
        // Include title/gap cost in the load estimate. Geometry, never foreground
        // Z order, determines ties, so selecting a window cannot shuffle the pack.
        double estimate=std::min(.85,std::sqrt(area.w*area.h/std::accumulate(src.begin(),src.end(),0.,[](double s,const Box&b){return s+b.w*b.h;})));
        double remaining=0;for(int id:order)remaining+=(columns?src[id].h:src[id].w)*estimate+gap+(columns?caption:0);
        int spatialGroup=0,placed=0;
        for(int id:order){
            int target=0;double cost=std::numeric_limits<double>::infinity();
            double itemLoad=(columns?src[id].h:src[id].w)*estimate+gap+(columns?caption:0);
            if(strategy==2){
                // Contiguous spatial groups preserve left/right (or top/bottom)
                // order instead of distributing distant windows just by size.
                double goal=(remaining+load[spatialGroup])/(groups-spatialGroup);
                int left=count-placed,otherGroups=groups-spatialGroup-1;
                if(otherGroups>0&&!stack[spatialGroup].empty()&&
                   (left==otherGroups||std::abs(load[spatialGroup]-goal)<=std::abs(load[spatialGroup]+itemLoad-goal)))++spatialGroup;
                target=spatialGroup;
            }else for(int g=0;g<groups;++g){
                double extent=(columns?src[id].w:src[id].h)*estimate;
                double candidate=load[g]+.06*std::max(0.,extent-cross[g]);
                if(stack[g].empty())candidate=-1;
                if(candidate<cost){cost=candidate;target=g;}
            }
            stack[target].push_back(id);
            load[target]+=itemLoad;remaining-=itemLoad;++placed;
            cross[target]=std::max(cross[target],(columns?src[id].w:src[id].h)*estimate);
        }
        double scale=.85,totalCross=0;
        for(auto&group:stack){
            double along=0,wide=0;
            for(int id:group){along+=columns?src[id].h:src[id].w;wide=std::max(wide,columns?src[id].w:src[id].h);}
            totalCross+=wide;
            double available=(columns?area.h:area.w)-gap*(group.size()-1)-(columns?caption*group.size():0);
            scale=std::min(scale,available/along);
        }
        scale=std::min(scale,((columns?area.w:area.h)-gap*(groups-1)-(columns?0:caption*groups))/totalCross);
        if(scale<=0)continue;
        auto anchor=[&](const std::vector<int>&group){
            double sum=0,weight=0;for(int id:group){double a=src[id].w*src[id].h;sum+=(columns?src[id].cx():src[id].cy())*a;weight+=a;}return sum/weight;
        };
        std::stable_sort(stack.begin(),stack.end(),[&](const auto&a,const auto&b){return anchor(a)<anchor(b);});
        std::vector<Box> footprint(groups);
        double allCross=gap*(groups-1),allAlong=0;
        for(int g=0;g<groups;++g){
            auto&group=stack[g];
            std::stable_sort(group.begin(),group.end(),[&](int a,int b){double x=columns?src[a].cy():src[a].cx(),y=columns?src[b].cy():src[b].cx();return x!=y?x<y:rank[a]<rank[b];});
            double w=0,h=0;
            for(int id:group){double iw=src[id].w*scale,ih=src[id].h*scale+caption;if(columns){w=std::max(w,iw);h+=ih;}else{w+=iw;h=std::max(h,ih);}}
            if(columns)h+=gap*(group.size()-1);else w+=gap*(group.size()-1);
            footprint[g]={0,0,w,h};allCross+=columns?w:h;allAlong=std::max(allAlong,columns?h:w);
        }
        double width=columns?allCross:allAlong,height=columns?allAlong:allCross;
        double cursor=(columns?area.cx()-width*.5:area.cy()-height*.5);
        std::vector<Box> attempt(count);
        for(int g=0;g<groups;++g){
            auto f=footprint[g];
            double x=columns?cursor:area.cx()-f.w*.5,y=columns?area.cy()-f.h*.5:cursor;
            double originalAnchor=anchor(stack[g]);
            for(int id:stack[g]){
                double w=src[id].w*scale,h=src[id].h*scale;
                // Use available slack to preserve original cross-axis position.
                // Large windows bound the group; smaller ones need not all snap
                // to the exact same center line when originally left/right offset.
                double slack=std::max(0.,columns?f.w-w:f.h-h-caption);
                double shift=std::clamp(((columns?src[id].cx():src[id].cy())-originalAnchor)*scale*.65,-slack*.5,slack*.5);
                attempt[id]={columns?x+(f.w-w)*.5+shift:x,columns?y:y+(f.h-h-caption)*.5+shift,w,h};
                if(columns)y+=h+caption+gap;else x+=w+gap;
            }
            cursor+=(columns?f.w:f.h)+gap;
        }
        // Preserve clear pairwise directions without pinning windows to absolute
        // screen corners. Ambiguous nearly coincident centers have little weight.
        double occupied=0,mx=0,my=0,movement=0;
        for(int i=0;i<count;++i){auto&b=attempt[i];double a=b.w*(b.h+caption);occupied+=a;mx+=a*b.cx();my+=a*(b.cy()+caption*.5);
            double dx=(b.cx()-std::clamp(src[i].cx(),area.x,area.right()))/area.w,dy=(b.cy()-std::clamp(src[i].cy(),area.y,area.bottom()))/area.h;movement+=dx*dx+dy*dy;}
        double dx=(mx/occupied-area.cx())/area.w,dy=(my/occupied-area.cy())/area.h;
        double waste=1-occupied/(width*height);
        double relation=0,weights=0;
        for(int a=0;a<count;++a)for(int b=a+1;b<count;++b){
            double ox=(src[a].cx()-src[b].cx())/area.w,oy=(src[a].cy()-src[b].cy())/area.h;
            double nx=(attempt[a].cx()-attempt[b].cx())/area.w,ny=(attempt[a].cy()-attempt[b].cy())/area.h;
            auto axis=[&](double before,double after){
                double weight=std::clamp((std::abs(before)-.025)/.25,0.,1.);
                if(weight<=0)return;
                weights+=weight;double signedAfter=before>0?after:-after;
                // Collapsing a clear top/bottom relation into one row is also
                // a loss of orientation, even if it does not strictly reverse it.
                double separation=std::min(.12,std::abs(before)*scale*.5);
                relation+=weight*std::clamp((separation-signedAfter)/std::max(.001,separation),0.,2.);
            };
            axis(ox,nx);axis(oy,ny);
        }
        double score=std::log(scale)-.18*waste-.5*(dx*dx+dy*dy)-.08*movement/count-1.2*relation/std::max(1.,weights);
        if(score>bestScore+1e-10){bestScore=score;best=std::move(attempt);}
    }
    return best;
}

static double Curve(double t) {
    t=std::clamp(t,0.0,1.0);
    return (1-(1+9*t)*std::exp(-9*t))/(1-10*std::exp(-9.0));
}
static double CurveDerivative(double t){return 81*t*std::exp(-9*t)/(1-10*std::exp(-9.0));}

struct Settings{bool winTab=true;int duration=320;float radius=12;};
static std::atomic<bool> g_winTab{true};
static std::atomic<int> g_duration{320},g_radius{12};
static std::atomic<HWND> g_controller{nullptr};
static HANDLE g_thread=nullptr,g_stopEvent=nullptr,g_readyEvent=nullptr;
static std::atomic<bool> g_ready{false};
static HINSTANCE g_instance=nullptr;
#ifdef SCATTER_STANDALONE
static bool g_probe=false;
static FILE* g_report=nullptr;
#endif
static void Log(const wchar_t*where,HRESULT hr) {
#ifndef SCATTER_STANDALONE
    Wh_Log(L"%s: 0x%08X",where,static_cast<unsigned>(hr));
#else
    if(g_report){fwprintf(g_report,L"%ls: 0x%08X\n",where,static_cast<unsigned>(hr));fflush(g_report);}
#endif
}

struct PrivateDwm {
    using CreateThumb=HRESULT(WINAPI*)(HWND,HWND,DWORD,DWM_THUMBNAIL_PROPERTIES*,void*,void**,HTHUMBNAIL*);
    using QuerySize=HRESULT(WINAPI*)(HWND,BOOL,SIZE*);
    CreateThumb createThumb=nullptr;QuerySize querySize=nullptr;
    bool Load(){
        HMODULE dwm=GetModuleHandle(L"dwmapi.dll");
        createThumb=reinterpret_cast<CreateThumb>(GetProcAddress(dwm,MAKEINTRESOURCEA(147)));
        querySize=reinterpret_cast<QuerySize>(GetProcAddress(dwm,MAKEINTRESOURCEA(162)));
        return createThumb&&querySize;
    }
};
// Public Windows SDK interfaces omitted by Windhawk's MinGW headers. Match the
// SDK ABI (MinGW declares overloaded COM methods in reverse declaration order).
struct CompositeEffect:IDCompositionFilterEffect {
    virtual HRESULT STDMETHODCALLTYPE SetMode(D2D1_COMPOSITE_MODE)=0;
};
struct AffineEffect:IDCompositionFilterEffect {
    virtual HRESULT STDMETHODCALLTYPE SetInterpolationMode(D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE)=0;
    virtual HRESULT STDMETHODCALLTYPE SetBorderMode(D2D1_BORDER_MODE)=0;
    virtual HRESULT STDMETHODCALLTYPE SetTransformMatrix(const D2D1_MATRIX_3X2_F&)=0;
    virtual HRESULT STDMETHODCALLTYPE SetTransformMatrixElement(int,int,IDCompositionAnimation*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetTransformMatrixElement(int,int,float)=0;
    virtual HRESULT STDMETHODCALLTYPE SetSharpness(IDCompositionAnimation*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetSharpness(float)=0;
};
constexpr float kCornerTexture=64;
// Two cubic Beziers: equal first/second derivatives at their join, zero second
// derivative at the straight-edge endpoints. Unlike a circular arc, curvature
// reaches zero continuously at both straight edges.
static void ContinuousCorner(ID2D1GeometrySink*s,float x,float y,float xx,float xy,float yx,float yy){
    auto p=[&](float a,float b){return D2D1::Point2F(x+a*xx+b*yx,y+a*xy+b*yy);};
    s->AddBezier(D2D1::BezierSegment(p(1.f/3,0),p(2.f/3,0),p(5.f/6,1.f/6)));
    s->AddBezier(D2D1::BezierSegment(p(1,1.f/3),p(1,2.f/3),p(1,1)));
}
static float CornerExtent(double width,double height,float radius){return std::clamp(1.5f*radius,.001f,float(std::max(.001,std::min(width,height)*.5)));}
struct Item {
    HWND source=nullptr;
    Box original,target;
    SIZE pixels{};
    float nativeRadius=0;
    HTHUMBNAIL thumbnail=nullptr;
    ComPtr<IDCompositionVisual2> surface,position,content,border,borderFill,label;
    ComPtr<IDCompositionScaleTransform> scale,borderScale;
    ComPtr<IDCompositionRectangleClip> clip;
    ComPtr<AffineEffect> corners[4];
    ComPtr<CompositeEffect> cornerRows[2],cornerUnion,cornerMask;
    ComPtr<IDCompositionEffectGroup> borderEffect,labelEffect;
    ComPtr<IDCompositionSurface> labelSurface,borderSurface;
    float stroke=3,gap=4,borderWidth=1,borderHeight=1;
    float Outset()const{return stroke+gap;}
};
struct View {
    HWND hwnd=nullptr;HMONITOR monitor=nullptr;RECT bounds{};float dpi=1;
    std::vector<Item> items;int selected=-1;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual2> root,windows,desktop;
    ComPtr<IDCompositionSurface> wallpaperSurface;
};
enum class Phase{Hidden,Opening,Settled,Closing};

class App {
public:
    static App* instance;
    HWND controller=nullptr;HANDLE timer=nullptr;HHOOK keyboard=nullptr;
    HWINEVENTHOOK foregroundHook=nullptr,destroyHook=nullptr;
    ComPtr<IVirtualDesktopManager> desktops;
    ComPtr<ID3D11Device> d3d;
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDCompositionDesktopDevice> device;
    ComPtr<IDCompositionDevice3> effects;
    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<IDCompositionSurfaceFactory> surfaceFactory;
    ComPtr<IDCompositionSurface> cornerTexture;
    UINT32 accentColor=0;
    ComPtr<IDWriteFactory> textFactory;
    ComPtr<IWICImagingFactory> imageFactory;
    std::wstring cachedWallpaperPath;
    ComPtr<IWICBitmap> cachedWallpaper;
    PrivateDwm api;
    std::vector<std::unique_ptr<View>> views;
    Settings settings;Phase phase=Phase::Hidden;
    HWND previous=nullptr,chosen=nullptr,selectedSource=nullptr;
    POINT lastPointer{};
    bool swallowedTab=false,trayAdded=false,idleTimer=false,building=false;
    LARGE_INTEGER frequency{};
    double start=0,seconds=0,from=0,to=1,progress=0;
    LONGLONG animationStart=0;

    double Now()const{LARGE_INTEGER t;QueryPerformanceCounter(&t);return double(t.QuadPart)/frequency.QuadPart;}
    void LoadSettings(){settings={g_winTab.load(),std::clamp(g_duration.load(),0,900),float(std::clamp(g_radius.load(),0,32))};}
    static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){return instance?instance->Message(hwnd,msg,wp,lp):DefWindowProc(hwnd,msg,wp,lp);}
    static LRESULT CALLBACK Keyboard(int code,WPARAM wp,LPARAM lp){
        App*a=instance;
        if(code==HC_ACTION&&a){
            auto&k=*reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
            if(k.dwExtraInfo==kInputMarker)return CallNextHookEx(nullptr,code,wp,lp);
            bool down=wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN,up=wp==WM_KEYUP||wp==WM_SYSKEYUP;
            if(k.vkCode==VK_TAB){
                if(up&&a->swallowedTab){a->swallowedTab=false;return 1;}
                if(down&&a->swallowedTab)return 1; // No autorepeat toggles.
                bool win=(GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000);
                bool alt=(k.flags&LLKHF_ALTDOWN)||(GetAsyncKeyState(VK_MENU)&0x8000);
                bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
                if(down&&a->settings.winTab&&win&&!alt&&!ctrl){
                    a->swallowedTab=true;
                    // Prevent a bare Win key release from opening Start after
                    // we consume Tab. Our tagged events bypass this hook.
                    INPUT mask[2]{};for(auto&i:mask){i.type=INPUT_KEYBOARD;i.ki.wVk=0xFF;i.ki.dwExtraInfo=kInputMarker;}
                    mask[1].ki.dwFlags=KEYEVENTF_KEYUP;SendInput(2,mask,sizeof(INPUT));
                    PostMessage(a->controller,(GetAsyncKeyState(VK_SHIFT)&0x8000)?kNativeTaskView:kToggle,0,0);
                    return 1;
                }
            }
        }return CallNextHookEx(nullptr,code,wp,lp);
    }
    static void CALLBACK Event(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG child,DWORD,DWORD){
        App*a=instance;if(!a||a->phase==Phase::Hidden)return;
        if(event==EVENT_SYSTEM_FOREGROUND)PostMessage(a->controller,kDismiss,0,0);
        else if(event==EVENT_OBJECT_DESTROY&&object==OBJID_WINDOW&&child==0)
            for(auto&v:a->views)for(auto&i:v->items)if(i.source==hwnd){PostMessage(a->controller,kDismiss,1,0);return;}
    }
    bool IsOurWindow(HWND hwnd)const{if(hwnd==controller)return true;for(auto&v:views)if(v->hwnd==hwnd)return true;return false;}
    bool Initialize(){
        QueryPerformanceFrequency(&frequency);SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);LoadSettings();RefreshAccent();
        if(!api.Load()){Log(L"Shared DWM APIs unavailable",E_NOINTERFACE);return false;}
        WNDCLASSEX wc{sizeof(wc)};wc.lpfnWndProc=WndProc;wc.hInstance=g_instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=kController;
        if(!RegisterClassEx(&wc))return false;wc.lpszClassName=kOverlay;if(!RegisterClassEx(&wc))return false;
        controller=CreateWindowEx(WS_EX_TOOLWINDOW,kController,L"Window Scatter Controller",0,0,0,0,0,nullptr,nullptr,g_instance,nullptr);
        if(!controller)return false;g_controller.store(controller);
        timer=CreateWaitableTimer(nullptr,FALSE,nullptr);if(!timer)return false;
        CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&desktops));
#ifdef SCATTER_STANDALONE
        if(g_probe)return true;
#endif
        if(settings.winTab){keyboard=SetWindowsHookEx(WH_KEYBOARD_LL,Keyboard,g_instance,0);if(!keyboard)return false;}
        RegisterHotKey(controller,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_SPACE);
        NOTIFYICONDATA ni{sizeof(ni)};ni.hWnd=controller;ni.uID=1;ni.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
        ni.uCallbackMessage=kTray;ni.hIcon=LoadIcon(nullptr,IDI_APPLICATION);wcscpy_s(ni.szTip,L"Window Scatter | Win+Tab");trayAdded=Shell_NotifyIcon(NIM_ADD,&ni)!=FALSE;
        return true;
    }
    bool EnsureDevice(){
        if(device)return true;
        HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d3d,nullptr,nullptr);
        if(SUCCEEDED(hr))hr=d3d.As(&dxgi);
        if(SUCCEEDED(hr))hr=DCompositionCreateDevice3(dxgi.Get(),IID_PPV_ARGS(&device));
        if(SUCCEEDED(hr))hr=device.As(&effects);
        if(SUCCEEDED(hr))hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,IID_PPV_ARGS(&d2dFactory));
        if(SUCCEEDED(hr))hr=d2dFactory->CreateDevice(dxgi.Get(),&d2dDevice);
        if(SUCCEEDED(hr))hr=device->CreateSurfaceFactory(d2dDevice.Get(),&surfaceFactory);
        if(SUCCEEDED(hr))hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory.GetAddressOf()));
        if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imageFactory));
        if(FAILED(hr)){Log(L"Create compositor",hr);ReleaseDevice();return false;}return true;
    }
    void ReleaseDevice(){cachedWallpaper.Reset();cachedWallpaperPath.clear();cornerTexture.Reset();imageFactory.Reset();textFactory.Reset();surfaceFactory.Reset();d2dDevice.Reset();d2dFactory.Reset();effects.Reset();device.Reset();dxgi.Reset();d3d.Reset();}
    template<class Draw> HRESULT Paint(ComPtr<IDCompositionSurface>&surface,UINT width,UINT height,bool opaque,Draw draw){
        HRESULT hr=surfaceFactory->CreateSurface(width,height,DXGI_FORMAT_B8G8R8A8_UNORM,
            opaque?DXGI_ALPHA_MODE_IGNORE:DXGI_ALPHA_MODE_PREMULTIPLIED,surface.ReleaseAndGetAddressOf());
        if(FAILED(hr))return hr;
        ComPtr<ID2D1DeviceContext> ctx;POINT offset{};
        hr=surface->BeginDraw(nullptr,IID_PPV_ARGS(&ctx),&offset);if(FAILED(hr))return hr;
        ctx->SetDpi(96,96);ctx->SetTransform(D2D1::Matrix3x2F::Translation(float(offset.x),float(offset.y)));
        ctx->PushAxisAlignedClip(D2D1::RectF(0,0,float(width),float(height)),D2D1_ANTIALIAS_MODE_ALIASED);
        ctx->Clear(D2D1::ColorF(0,0.f));draw(ctx.Get());ctx->PopAxisAlignedClip();
        return surface->EndDraw();
    }
    bool CreateDesktop(View&v){
        MONITORINFO mi{sizeof(mi)};GetMonitorInfo(v.monitor,&mi);
        RECT display=mi.rcMonitor;COLORREF color=GetSysColor(COLOR_DESKTOP);
        DESKTOP_WALLPAPER_POSITION mode=DWPOS_FILL;std::wstring path;
        ComPtr<IDesktopWallpaper> wallpaper;
        if(SUCCEEDED(CoCreateInstance(CLSID_DesktopWallpaper,nullptr,CLSCTX_ALL,IID_PPV_ARGS(&wallpaper)))){
            wallpaper->GetBackgroundColor(&color);wallpaper->GetPosition(&mode);
            UINT monitors=0;wallpaper->GetMonitorDevicePathCount(&monitors);
            for(UINT n=0;n<monitors;++n){LPWSTR id=nullptr;if(FAILED(wallpaper->GetMonitorDevicePathAt(n,&id)))continue;
                RECT rect{};wallpaper->GetMonitorRECT(id,&rect);
                if(EqualRect(&rect,&display)){LPWSTR file=nullptr;if(SUCCEEDED(wallpaper->GetWallpaper(id,&file))&&file){path=file;CoTaskMemFree(file);}}
                CoTaskMemFree(id);if(!path.empty())break;
            }
        }
        if(path.empty()){wchar_t file[32768]{};if(SystemParametersInfo(SPI_GETDESKWALLPAPER,32768,file,0))path=file;}
        if(path!=cachedWallpaperPath){
            cachedWallpaper.Reset();cachedWallpaperPath=path;
            ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
            HRESULT hr=path.empty()?E_FAIL:imageFactory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
            if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
            if(SUCCEEDED(hr))hr=imageFactory->CreateFormatConverter(&converter);
            if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
            if(SUCCEEDED(hr))imageFactory->CreateBitmapFromSource(converter.Get(),WICBitmapCacheOnLoad,&cachedWallpaper);
        }
        if(mode==DWPOS_SPAN)display={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN)};
        UINT width=v.bounds.right-v.bounds.left,height=v.bounds.bottom-v.bounds.top;
        HRESULT hr=Paint(v.wallpaperSurface,width,height,true,[&](ID2D1DeviceContext*ctx){
            // This opaque base is mandatory: excluding a live window may leave
            // transparent pixels, which otherwise expose the ORIGINAL window.
            ctx->Clear(D2D1::ColorF(GetRValue(color)/255.f,GetGValue(color)/255.f,GetBValue(color)/255.f,1.f));
            if(!cachedWallpaper)return;ComPtr<ID2D1Bitmap> bitmap;
            if(FAILED(ctx->CreateBitmapFromWicBitmap(cachedWallpaper.Get(),nullptr,&bitmap)))return;
            auto size=bitmap->GetSize();if(size.width<1||size.height<1)return;
            float mw=float(display.right-display.left),mh=float(display.bottom-display.top);
            float x=float(display.left-v.bounds.left),y=float(display.top-v.bounds.top),w=size.width,h=size.height;
            if(mode==DWPOS_TILE){for(float ty=y;ty<float(height);ty+=h)for(float tx=x;tx<float(width);tx+=w)ctx->DrawBitmap(bitmap.Get(),D2D1::RectF(tx,ty,tx+w,ty+h));return;}
            if(mode==DWPOS_STRETCH){w=mw;h=mh;}
            else if(mode!=DWPOS_CENTER){float scale=mode==DWPOS_FIT?std::min(mw/w,mh/h):std::max(mw/w,mh/h);w*=scale;h*=scale;}
            x+=(mw-w)*.5f;y+=(mh-h)*.5f;ctx->DrawBitmap(bitmap.Get(),D2D1::RectF(x,y,x+w,y+h));
        });
        if(SUCCEEDED(hr))hr=device->CreateVisual(&v.desktop);
        if(SUCCEEDED(hr))hr=v.desktop->SetContent(v.wallpaperSurface.Get());
        // With a null reference, AddVisual(FALSE) inserts ABOVE all siblings.
        // An explicit sibling reference keeps the wallpaper below the windows.
        if(SUCCEEDED(hr))hr=v.root->AddVisual(v.desktop.Get(),FALSE,v.windows.Get());
        if(FAILED(hr)){Log(L"Opaque desktop",hr);return false;}
        return true;
    }
    void RefreshAccent(){
        DWORD color=0;BOOL opaque=FALSE;UINT32 rgb;
        if(SUCCEEDED(DwmGetColorizationColor(&color,&opaque)))rgb=color&0xFFFFFF; // ARGB, not COLORREF.
        else{COLORREF c=GetSysColor(COLOR_HIGHLIGHT);rgb=(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c);}
        if(rgb==accentColor)return;accentColor=rgb;
        if(!building&&phase!=Phase::Hidden&&device){
            for(auto&v:views)for(auto&i:v->items)if(!CreateOutline(*v,i)){End(false);return;}
            device->Commit();
        }
    }
    bool CreateCorners(Item&i){
        HRESULT hr=S_OK;
        if(!cornerTexture){
            ComPtr<ID2D1PathGeometry> path;ComPtr<ID2D1GeometrySink> sink;
            hr=d2dFactory->CreatePathGeometry(&path);if(SUCCEEDED(hr))hr=path->Open(&sink);
            if(SUCCEEDED(hr)){
                sink->BeginFigure(D2D1::Point2F(0,0),D2D1_FIGURE_BEGIN_FILLED);sink->AddLine(D2D1::Point2F(0,kCornerTexture));
                ContinuousCorner(sink.Get(),0,kCornerTexture,0,-kCornerTexture,kCornerTexture,0);
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);hr=sink->Close();
            }
            if(SUCCEEDED(hr))hr=Paint(cornerTexture,UINT(kCornerTexture),UINT(kCornerTexture),false,[&](ID2D1DeviceContext*c){
                ComPtr<ID2D1SolidColorBrush>b;if(SUCCEEDED(c->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&b)))c->FillGeometry(path.Get(),b.Get());
            });
        }
        if(SUCCEEDED(hr))hr=effects->CreateCompositeEffect(reinterpret_cast<void**>(i.cornerUnion.GetAddressOf()));
        if(SUCCEEDED(hr))hr=i.cornerUnion->SetMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
        for(int row=0;row<2&&SUCCEEDED(hr);++row){
            // Binary combines: some DWM versions silently ignore inputs > 1.
            hr=effects->CreateCompositeEffect(reinterpret_cast<void**>(i.cornerRows[row].GetAddressOf()));
            if(SUCCEEDED(hr))hr=i.cornerRows[row]->SetMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
            if(SUCCEEDED(hr))hr=i.cornerUnion->SetInput(row,i.cornerRows[row].Get(),0);
        }
        for(int n=0;n<4&&SUCCEEDED(hr);++n){
            hr=effects->CreateAffineTransform2DEffect(reinterpret_cast<void**>(i.corners[n].GetAddressOf()));
            if(SUCCEEDED(hr))hr=i.corners[n]->SetInput(0,cornerTexture.Get(),0);
            if(SUCCEEDED(hr))hr=i.corners[n]->SetInterpolationMode(D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_MULTI_SAMPLE_LINEAR);
            // Clamp this MASK's exterior texels. Sampling transparent pixels
            // beyond its edge leaves a thin uncut frame around the preview.
            // The live window still uses soft, antialiased composition.
            if(SUCCEEDED(hr))hr=i.corners[n]->SetBorderMode(D2D1_BORDER_MODE_HARD);
            if(SUCCEEDED(hr))hr=i.cornerRows[n/2]->SetInput(n%2,i.corners[n].Get(),0);
        }
        if(SUCCEEDED(hr))hr=effects->CreateCompositeEffect(reinterpret_cast<void**>(i.cornerMask.GetAddressOf()));
        if(SUCCEEDED(hr))hr=i.cornerMask->SetMode(D2D1_COMPOSITE_MODE_DESTINATION_OUT);
        if(SUCCEEDED(hr))hr=i.cornerMask->SetInput(0,nullptr,0); // Live visual subtree; preserve its alpha.
        if(SUCCEEDED(hr))hr=i.cornerMask->SetInput(1,i.cornerUnion.Get(),0);
        if(SUCCEEDED(hr))hr=i.content->SetEffect(i.cornerMask.Get());
        if(FAILED(hr))Log(L"Create continuous corners",hr);return SUCCEEDED(hr);
    }
    bool CreateOutline(View&v,Item&i){
        // A hollow premultiplied surface leaves the requested gap transparent.
        // Rasterize once at overview size, then animate its transform on the GPU.
        i.borderWidth=float(i.target.w)+2*i.Outset();i.borderHeight=float(i.target.h)+2*i.Outset();
        ComPtr<ID2D1PathGeometry> path;ComPtr<ID2D1GeometrySink> sink;
        HRESULT hr=d2dFactory->CreatePathGeometry(&path);if(SUCCEEDED(hr))hr=path->Open(&sink);
        if(FAILED(hr))return false;
        float o=i.Outset(),w=float(i.target.w),h=float(i.target.h),r=CornerExtent(w,h,settings.radius*v.dpi);
        sink->BeginFigure(D2D1::Point2F(o+r,o),D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(o+w-r,o));ContinuousCorner(sink.Get(),o+w-r,o,r,0,0,r);
        sink->AddLine(D2D1::Point2F(o+w,o+h-r));ContinuousCorner(sink.Get(),o+w,o+h-r,0,r,-r,0);
        sink->AddLine(D2D1::Point2F(o+r,o+h));ContinuousCorner(sink.Get(),o+r,o+h,-r,0,0,-r);
        sink->AddLine(D2D1::Point2F(o,o+r));ContinuousCorner(sink.Get(),o,o+r,0,-r,r,0);
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);hr=sink->Close();if(FAILED(hr))return false;
        hr=Paint(i.borderSurface,UINT(std::ceil(i.borderWidth)),UINT(std::ceil(i.borderHeight)),false,[&](ID2D1DeviceContext*ctx){
            ComPtr<ID2D1SolidColorBrush> brush,clear;
            if(FAILED(ctx->CreateSolidColorBrush(D2D1::ColorF(accentColor),&brush))||FAILED(ctx->CreateSolidColorBrush(D2D1::ColorF(0,0.f),&clear)))return;
            ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            ctx->DrawGeometry(path.Get(),brush.Get(),2*i.Outset());
            // Erase the interior and gap. This creates a true constant-distance
            // offset of the SAME curve, including on transparent source windows.
            ctx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            ctx->FillGeometry(path.Get(),clear.Get());ctx->DrawGeometry(path.Get(),clear.Get(),2*i.gap);
        });
        if(SUCCEEDED(hr))hr=i.borderFill->SetContent(i.borderSurface.Get());
        return SUCCEEDED(hr);
    }
    bool CreateTitle(View&v,Item&i){
        if(!CreateOutline(v,i))return false;
        wchar_t title[512]{};GetWindowText(i.source,title,512);if(!title[0])wcscpy_s(title,L"Window");
        UINT width=UINT(std::max(1.,std::ceil(i.target.w))),height=UINT(std::ceil(25*v.dpi));
        HRESULT hr=Paint(i.labelSurface,width,height,false,[&](ID2D1DeviceContext*ctx){
            ComPtr<IDWriteTextFormat> format;ComPtr<IDWriteTextLayout> layout;
            ComPtr<ID2D1SolidColorBrush> textBrush,bgBrush;
            if(FAILED(textFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13*v.dpi,L"",&format)))return;
            format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            if(FAILED(textFactory->CreateTextLayout(title,UINT32(wcslen(title)),format.Get(),std::max(1.f,float(width)-16*v.dpi),float(height),&layout)))return;
            ComPtr<IDWriteInlineObject> ellipsis;textFactory->CreateEllipsisTrimmingSign(format.Get(),&ellipsis);DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};layout->SetTrimming(&trimming,ellipsis.Get());
            ctx->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&textBrush);ctx->CreateSolidColorBrush(D2D1::ColorF(0x16191E,.88f),&bgBrush);
            if(!textBrush||!bgBrush)return;
            DWRITE_TEXT_METRICS metrics{};layout->GetMetrics(&metrics);float half=std::min(float(width)*.5f,metrics.width*.5f+10*v.dpi),mid=float(width)*.5f;
            ctx->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(mid-half,0,mid+half,float(height)),6*v.dpi,6*v.dpi),bgBrush.Get());
            ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);ctx->DrawTextLayout(D2D1::Point2F(8*v.dpi,0),layout.Get(),textBrush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        });
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.label);
        if(SUCCEEDED(hr))hr=device->CreateEffectGroup(&i.labelEffect);
        if(SUCCEEDED(hr))hr=i.label->SetContent(i.labelSurface.Get());
        if(SUCCEEDED(hr))hr=i.label->SetEffect(i.labelEffect.Get());
        if(SUCCEEDED(hr))hr=i.position->AddVisual(i.label.Get(),TRUE,i.content.Get());
        if(SUCCEEDED(hr)){i.label->SetOffsetY(float(i.target.h+i.Outset()+6*v.dpi));i.labelEffect->SetOpacity(0.f);}
        return SUCCEEDED(hr);
    }
    void Cleanup(){
        End(false,false);if(keyboard){UnhookWindowsHookEx(keyboard);keyboard=nullptr;}
        if(controller){UnregisterHotKey(controller,1);if(trayAdded){NOTIFYICONDATA ni{sizeof(ni)};ni.hWnd=controller;ni.uID=1;Shell_NotifyIcon(NIM_DELETE,&ni);}g_controller.store(nullptr);DestroyWindow(controller);controller=nullptr;}
        if(timer){CloseHandle(timer);timer=nullptr;}ReleaseDevice();desktops.Reset();UnregisterClass(kOverlay,g_instance);UnregisterClass(kController,g_instance);
    }
    void Schedule(double delay){LARGE_INTEGER due;due.QuadPart=-std::max<LONGLONG>(1,LONGLONG(delay*10000000));SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);}
    void Loop(){
        HANDLE handles[]{g_stopEvent,timer};
        for(;;){
            DWORD r=MsgWaitForMultipleObjectsEx(2,handles,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(r==WAIT_OBJECT_0||r==WAIT_FAILED)break;
            if(r==WAIT_OBJECT_0+1){if(idleTimer&&phase==Phase::Hidden){idleTimer=false;ReleaseDevice();}else FinishTransition();}
            MSG msg;for(int n=0;n<64&&PeekMessage(&msg,nullptr,0,0,PM_REMOVE);++n){if(msg.message==WM_QUIT)return;TranslateMessage(&msg);DispatchMessage(&msg);}
        }
    }
    static BOOL CALLBACK AddMonitor(HMONITOR monitor,HDC,LPRECT,LPARAM param){
        auto&a=*reinterpret_cast<App*>(param);MONITORINFO mi{sizeof(mi)};if(!GetMonitorInfo(monitor,&mi))return TRUE;
        auto v=std::make_unique<View>();v->monitor=monitor;v->bounds=mi.rcWork;a.views.push_back(std::move(v));return TRUE;
    }
    static bool Bounds(HWND hwnd,RECT&r){return SUCCEEDED(DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))||GetWindowRect(hwnd,&r);}
    static RECT SourceRect(const RECT&outer,const RECT&frame,SIZE source){
        LONG width=frame.right-frame.left,height=frame.bottom-frame.top;
        auto same=[](LONG a,LONG b){return std::abs(a-b)<=1;};
        bool valid=width>0&&height>0&&frame.left>=outer.left&&frame.top>=outer.top&&frame.right<=outer.right&&frame.bottom<=outer.bottom;
        bool knownSize=(same(source.cx,width)&&same(source.cy,height))||
                       (same(source.cx,outer.right-outer.left)&&same(source.cy,outer.bottom-outer.top));
        // Private DWM reports visible-frame SIZE, but rcSource still uses the
        // outer-window ORIGIN. Its right edge may therefore exceed source.cx.
        // Cropping by size again would cut away the right/bottom edge a second time.
        if(valid&&knownSize)return {frame.left-outer.left,frame.top-outer.top,frame.right-outer.left,frame.bottom-outer.top};
        return {0,0,source.cx,source.cy};
    }
    static BOOL CALLBACK AddWindow(HWND hwnd,LPARAM param){
        auto&a=*reinterpret_cast<App*>(param);if(!IsWindowVisible(hwnd)||IsIconic(hwnd))return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);if(pid==GetCurrentProcessId())return TRUE;
        LONG_PTR ex=GetWindowLongPtr(hwnd,GWL_EXSTYLE);if(ex&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))return TRUE;
        if(!(ex&WS_EX_APPWINDOW)){
            HWND walk=GetAncestor(hwnd,GA_ROOTOWNER);
            for(int j=0;j<16;++j){HWND next=GetLastActivePopup(walk);if(walk==next)break;walk=next;if(IsWindowVisible(walk))break;}
            if(walk!=hwnd)return TRUE;
        }
        DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked)return TRUE;
        if(a.desktops){BOOL current=TRUE;if(SUCCEEDED(a.desktops->IsWindowOnCurrentVirtualDesktop(hwnd,&current))&&!current)return TRUE;}
        wchar_t title[2];if(!GetWindowText(hwnd,title,2))return TRUE;
        wchar_t cls[256];GetClassName(hwnd,cls,256);if(!wcscmp(cls,L"Progman")||!wcscmp(cls,L"WorkerW")||!wcscmp(cls,L"Shell_TrayWnd"))return TRUE;
        RECT bounds{};if(!Bounds(hwnd,bounds)||bounds.right-bounds.left<32||bounds.bottom-bounds.top<32)return TRUE;
        HMONITOR mon=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
        for(auto&v:a.views)if(v->monitor==mon){Item i;i.source=hwnd;i.original={double(bounds.left-v->bounds.left),double(bounds.top-v->bounds.top),double(bounds.right-bounds.left),double(bounds.bottom-bounds.top)};v->items.push_back(std::move(i));break;}return TRUE;
    }
    bool CreateView(View&v){
        RECT r=v.bounds;
        v.hwnd=CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,kOverlay,L"Window Scatter",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,g_instance,&v);
        if(!v.hwnd)return false;
        v.dpi=GetDpiForWindow(v.hwnd)/96.f;
        // Our compositor already animates the scene. A native hide transition
        // could keep displaying this HWND after its composition tree is freed.
        BOOL disabled=TRUE;
        HRESULT hr=DwmSetWindowAttribute(v.hwnd,DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled));
        if(SUCCEEDED(hr))hr=device->CreateTargetForHwnd(v.hwnd,FALSE,&v.target);
        if(SUCCEEDED(hr))hr=device->CreateVisual(&v.root);
        if(SUCCEEDED(hr))hr=v.root->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
        if(SUCCEEDED(hr))hr=v.root->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
        if(SUCCEEDED(hr))hr=device->CreateVisual(&v.windows);
        if(SUCCEEDED(hr))hr=v.target->SetRoot(v.root.Get());
        if(SUCCEEDED(hr))hr=v.root->AddVisual(v.windows.Get(),TRUE,nullptr);
        if(FAILED(hr)){Log(L"Create view",hr);return false;}return true;
    }
    bool CreateItem(View&v,Item&i){
        HRESULT hr=api.querySize(i.source,FALSE,&i.pixels);
        if(FAILED(hr)||i.pixels.cx<=0||i.pixels.cy<=0)return false;
        DWM_THUMBNAIL_PROPERTIES p{};
        p.dwFlags=DWM_TNP_VISIBLE|DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY|DWM_TNP_RECTDESTINATION|DWM_TNP_RECTSOURCE|0x04000000;
        p.fVisible=TRUE;p.opacity=255;p.fSourceClientAreaOnly=FALSE;
        // DWM preserves per-pixel alpha and color keys, but the explicit
        // thumbnail opacity replaces a layered source's whole-window alpha.
        COLORREF key=0;BYTE alpha=255;DWORD flags=0;
        if(GetLayeredWindowAttributes(i.source,&key,&alpha,&flags)&&(flags&LWA_ALPHA))p.opacity=alpha;
        RECT outer{},frame{};p.rcSource={0,0,i.pixels.cx,i.pixels.cy};
        if(GetWindowRect(i.source,&outer)&&SUCCEEDED(DwmGetWindowAttribute(i.source,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))))
            p.rcSource=SourceRect(outer,frame,i.pixels);
        // Normalize the sampled visible frame to a zero-based destination.
        i.pixels={p.rcSource.right-p.rcSource.left,p.rcSource.bottom-p.rcSource.top};
        p.rcDestination={0,0,i.pixels.cx,i.pixels.cy};
        hr=api.createThumb(v.hwnd,i.source,2,&p,device.Get(),reinterpret_cast<void**>(i.surface.GetAddressOf()),&i.thumbnail);
        if(FAILED(hr)){Log(L"Create shared source",hr);return false;}
        // DWM's shared visual may reject rendering-mode setters. Apply them to
        // our own parent visual; descendants inherit the composition settings.
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.position);
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.content);
        if(SUCCEEDED(hr))hr=device->CreateScaleTransform(&i.scale);
        if(SUCCEEDED(hr))hr=device->CreateRectangleClip(&i.clip);
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.border);
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.borderFill);
        if(SUCCEEDED(hr))hr=device->CreateScaleTransform(&i.borderScale);
        if(SUCCEEDED(hr))hr=device->CreateEffectGroup(&i.borderEffect);
        if(SUCCEEDED(hr))hr=i.content->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
        if(SUCCEEDED(hr))hr=i.content->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
        if(SUCCEEDED(hr))hr=i.border->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
        if(FAILED(hr)){Log(L"Create window surface",hr);return false;}
        i.stroke=3*v.dpi;i.gap=4*v.dpi;
        i.surface->SetTransform(i.scale.Get());i.content->AddVisual(i.surface.Get(),TRUE,nullptr);i.content->SetClip(i.clip.Get());
        i.position->AddVisual(i.content.Get(),TRUE,nullptr);
        i.borderFill->SetTransform(i.borderScale.Get());
        i.border->AddVisual(i.borderFill.Get(),TRUE,nullptr);
        i.border->SetOffsetX(-i.Outset());i.border->SetOffsetY(-i.Outset());
        i.borderEffect->SetOpacity(0.f);i.border->SetEffect(i.borderEffect.Get());
        i.position->AddVisual(i.border.Get(),FALSE,i.content.Get());
        i.nativeRadius=IsZoomed(i.source)?0.f:8.f*v.dpi;
        DWORD preference=DWMWCP_DEFAULT;
        if(SUCCEEDED(DwmGetWindowAttribute(i.source,DWMWA_WINDOW_CORNER_PREFERENCE,&preference,sizeof(preference)))&&preference==DWMWCP_DONOTROUND)i.nativeRadius=0;
        i.clip->SetLeft(0.f);i.clip->SetTop(0.f);
        return CreateCorners(i);
    }
    bool Begin(){
        if(phase!=Phase::Hidden)return false;
        CancelWaitableTimer(timer);idleTimer=false;
        previous=GetForegroundWindow();chosen=nullptr;progress=0;building=true;RefreshAccent();
        if(!EnsureDevice()){building=false;return false;}
        EnumDisplayMonitors(nullptr,nullptr,AddMonitor,reinterpret_cast<LPARAM>(this));
        EnumWindows(AddWindow,reinterpret_cast<LPARAM>(this));
        for(auto&v:views)if(!v->items.empty()&&!CreateView(*v)){building=false;End(false);return false;}
        size_t count=0;
        for(auto&v:views)if(v->hwnd){
            std::erase_if(v->items,[&](Item&i){return !CreateItem(*v,i);});
            std::stable_sort(v->items.begin(),v->items.end(),[](const Item&a,const Item&b){
                if(a.original.x!=b.original.x)return a.original.x<b.original.x;
                if(a.original.y!=b.original.y)return a.original.y<b.original.y;
                if(a.original.w!=b.original.w)return a.original.w>b.original.w;
                if(a.original.h!=b.original.h)return a.original.h>b.original.h;
                return reinterpret_cast<ULONG_PTR>(a.source)<reinterpret_cast<ULONG_PTR>(b.source);
            });
            std::vector<Box> original;for(auto&i:v->items)original.push_back(i.original);
            double pad=28*v->dpi;Box area{pad,pad,std::max(1.,double(v->bounds.right-v->bounds.left)-2*pad),std::max(1.,double(v->bounds.bottom-v->bounds.top)-2*pad)};
            auto layout=Layout(original,area,22*v->dpi,40*v->dpi);
            if(layout.size()!=v->items.size()){building=false;End(false);return false;}
            for(size_t j=0;j<layout.size();++j){v->items[j].target=layout[j];if(!CreateTitle(*v,v->items[j])){building=false;End(false);return false;}}
            count+=v->items.size();
        }
        if(!count){building=false;End(false);return false;}
        FollowNativeZOrder();
        selectedSource=nullptr; // Mouse hover/click chooses; no keyboard candidate.
        GetCursorPos(&lastPointer);
        for(auto&v:views)if(v->hwnd&&!CreateDesktop(*v)){building=false;End(false);return false;}
        StaticPose(0);
        if(FAILED(device->Commit())){building=false;End(false);return false;}
        device->WaitForCommitCompletion();
        phase=Phase::Opening;
        POINT cursor{};GetCursorPos(&cursor);HMONITOR active=MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);HWND focus=nullptr;
        for(auto&v:views)if(v->hwnd){ShowWindow(v->hwnd,SW_SHOWNOACTIVATE);if(!focus||v->monitor==active)focus=v->hwnd;}
        if(focus){SetForegroundWindow(focus);SetFocus(focus);}
        foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT);
        destroyHook=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_DESTROY,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        building=false;Animate(1);return true;
    }
    template<class Setter> void Property(double a,double b,Setter setter){
        if(seconds<=0||std::abs(b-a)<.00001){setter(float(b));return;}
        ComPtr<IDCompositionAnimation> animation;
        if(FAILED(device->CreateAnimation(&animation))){setter(float(b));return;}
        // Twelve Hermite cubics approximate critically damped motion. All share
        // an absolute QPC start so position, scale and corner clips stay in sync.
        LARGE_INTEGER begin{};begin.QuadPart=animationStart;animation->SetAbsoluteBeginTime(begin);
        constexpr int parts=12;double dt=seconds/parts,delta=b-a;
        for(int j=0;j<parts;++j){
            double u=double(j)/parts,v=double(j+1)/parts;
            double f0=a+delta*Curve(u),f1=a+delta*Curve(v);
            double m0=delta*CurveDerivative(u)/seconds,m1=delta*CurveDerivative(v)/seconds;
            double c2=(3*(f1-f0)/dt-2*m0-m1)/dt,c3=(2*(f0-f1)/dt+m0+m1)/(dt*dt);
            animation->AddCubic(j*dt,float(f0),float(m0),float(c2),float(c3));
        }
        animation->End(seconds,float(b));setter(animation.Get());
    }
    static void Corners(Item&i,const Box&b,float radius){
        float s=CornerExtent(b.w,b.h,radius)/kCornerTexture;
        for(int n=0;n<4;++n)i.corners[n]->SetTransformMatrix(D2D1::Matrix3x2F((n&1)?-s:s,0,0,(n&2)?-s:s,(n&1)?float(b.w):0,(n&2)?float(b.h):0));
    }
    void StaticPose(double p){
        for(auto&v:views)for(auto&i:v->items){
            Box b=Mix(i.original,i.target,p);i.position->SetOffsetX(float(b.x));i.position->SetOffsetY(float(b.y));
            i.scale->SetScaleX(float(b.w/i.pixels.cx));i.scale->SetScaleY(float(b.h/i.pixels.cy));
            i.clip->SetRight(float(b.w));i.clip->SetBottom(float(b.h));
            i.borderScale->SetScaleX(float(b.w+2*i.Outset())/i.borderWidth);i.borderScale->SetScaleY(float(b.h+2*i.Outset())/i.borderHeight);
            if(i.label){i.label->SetOffsetX(float((b.w-i.target.w)*.5));i.label->SetOffsetY(float(b.h+i.Outset()+6*v->dpi));i.labelEffect->SetOpacity(float(p));}
            Corners(i,b,float(i.nativeRadius+(settings.radius*v->dpi-i.nativeRadius)*p));
        }
    }
    double CurrentProgress(){
        if(phase==Phase::Settled)return 1;
        if(phase==Phase::Hidden)return 0;
        return from+(to-from)*Curve(seconds<=0?1:(Now()-start)/seconds);
    }
    void Animate(double destination){
        from=progress;to=destination;
        BOOL animations=TRUE;SystemParametersInfo(SPI_GETCLIENTAREAANIMATION,0,&animations,0);
        seconds=(animations?settings.duration:0)/1000.0*std::abs(to-from);
        LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);animationStart=qpc.QuadPart+frequency.QuadPart/120;
        start=double(animationStart)/frequency.QuadPart;phase=to==1?Phase::Opening:Phase::Closing;
        for(auto&v:views)for(auto&i:v->items){
            Box a=Mix(i.original,i.target,from),b=Mix(i.original,i.target,to);
            Property(a.x,b.x,[&](auto value){i.position->SetOffsetX(value);});
            Property(a.y,b.y,[&](auto value){i.position->SetOffsetY(value);});
            Property(a.w/i.pixels.cx,b.w/i.pixels.cx,[&](auto value){i.scale->SetScaleX(value);});
            Property(a.h/i.pixels.cy,b.h/i.pixels.cy,[&](auto value){i.scale->SetScaleY(value);});
            Property(a.w,b.w,[&](auto value){i.clip->SetRight(value);});
            Property(a.h,b.h,[&](auto value){i.clip->SetBottom(value);});
            Property((a.w+2*i.Outset())/i.borderWidth,(b.w+2*i.Outset())/i.borderWidth,[&](auto value){i.borderScale->SetScaleX(value);});
            Property((a.h+2*i.Outset())/i.borderHeight,(b.h+2*i.Outset())/i.borderHeight,[&](auto value){i.borderScale->SetScaleY(value);});
            if(i.label){
                Property((a.w-i.target.w)*.5,(b.w-i.target.w)*.5,[&](auto value){i.label->SetOffsetX(value);});
                Property(a.h+i.Outset()+6*v->dpi,b.h+i.Outset()+6*v->dpi,[&](auto value){i.label->SetOffsetY(value);});
                Property(from,to,[&](auto value){i.labelEffect->SetOpacity(value);});
            }
            double ra=CornerExtent(a.w,a.h,float(i.nativeRadius+(settings.radius*v->dpi-i.nativeRadius)*from))/kCornerTexture;
            double rb=CornerExtent(b.w,b.h,float(i.nativeRadius+(settings.radius*v->dpi-i.nativeRadius)*to))/kCornerTexture;
            for(int n=0;n<4;++n){
                auto*c=i.corners[n].Get();float sx=(n&1)?-1.f:1.f,sy=(n&2)?-1.f:1.f;
                Property(sx*ra,sx*rb,[&](auto value){c->SetTransformMatrixElement(0,0,value);});
                Property(sy*ra,sy*rb,[&](auto value){c->SetTransformMatrixElement(1,1,value);});
                if(n&1)Property(a.w,b.w,[&](auto value){c->SetTransformMatrixElement(2,0,value);});
                if(n&2)Property(a.h,b.h,[&](auto value){c->SetTransformMatrixElement(2,1,value);});
            }
        }
        HRESULT hr=device->Commit();if(FAILED(hr)){Log(L"Animate",hr);End(true);return;}
        Schedule(seconds+.045);
    }
    void FinishTransition(){
        if(phase!=Phase::Opening&&phase!=Phase::Closing)return;
        progress=to;StaticPose(to);device->Commit();
        if(phase==Phase::Closing){End(true);return;}
        phase=Phase::Settled;
    }
    static BOOL CALLBACK CollectZOrder(HWND hwnd,LPARAM p){reinterpret_cast<std::vector<HWND>*>(p)->push_back(hwnd);return TRUE;}
    void FollowNativeZOrder(){
        std::vector<HWND> native;
        EnumWindows(CollectZOrder,reinterpret_cast<LPARAM>(&native));
        for(auto&v:views)if(v->windows){
            v->windows->RemoveAllVisuals();
            for(auto hwnd=native.rbegin();hwnd!=native.rend();++hwnd)
                for(auto&i:v->items)if(i.source==*hwnd){v->windows->AddVisual(i.position.Get(),FALSE,nullptr);break;}
        }
    }
    void Close(HWND selection){
        if(phase==Phase::Hidden||phase==Phase::Closing)return;
        progress=CurrentProgress();chosen=selection;phase=Phase::Closing;
        HWND target=chosen?chosen:previous;
        // Activate FIRST, behind the compositor scene. Both real windows and
        // returning visuals then share the native order and active decoration.
        if(IsWindow(target)){
            SetForegroundWindow(target);
            // Foreground activation is asynchronous across input queues. Wait
            // for the app to process it before reading its resulting Z order.
            DWORD_PTR ignored=0;SendMessageTimeout(target,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&ignored);
        }
        // Match the current native rectangle at handoff, including app-driven
        // size changes that may have happened while the overview was open.
        for(auto&v:views)for(auto&i:v->items){RECT r{};if(Bounds(i.source,r))i.original={double(r.left-v->bounds.left),double(r.top-v->bounds.top),double(r.right-r.left),double(r.bottom-r.top)};}
        for(auto&v:views)for(auto&i:v->items)i.borderEffect->SetOpacity(0.f);
        FollowNativeZOrder();
        Animate(0);
    }
    void End(bool restore,bool idle=true){
        if(timer)CancelWaitableTimer(timer);idleTimer=false;
        bool visible=std::any_of(views.begin(),views.end(),[](const auto&v){return v->hwnd&&IsWindowVisible(v->hwnd);});
        phase=Phase::Hidden;
        if(foregroundHook){UnhookWinEvent(foregroundHook);foregroundHook=nullptr;}
        if(destroyHook){UnhookWinEvent(destroyHook);destroyHook=nullptr;}
        HWND target=chosen?chosen:previous;
        if(restore&&IsWindow(target)&&GetForegroundWindow()!=target)SetForegroundWindow(target);
        if(visible&&device){
            // Finish the existing, valid scene. Creating a new whole-desktop
            // thumbnail here can publish an uninitialized black surface.
            HRESULT hr=device->Commit();
            if(SUCCEEDED(hr))hr=device->WaitForCommitCompletion();
            if(FAILED(hr))Log(L"Finish preview before hide",hr);
        }
        // Hide every monitor before detaching ANY visual. Keep the wallpaper
        // and shared sources alive across the presentation fence, including
        // interrupted animations, display changes and unload/error paths.
        for(auto&v:views)if(v->hwnd)ShowWindow(v->hwnd,SW_HIDE);
        if(visible){HRESULT hr=DwmFlush();if(FAILED(hr))Log(L"Present hidden overlays",hr);}
        for(auto&v:views)if(v->target)v->target->SetRoot(nullptr);
        if(device){HRESULT hr=device->Commit();if(SUCCEEDED(hr))device->WaitForCommitCompletion();}
        // Shared visuals own their private thumbnail references. Release visual
        // trees and destroy destinations; don't treat private IDs as public registrations.
        for(auto&v:views){v->items.clear();v->desktop.Reset();v->wallpaperSurface.Reset();v->windows.Reset();v->root.Reset();v->target.Reset();if(v->hwnd)DestroyWindow(v->hwnd);}
        views.clear();previous=chosen=selectedSource=nullptr;progress=0;
        if(idle&&timer&&device){idleTimer=true;Schedule(15);}
    }
    int Hit(const View&v,POINT p){
        double progress=CurrentProgress();
        for(size_t n=0;n<v.items.size();++n){Box b=Mix(v.items[n].original,v.items[n].target,progress);if(p.x>=b.x&&p.x<b.right()&&p.y>=b.y&&p.y<b.bottom())return int(n);}return -1;
    }
    void Select(HWND source){
        if(phase==Phase::Closing||phase==Phase::Hidden)return;
        selectedSource=source;
        for(auto&v:views){v->selected=-1;for(size_t n=0;n<v->items.size();++n){auto&i=v->items[n];bool selected=i.source==source;if(selected)v->selected=int(n);i.borderEffect->SetOpacity(selected?1.f:0.f);}}
        device->Commit();
    }
    static std::vector<INPUT> NativeTaskViewInputs(bool leftWin,bool rightWin,bool leftShift,bool rightShift){
        std::vector<INPUT> keys;keys.reserve(8);
        auto key=[&](WORD vk,bool up){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=vk;
            i.ki.dwFlags=(up?KEYEVENTF_KEYUP:0)|((vk==VK_LWIN||vk==VK_RWIN)?KEYEVENTF_EXTENDEDKEY:0);
            i.ki.dwExtraInfo=kInputMarker;keys.push_back(i);};
        // Send plain Win+Tab to the shell, then restore the user's held Shift
        // keys. SendInput inserts the entire sequence without interleaving.
        if(leftShift)key(VK_LSHIFT,true);if(rightShift)key(VK_RSHIFT,true);
        if(!leftWin&&!rightWin)key(VK_LWIN,false);
        key(VK_TAB,false);key(VK_TAB,true);
        if(!leftWin&&!rightWin)key(VK_LWIN,true);
        if(leftShift)key(VK_LSHIFT,false);if(rightShift)key(VK_RSHIFT,false);
        return keys;
    }
    void NativeTaskView(){
        if(phase!=Phase::Hidden)End(true);
        auto keys=NativeTaskViewInputs((GetAsyncKeyState(VK_LWIN)&0x8000)!=0,(GetAsyncKeyState(VK_RWIN)&0x8000)!=0,
                                      (GetAsyncKeyState(VK_LSHIFT)&0x8000)!=0,(GetAsyncKeyState(VK_RSHIFT)&0x8000)!=0);
        if(SendInput(UINT(keys.size()),keys.data(),sizeof(INPUT))!=keys.size())Log(L"Open Windows Task View",HRESULT_FROM_WIN32(GetLastError()));
    }
    void TrayMenu(){
        HMENU menu=CreatePopupMenu();AppendMenu(menu,MF_STRING,1,L"打开窗口总览");AppendMenu(menu,MF_STRING,2,L"退出");POINT p;GetCursorPos(&p);SetForegroundWindow(controller);
        UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,p.x,p.y,0,controller,nullptr);DestroyMenu(menu);PostMessage(controller,WM_NULL,0,0);
        if(cmd==1)Begin();else if(cmd==2)SetEvent(g_stopEvent);
    }
    LRESULT Message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
        if(msg==WM_NCCREATE){auto cs=reinterpret_cast<CREATESTRUCT*>(lp);if(cs->lpCreateParams)SetWindowLongPtr(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));}
        auto v=reinterpret_cast<View*>(GetWindowLongPtr(hwnd,GWLP_USERDATA));
        switch(msg){
        case WM_HOTKEY:if(wp==1){if(phase==Phase::Hidden)Begin();else Close(nullptr);}return 0;
        case kToggle:if(phase==Phase::Hidden)Begin();else if(phase==Phase::Closing){End(true);Begin();}else Close(nullptr);return 0;
        case kNativeTaskView:NativeTaskView();return 0;
        case kSettings:End(false);LoadSettings();if(keyboard){UnhookWindowsHookEx(keyboard);keyboard=nullptr;}swallowedTab=false;if(settings.winTab)keyboard=SetWindowsHookEx(WH_KEYBOARD_LL,Keyboard,g_instance,0);return 0;
        case kDismiss:if(!building&&phase!=Phase::Hidden&&phase!=Phase::Closing&&(wp||!IsOurWindow(GetForegroundWindow())))End(false);return 0;
        case kTray:if(lp==WM_LBUTTONUP){if(phase==Phase::Hidden)Begin();else Close(nullptr);}else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)TrayMenu();return 0;
        case WM_DWMCOLORIZATIONCOLORCHANGED:case WM_THEMECHANGED:if(hwnd==controller)RefreshAccent();return 0;
        case WM_DISPLAYCHANGE:case WM_DPICHANGED:case WM_SETTINGCHANGE:if(!building&&phase!=Phase::Hidden)PostMessage(controller,kDismiss,1,0);return 0;
        case WM_QUERYENDSESSION:return TRUE;
        case WM_ENDSESSION:if(wp)SetEvent(g_stopEvent);return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;}
        case WM_MOUSEMOVE:if(v&&phase==Phase::Settled){POINT p;GetCursorPos(&p);if(p.x!=lastPointer.x||p.y!=lastPointer.y){lastPointer=p;int i=Hit(*v,{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});HWND hover=i>=0?v->items[i].source:nullptr;if(selectedSource!=hover)Select(hover);}}return 0;
        case WM_LBUTTONUP:if(v&&(phase==Phase::Settled||phase==Phase::Opening)){int i=Hit(*v,{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});Close(i<0?nullptr:v->items[i].source);}return 0;
        case WM_RBUTTONUP:Close(nullptr);return 0;
        case WM_SYSKEYDOWN:case WM_KEYDOWN:
            if(wp==VK_ESCAPE){Close(nullptr);return 0;}
            if(v&&(phase==Phase::Settled||phase==Phase::Opening)){if(wp==VK_TAB)return 0;if(wp==VK_RETURN){Close(selectedSource);return 0;}}break;
        case WM_SYSCOMMAND:if((wp&0xFFF0)==SC_KEYMENU)return 0;break;
        case WM_CLOSE:if(v)Close(nullptr);else SetEvent(g_stopEvent);return 0;
        }return DefWindowProc(hwnd,msg,wp,lp);
    }

#ifdef SCATTER_STANDALONE
    bool Probe(){
        if(!EnsureDevice())return false;
        HWND fixture=CreateWindowEx(WS_EX_TOOLWINDOW,kController,L"Scatter API probe",WS_OVERLAPPEDWINDOW,-20000,-20000,640,420,nullptr,nullptr,g_instance,nullptr);
        if(!fixture)return false;
        auto v=std::make_unique<View>();MONITORINFO mi{sizeof(mi)};v->monitor=MonitorFromWindow(fixture,MONITOR_DEFAULTTOPRIMARY);GetMonitorInfo(v->monitor,&mi);v->bounds=mi.rcWork;
        views.push_back(std::move(v));View&view=*views[0];
        bool ok=CreateView(view);Item i;i.source=fixture;i.original={10,10,640,420};i.target={180,130,320,210};
        if(ok)ok=CreateItem(view,i);
        if(ok)ok=CreateTitle(view,i);
        if(ok){view.windows->AddVisual(i.position.Get(),TRUE,nullptr);view.items.push_back(std::move(i));ok=CreateDesktop(view);Log(L"Wallpaper below preview tree",ok?S_OK:E_FAIL);}
        if(ok)view.items[0].borderEffect->SetOpacity(1.f);
        if(ok){StaticPose(0);Animate(1);HRESULT hr=device->WaitForCommitCompletion();ok=SUCCEEDED(hr);Log(L"Hidden composition probe",hr);}
        End(false,false);DestroyWindow(fixture);return ok;
    }
#endif
};
App* App::instance=nullptr;
static DWORD WINAPI ThreadMain(void*){
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);App app;App::instance=&app;
    bool ok=app.Initialize();g_ready.store(ok);SetEvent(g_readyEvent);if(ok)app.Loop();app.Cleanup();App::instance=nullptr;if(SUCCEEDED(co))CoUninitialize();return ok?0:1;
}
static bool Start(){
    g_stopEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);g_readyEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);if(!g_stopEvent||!g_readyEvent)return false;
    g_ready=false;g_thread=CreateThread(nullptr,0,ThreadMain,nullptr,0,nullptr);if(!g_thread)return false;HANDLE waits[]{g_readyEvent,g_thread};return WaitForMultipleObjects(2,waits,FALSE,10000)==WAIT_OBJECT_0&&g_ready;
}
static void Stop(){if(g_stopEvent)SetEvent(g_stopEvent);if(g_thread){WaitForSingleObject(g_thread,INFINITE);CloseHandle(g_thread);g_thread=nullptr;}if(g_readyEvent){CloseHandle(g_readyEvent);g_readyEvent=nullptr;}if(g_stopEvent){CloseHandle(g_stopEvent);g_stopEvent=nullptr;}}
} // namespace scatter

#ifndef SCATTER_STANDALONE
static void ReadSettings(){scatter::g_winTab=Wh_GetIntSetting(L"replaceWinTab")!=0;scatter::g_duration=Wh_GetIntSetting(L"durationMs");scatter::g_radius=Wh_GetIntSetting(L"cornerRadius");}
BOOL WhTool_ModInit(){GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&WhTool_ModInit),&scatter::g_instance);ReadSettings();if(!scatter::Start()){scatter::Stop();return FALSE;}return TRUE;}
void WhTool_ModSettingsChanged(){ReadSettings();if(auto hwnd=scatter::g_controller.load())PostMessage(hwnd,scatter::kSettings,0,0);}
void WhTool_ModUninit(){scatter::Stop();}
////////////////////////////////////////////////////////////////////////////////
// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. Context:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated windhawk.exe process.
//
// Paste the code below as part of the mod code, and use these callbacks:
// * WhTool_ModInit
// * WhTool_ModSettingsChanged
// * WhTool_ModUninit
//
// Currently, other callbacks are not supported.

bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex =
            CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        IMAGE_DOS_HEADER* dosHeader =
            (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* ntHeaders =
            (IMAGE_NT_HEADERS*)((BYTE*)dosHeader + dosHeader->e_lfanew);

        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = (BYTE*)dosHeader + entryPointRVA;

        Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr);
        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    WCHAR
    commandLine[MAX_PATH + 2 +
                (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath,
               WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si{
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_FORCEOFFFEEDBACK,
    };
    PROCESS_INFORMATION pi;
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}

#elif !defined(SCATTER_TEST)
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    int argc=0;LPWSTR*argv=CommandLineToArgvW(GetCommandLine(),&argc);bool open=true;
    for(int i=1;i<argc;++i){if(!wcscmp(argv[i],L"--probe"))scatter::g_probe=true;else if(!wcscmp(argv[i],L"--background"))open=false;else if(!wcscmp(argv[i],L"--report")&&i+1<argc)_wfopen_s(&scatter::g_report,argv[++i],L"w");}
    LocalFree(argv);scatter::g_instance=instance;
    if(scatter::g_probe){HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);scatter::App app;scatter::App::instance=&app;bool ok=app.Initialize()&&app.Probe();app.Cleanup();scatter::App::instance=nullptr;if(SUCCEEDED(co))CoUninitialize();if(scatter::g_report)fclose(scatter::g_report);return ok?0:1;}
    HANDLE mutex=CreateMutex(nullptr,FALSE,L"Local\\WindowScatter.UserInstance.v2");if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(HWND hwnd=FindWindow(scatter::kController,L"Window Scatter Controller"))PostMessage(hwnd,scatter::kToggle,0,0);CloseHandle(mutex);return 0;}
    bool ok=scatter::Start();if(ok){if(open)PostMessage(scatter::g_controller.load(),scatter::kToggle,0,0);WaitForSingleObject(scatter::g_thread,INFINITE);}scatter::Stop();CloseHandle(mutex);return ok?0:1;
}
#endif
