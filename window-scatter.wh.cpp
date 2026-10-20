// ==WindhawkMod==
// @id              window-scatter
// @name            Window Scatter
// @description     An independent window overview for Windows 11, with spatial layouts, live previews and touchpad navigation.
// @version         1.0.2
// @author          SinCircle
// @include         windhawk.exe
// @include         explorer.exe
// @compilerOptions -ld3d11 -ldxgi -ldcomp -ldwmapi -ld2d1 -ldwrite -lwindowscodecs -lole32 -loleaut32 -lshell32 -lgdi32 -luser32 -luuid -lruntimeobject
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Window Scatter

Window Scatter is an independently designed window overview for Windows 11.
Multitasking overviews on macOS and Linux desktops serve as interaction references.
The project develops its own spatial layout, gesture selection, continuous reflow,
and minimize/restore interactions around its design goals and Windows workflows.

Win+Tab and the system Task View touchpad gesture open this overview through
Explorer's native Task View request. No global keyboard hook is installed.
With the touchpad, a short upward swipe opens the overview and leaves it open.
Keep three fingers down and move again to select windows by their actual position,
including diagonally; lift after selecting to activate the window. A downward
swipe while the overview is open cancels.
The selected outline follows the gesture without moving the system mouse cursor.
Click a window to switch; hover a preview and click its close button to ask the
application to close it. Arrow keys select nearby previews, including minimized
windows and other monitors; Enter switches to the selection. Moving the mouse
resumes hover selection. Escape or a background click cancels. Tab does not
cycle candidates. Shift+Win+Tab opens the original Windows Task View, even when
this overview is already open. Alt+Tab retains its native behavior.
No tray icon is created. Window titles are shown, with a system-accent
selection outline with a 4 DIP transparent gap. Preview brightness is unchanged.
No instruction footer is drawn.

Ordinary windows use one common scale per monitor, preserving their relative dimensions.
Their source positions use the same scale around the available area's center,
so the placement solver does not pull smaller previews toward unscaled coordinates.
Every new opening computes a fresh layout, including an interrupted closing animation.
The initial layout compares compact stacks with mixed horizontal/vertical
subgroups and centers the complete footprint within each monitor. Later changes
preserve a feasible common scale and allow wider movement to use available space,
while existing previews retain separated motion paths. Interrupted local changes
use the current captured positions instead of their unfinished destinations. Minimized
previews have a separate bottom-center shelf and slide from below the display
with a subtle 94%-100% scale change, retaining their colors and opacity.
Size, compactness, visual balance and the original left/right
and top/bottom relationships determine the arrangement. There are no equal-sized cells.

DirectComposition renders complete live window surfaces with their native corners,
linear bitmap filtering, antialiased clip edges, and non-linear critically damped
motion. Source rectangles use visible-frame coordinates relative to the outer
window, preventing invisible resize margins from shifting or clipping previews.
Animations are submitted once; there is
no application frame loop, screenshot loop or timer-resolution change. DWM runs
at display refresh. Animation start and completion use the compositor's refresh
timing. Equal animation functions share one object within a submission.
Automatic mode keeps the GPU device ready while memory is available. Windows
low-memory notifications release rendering resources once the overview is hidden,
leaving the shell request/event listeners. Resident mode always keeps the device ready;
Memory saver skips startup prewarming and releases it after each overview.
Reopening after release recreates graphics resources. No periodic memory polling
is used when Windows memory notifications are available. Cached wallpaper surfaces
are reused per monitor; after 60 seconds of inactivity, at most 64 MiB of
wallpaper pixels remain cached. Automatic low-memory handling and Memory saver release them. Wallpaper
file timestamps and display/theme notifications invalidate stale backgrounds.
DWM supplies the live window surface and its transparency. The existing native
DirectComposition rectangle clip applies 8 DIP corners, including to maximized
and snapped previews, and removes DWM corner shadow pixels outside that shape.
No corner texture or filter-effect graph is used. The eight clip radius properties
share one compositor animation per distinct radius transition, submitted once.
The outline follows the same circular corners at a constant 4 DIP gap and tracks
the Windows accent color.
Minimized windows remain selectable using DWM's retained surface at full source
opacity, in smaller tiles with attached application titles and icons. They expand
from below the display; selecting one restores its previous normal/maximized state.
The minimize button beside close moves a supported window to the bottom row while
keeping overview open. Opening or canceling never restores minimized windows.
If no retained surface is available, one small shared placeholder supplies a card.
Only a hovered window allocates an outline; inactive outlines are detached.
Explorer's Task View request is redirected to the dedicated renderer. The system
recognizes Win+Tab and the configured three-finger upward swipe. There is no
keyboard capture or periodic hook renewal. UISettings supplies the accent color
and notifies color changes. Native activation
happens after animation submission, so a slow input queue cannot delay its start.
Reversing a closing animation keeps the current pose and computes fresh destinations.
Animated click targets follow the same front-to-back order as the previews.

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
be unavailable. Virtual-desktop management is out of scope.

Shared-visual API research: ADeltaX
https://gist.github.com/ADeltaX/aea6aac248604d0cb7d423a61b06e247
Windhawk 1.7.3 tool-process launcher: Ramen Software (copied below)
https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- replaceWinTab: true
  $name: Replace system Task View
  $description: Short swipe up opens and stays open. Keep three fingers down and move in any direction to select, then release to switch. Swipe down in the overview to cancel. Shift+Win+Tab opens Windows Task View.
- durationMs: 320
  $name: Animation duration (ms)
  $description: Non-linear motion executed by DirectComposition. 0 disables animation.
- memoryMode: auto
  $name: 内存模式 / Memory mode
  $description: 自动：内存不足时在退出总览后释放图形资源。常驻：优先响应速度。省内存：每次退出后释放，下次打开需要重新初始化。
  $options:
  - auto: 自动 / Automatic
  - resident: 常驻 / Resident
  - saver: 省内存 / Memory saver
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
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <uiautomation.h>
#include <appmodel.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.ui.viewmanagement.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <map>
#include <mutex>
#include <numeric>
#include <string>
#include <utility>
#include <array>
#include <unordered_map>
#include <vector>

#ifndef SCATTER_STANDALONE
// Shared implementation embedded in each single-file Windhawk mod.
// Cache only successful lookups, keyed by the PE/PDB identity and symbol schema.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <iterator>
#include <type_traits>

namespace StartupSupport {
struct SymbolHook {
    std::vector<std::wstring> names;
    void** original = nullptr;
    void* replacement = nullptr;
    bool optional = false;
    void* address = nullptr;
    template<class T>
    SymbolHook(std::initializer_list<std::wstring_view> symbols, T** output,
               std::type_identity_t<T*> hook = nullptr, bool allowMissing = false)
        : original(reinterpret_cast<void**>(output)),
          replacement(reinterpret_cast<void*>(hook)), optional(allowMissing) {
        for (auto name : symbols) names.emplace_back(name);
    }
};
template<size_t N>
void Append(std::vector<SymbolHook>& target, SymbolHook (&items)[N]) {
    target.insert(target.end(), std::make_move_iterator(std::begin(items)),
                  std::make_move_iterator(std::end(items)));
}
inline bool Cancelled(HANDLE stop) {
    return stop && WaitForSingleObject(stop, 0) == WAIT_OBJECT_0;
}

inline std::vector<uint64_t> CacheIdentity(HMODULE module,
        const std::vector<SymbolHook>& hooks, bool decorated) {
    const auto* base = reinterpret_cast<const BYTE*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return {};
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return {};
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    auto debug = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!debug.VirtualAddress || debug.VirtualAddress >= imageSize ||
        debug.Size > imageSize - debug.VirtualAddress) return {};
    uint64_t guid[2]{}; DWORD age = 0; bool hasPdb = false;
    for (DWORD offset = 0; offset + sizeof(IMAGE_DEBUG_DIRECTORY) <= debug.Size;
         offset += sizeof(IMAGE_DEBUG_DIRECTORY)) {
        const auto* entry = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base + debug.VirtualAddress + offset);
        if (entry->Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry->SizeOfData < 24 ||
            entry->AddressOfRawData >= imageSize ||
            entry->SizeOfData > imageSize - entry->AddressOfRawData) continue;
        const BYTE* data = base + entry->AddressOfRawData;
        if (memcmp(data, "RSDS", 4)) continue;
        memcpy(guid, data + 4, sizeof(guid)); memcpy(&age, data + 20, sizeof(age));
        hasPdb = true; break;
    }
    if (!hasPdb) return {};
    uint64_t schema = 14695981039346656037ULL;
    auto hash = [&](uint64_t value) { schema = (schema ^ value) * 1099511628211ULL; };
    hash(decorated); hash(hooks.size());
    for (const auto& hook : hooks) {
        hash(hook.optional); hash(hook.names.size());
        for (const auto& name : hook.names) { hash(name.size()); for (wchar_t ch : name) hash(ch); }
    }
    return {1, nt->FileHeader.Machine, nt->FileHeader.TimeDateStamp, imageSize,
            guid[0], guid[1], age, schema, hooks.size()};
}

// Resolve all entries before registering any hook. A failed download does not
// leave partial hooks or Windhawk's hours-long negative symbol-cache entry.
inline bool Resolve(HMODULE module, PCWSTR cacheKey, std::vector<SymbolHook>& hooks,
                    bool decorated, HANDLE stop) {
    if (hooks.empty()) return true;
    if (!module || Cancelled(stop)) return false;
    for (auto& hook : hooks) hook.address = nullptr;
    const auto identity = CacheIdentity(module, hooks, decorated);
    const auto base = reinterpret_cast<uintptr_t>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    if (!identity.empty()) {
        std::vector<uint64_t> cached(identity.size() + hooks.size());
        const size_t bytes = cached.size() * sizeof(uint64_t);
        if (Wh_GetBinaryValue(cacheKey, cached.data(), bytes) == bytes &&
            std::equal(identity.begin(), identity.end(), cached.begin())) {
            bool valid = true;
            for (size_t i = 0; i < hooks.size(); ++i) {
                const auto rva = cached[identity.size() + i];
                if (rva >= imageSize || (!rva && !hooks[i].optional)) { valid = false; break; }
            }
            if (valid) {
                for (size_t i = 0; i < hooks.size(); ++i) {
                    auto rva = cached[identity.size() + i];
                    hooks[i].address = rva ? reinterpret_cast<void*>(base + rva) : nullptr;
                }
                Wh_Log(L"Startup symbol cache hit: %s (%zu entries)", cacheKey, hooks.size());
                return !Cancelled(stop);
            }
        }
    }
    WH_FIND_SYMBOL symbol{};
    // First try the on-disk PDB without contacting a symbol server.
    WH_FIND_SYMBOL_OPTIONS options{sizeof(options), L"", decorated};
    HANDLE search = Wh_FindFirstSymbol(module, &options, &symbol);
    if (!search && !Cancelled(stop)) {
        options.symbolServer = nullptr;
        search = Wh_FindFirstSymbol(module, &options, &symbol);
    }
    if (!search) return false;
    size_t remaining = hooks.size();
    do {
        if (Cancelled(stop)) break;
        PCWSTR name = decorated ? symbol.symbolDecorated : symbol.symbol;
        if (!name) continue;
        for (auto& hook : hooks) {
            if (hook.address || std::find(hook.names.begin(), hook.names.end(), name) == hook.names.end()) continue;
            auto address = reinterpret_cast<uintptr_t>(symbol.address);
            if (address <= base || address - base >= imageSize) continue;
            hook.address = symbol.address; --remaining;
        }
        if (!remaining) break;
    } while (Wh_FindNextSymbol(search, &symbol));
    Wh_FindCloseSymbol(search);
    if (Cancelled(stop)) return false;
    for (const auto& hook : hooks) if (!hook.address && !hook.optional) {
        Wh_Log(L"Startup symbol not available yet: %s", hook.names.front().c_str());
        return false;
    }
    if (!identity.empty()) {
        auto record = identity;
        for (const auto& hook : hooks) record.push_back(hook.address ? reinterpret_cast<uintptr_t>(hook.address) - base : 0);
        Wh_SetBinaryValue(cacheKey, record.data(), record.size() * sizeof(uint64_t));
    }
    return true;
}

inline bool Install(std::vector<SymbolHook>& hooks, HANDLE stop) {
    std::vector<void*> queued;
    auto rollback = [&] {
        for (void* address : queued) Wh_RemoveFunctionHook(address);
        if (!queued.empty()) Wh_ApplyHookOperations();
    };
    for (auto& hook : hooks) {
        if (Cancelled(stop)) { rollback(); return false; }
        if (!hook.address) { if (hook.original) *hook.original = nullptr; continue; }
        if (!hook.replacement) { if (hook.original) *hook.original = hook.address; continue; }
        if (!Wh_SetFunctionHook(hook.address, hook.replacement, hook.original)) {
            rollback(); return false;
        }
        queued.push_back(hook.address);
    }
    if (Cancelled(stop) || (!queued.empty() && !Wh_ApplyHookOperations())) { rollback(); return false; }
    return true;
}

enum class Result { Waiting, Retry, Ready };
class Worker {
    HANDLE stop_ = nullptr, begin_ = nullptr, thread_ = nullptr;
    Result (*attempt_)(HANDLE) = nullptr;
    PCWSTR prefix_ = nullptr;
    static DWORD WINAPI Entry(void* data) { return static_cast<Worker*>(data)->Run(); }
    DWORD Run() {
        HANDLE events[]{stop_, begin_};
        if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) return 0;
        HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ULONGLONG started = GetTickCount64(); unsigned failures = 0, attempts = 0;
        int lastState = -1;
        const std::wstring stateKey = std::wstring(prefix_) + L"-state";
        Wh_SetIntValue((std::wstring(prefix_) + L"-pid").c_str(), GetCurrentProcessId());
        while (!Cancelled(stop_)) {
            Result result = Result::Retry;
            try { result = attempt_(stop_); } catch (...) { Wh_Log(L"Deferred startup failed; retry scheduled"); }
            if (Cancelled(stop_)) break;
            if (result != Result::Waiting) ++attempts;
            if (lastState != int(result)) {
                Wh_SetStringValue(stateKey.c_str(), result == Result::Ready ? L"ready" :
                                  result == Result::Waiting ? L"waiting-for-shell" : L"retrying");
                lastState = int(result);
            }
            if (result == Result::Ready) {
                Wh_SetIntValue((std::wstring(prefix_) + L"-elapsed-ms").c_str(),
                    static_cast<int>(std::min<ULONGLONG>(GetTickCount64() - started, INT_MAX)));
                Wh_SetIntValue((std::wstring(prefix_) + L"-attempts").c_str(), attempts);
                break;
            }
            DWORD delay = result == Result::Waiting ? 200 :
                std::min<DWORD>(60000, 1000u << std::min<unsigned>(failures++, 6));
            if (WaitForSingleObject(stop_, delay) != WAIT_TIMEOUT) break;
        }
        if (SUCCEEDED(co)) CoUninitialize();
        return 0;
    }
public:
    bool Prepare(Result (*attempt)(HANDLE), PCWSTR prefix) {
        attempt_ = attempt; prefix_ = prefix;
        stop_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        begin_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (stop_ && begin_) thread_ = CreateThread(nullptr, 0, Entry, this, 0, nullptr);
        if (!thread_) { Stop(); return false; }
        return true;
    }
    void Begin() { if (begin_) SetEvent(begin_); }
    void Stop() {
        if (stop_) SetEvent(stop_);
        if (thread_) { WaitForSingleObject(thread_, INFINITE); CloseHandle(thread_); thread_ = nullptr; }
        if (begin_) { CloseHandle(begin_); begin_ = nullptr; }
        if (stop_) { CloseHandle(stop_); stop_ = nullptr; }
    }
};
inline HWND OwnedTaskbar() {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr); DWORD process = 0;
    return taskbar && GetWindowThreadProcessId(taskbar, &process) &&
        process == GetCurrentProcessId() ? taskbar : nullptr;
}
} // namespace StartupSupport

#endif

namespace scatter {
using Microsoft::WRL::ComPtr;
constexpr wchar_t kController[]=L"WindowScatter.Controller.v2";
constexpr wchar_t kOverlay[]=L"WindowScatter.Overview.v2";
constexpr UINT kToggle=WM_APP+1,kSettings=WM_APP+2,kDismiss=WM_APP+3,kNativeTaskView=WM_APP+5,kWarmup=WM_APP+6,kSyncOrder=WM_APP+7,kReleaseInactive=WM_APP+8,kShellTaskView=WM_APP+10,kAccentChanged=WM_APP+11,kWindowGone=WM_APP+12,kNativeConsumed=WM_APP+13,kShow=WM_APP+14;
constexpr UINT kGestureBegin=WM_APP+15,kGestureUpdate=WM_APP+16,kGestureEnd=WM_APP+17,kGestureCancel=WM_APP+18;
constexpr UINT kWindowStateChanged=WM_APP+19,kTaskbarOrder=WM_APP+20,kSceneChanged=WM_APP+21;
constexpr ULONG_PTR kInputMarker=0x57534354;
constexpr wchar_t kShellReadyProperty[]=L"WindowScatter.ShellReady.v4";
constexpr wchar_t kNativeBypassProperty[]=L"WindowScatter.NativeTaskView.v4";
constexpr wchar_t kOverviewProperty[]=L"WindowScatter.OverviewReady.v5";
constexpr wchar_t kGestureProperty[]=L"WindowScatter.GestureToken.v5";
constexpr ULONG_PTR kShellAck=0x57534334;
struct Box {
    double x=0,y=0,w=0,h=0;
    double right()const{return x+w;} double bottom()const{return y+h;}
    double cx()const{return x+w*.5;} double cy()const{return y+h*.5;}
};
static bool Overlap(const Box&a,const Box&b){return a.x<b.right()-.01&&a.right()>b.x+.01&&a.y<b.bottom()-.01&&a.bottom()>b.y+.01;}
static bool Contains(const Box&a,const Box&b){return b.x>=a.x-.01&&b.y>=a.y-.01&&b.right()<=a.right()+.01&&b.bottom()<=a.bottom()+.01;}
static Box Mix(const Box&a,const Box&b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.w+(b.w-a.w)*t,a.h+(b.h-a.h)*t};}
// Scale source positions and dimensions in the same coordinate system. Scaling
// around the available area's center preserves translations between monitors.
static Box ScaledSource(const Box&source,Box area,double scale){
    return {area.cx()+(source.x-area.cx())*scale,area.cy()+(source.y-area.cy())*scale,source.w*scale,source.h*scale};
}

// Packing candidates establish a feasible common size. Final positions are
// optimized independently; history stores CPU geometry, never grid membership.
using LayoutStacks=std::vector<std::vector<int>>;
struct LayoutSlot {int group=-1,order=0,count=0;bool columns=false;double offset=0;Box original,area;bool mixed=false;std::string path;};
// Source geometry is constant across packing candidates. Cache its directional
// weights once and abandon a candidate only when its proven score bound loses.
struct SourceRelationCache {
    struct Axis {int a,b;bool vertical;double sign,weight,separation;};
    std::vector<Axis> axes;double weights=0;
    SourceRelationCache(const std::vector<Box>&src,Box area){
        for(size_t a=0;a<src.size();++a)for(size_t b=0;b<a;++b){
            double x=(src[a].cx()-src[b].cx())/area.w,y=(src[a].cy()-src[b].cy())/area.h;
            auto add=[&](double before,double other,bool vertical){double weight=std::clamp((std::abs(before)-.025)/.25,0.,1.);if(weight<=0)return;
                if(std::abs(before)*1.25<std::abs(other)){if(std::abs(before)<.08)return;weight*=.15;}
                axes.push_back({int(a),int(b),vertical,before>0?1.:-1.,weight,std::abs(before)*.5});weights+=weight;};
            add(x,y,false);add(y,x,true);
        }
    }
    double Cost(const std::vector<Box>&boxes,Box area,double scale,double limit)const{
        std::vector<double> x(boxes.size()),y(boxes.size());for(size_t i=0;i<boxes.size();++i){x[i]=boxes[i].cx();y[i]=boxes[i].cy();}
        double cost=0,denominator=std::max(1.,weights),maximum=limit*denominator;
        for(const auto&a:axes){double separation=std::min(.12,a.separation*scale);
            double after=a.sign*(a.vertical?(y[a.a]-y[a.b])/area.h:(x[a.a]-x[a.b])/area.w);
            cost+=a.weight*std::clamp((separation-after)/std::max(.001,separation),0.,2.);
            if(cost>maximum)return cost/denominator;
        }
        return cost/denominator;
    }
};

static double SourceRelationCost(const std::vector<Box>&src,const std::vector<Box>&boxes,Box area,double scale);
static double LayoutQuality(const std::vector<Box>&src,const std::vector<Box>&boxes,Box area,double caption,double cutoff=-1e100,const SourceRelationCache*relations=nullptr){
    if(boxes.empty())return -1e100;
    double left=1e100,top=1e100,right=-1e100,bottom=-1e100,occupied=0,movement=0,spread=0,scale=.85;int count=0;
    for(size_t i=0;i<boxes.size();++i)if(boxes[i].w>0)scale=std::min(scale,boxes[i].w/src[i].w);
    for(size_t i=0;i<boxes.size();++i){auto b=boxes[i];if(b.w<=0)continue;++count;
        left=std::min(left,b.x);right=std::max(right,b.right());top=std::min(top,b.y);bottom=std::max(bottom,b.bottom()+caption);occupied+=b.w*(b.h+caption);
        auto target=ScaledSource(src[i],area,scale);
        double dx=(b.cx()-std::clamp(target.cx(),area.x,area.right()))/area.w,dy=(b.cy()-std::clamp(target.cy(),area.y,area.bottom()))/area.h;movement+=dx*dx+dy*dy;
        double cx=(b.cx()-area.cx())/area.w,cy=(b.cy()+caption*.5-area.cy())/area.h;spread+=b.w*(b.h+caption)*(cx*cx+cy*cy);}
    if(!count||scale<=0)return -1e100;
    double waste=1-occupied/std::max(1.,(right-left)*(bottom-top));
    double upper=std::log(scale)-.18*waste-.5*spread/occupied-.06*movement/count;
    if(upper<=cutoff+1e-10)return -1e100;
    return upper-1.4*(relations?relations->Cost(boxes,area,scale,(upper-cutoff)/1.4):SourceRelationCost(src,boxes,area,scale));
}
static std::vector<LayoutSlot> SaveSlots(const std::vector<Box>&src,const std::vector<Box>&boxes,const LayoutStacks&stacks,bool columns,Box area){
    std::vector<LayoutSlot> slots(src.size());
    for(size_t g=0;g<stacks.size();++g){double low=1e100,high=-1e100;
        for(int id:stacks[g]){low=std::min(low,columns?boxes[id].x:boxes[id].y);high=std::max(high,columns?boxes[id].right():boxes[id].bottom());}
        for(size_t n=0;n<stacks[g].size();++n){int id=stacks[g][n];auto b=boxes[id];double slack=high-low-(columns?b.w:b.h);
            double offset=slack>1e-6?((columns?b.cx():b.cy())-(low+high)*.5)/slack:0;
            slots[id]={int(g),int(n),int(src.size()),columns,offset,src[id],area};}
    }
    return slots;
}
// Compare compact, centered stacks in both orientations. Each stack sizes itself
// to its actual windows; there are no fixed cells or per-window scale changes.
// Closed-form fit avoids binary-searching a greedy packing feasibility test.
static std::vector<Box> StackLayout(const std::vector<Box>&src,Box area,double gap,double caption=0,std::vector<LayoutSlot>*slots=nullptr) {
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
    double bestScore=-std::numeric_limits<double>::infinity();std::vector<Box> best;LayoutStacks bestStacks;bool bestColumns=false;
    std::unique_ptr<SourceRelationCache> relations;if(count>24)relations=std::make_unique<SourceRelationCache>(src,area);
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
        double score=LayoutQuality(src,attempt,area,caption,bestScore,relations.get());
        if(score>bestScore+1e-10){bestScore=score;bestStacks=stack;bestColumns=columns;best=std::move(attempt);}
    }
    if(!best.empty()&&slots)*slots=SaveSlots(src,best,bestStacks,bestColumns,area);
    return best;
}

struct StackPacking{std::vector<Box> boxes;double scale=0,waste=1;};
static StackPacking PackStacks(const std::vector<Box>&src,const LayoutStacks&stacks,bool columns,const std::vector<LayoutSlot>&slots,Box area,double gap,double caption){
    if(src.empty()||stacks.empty())return {};
    double scale=.85,totalCross=0;std::vector<double> along,cross;
    for(const auto&group:stacks){
        if(group.empty())return {};double length=0,width=0;
        for(int id:group){if(src[id].w<=0||src[id].h<=0)return {};length+=columns?src[id].h:src[id].w;width=std::max(width,columns?src[id].w:src[id].h);}
        scale=std::min(scale,((columns?area.h:area.w)-gap*(group.size()-1)-(columns?caption*group.size():0))/length);
        along.push_back(length);cross.push_back(width);totalCross+=width;
    }
    scale=std::min(scale,((columns?area.w:area.h)-gap*(stacks.size()-1)-(columns?0:caption*stacks.size()))/totalCross);
    if(scale<=0)return {};
    double across=totalCross*scale+gap*(stacks.size()-1)+(columns?0:caption*stacks.size()),longest=0,occupied=0;
    double cursor=(columns?area.cx():area.cy())-across*.5;StackPacking result;result.scale=scale;result.boxes.resize(src.size());
    for(size_t g=0;g<stacks.size();++g){
        double length=along[g]*scale+gap*(stacks[g].size()-1)+(columns?caption*stacks[g].size():0);
        double width=cross[g]*scale+(columns?0:caption),next=(columns?area.cy():area.cx())-length*.5;longest=std::max(longest,length);
        for(int id:stacks[g]){double w=src[id].w*scale,h=src[id].h*scale;
            double slack=columns?width-w:width-h-caption,offset=id<int(slots.size())?slots[id].offset:0;
            result.boxes[id]={columns?cursor+slack*(.5+offset):next,columns?next:cursor+slack*(.5+offset),w,h};
            next+=(columns?h+caption:w)+gap;occupied+=w*(h+caption);
        }
        cursor+=width+gap;
    }
    result.waste=1-occupied/std::max(1.,across*longest);return result;
}
// If a pair keeps one separating direction at both endpoints, linear motion
// keeps it separated for the entire animation, not only at sampled frames.
static bool ClearReflow(const std::vector<Box>&before,const std::vector<Box>&after,double caption){
    for(size_t a=0;a<before.size();++a)if(before[a].w>0)for(size_t b=0;b<a;++b)if(before[b].w>0){
        auto separated=[&](const Box&x,const Box&y,const Box&u,const Box&v){
            return (x.right()<=y.x+.01&&u.right()<=v.x+.01)||(x.bottom()+caption<=y.y+.01&&u.bottom()+caption<=v.y+.01);};
        if(!separated(before[a],before[b],after[a],after[b])&&!separated(before[b],before[a],after[b],after[a]))return false;
    }
    return true;
}
static double SourceRelationCost(const std::vector<Box>&src,const std::vector<Box>&boxes,Box area,double scale){
    double cost=0,weights=0;
    for(size_t a=0;a<src.size();++a)if(boxes[a].w>0)for(size_t b=0;b<a;++b)if(boxes[b].w>0){
        double ox=(src[a].cx()-src[b].cx())/area.w,oy=(src[a].cy()-src[b].cy())/area.h;
        auto axis=[&](double before,double after,double other){
            double weight=std::clamp((std::abs(before)-.025)/.25,0.,1.);if(weight<=0)return;
            // A small secondary offset must not force another row or column.
            // The dominant direction remains the strongest spatial constraint.
            if(std::abs(before)*1.25<std::abs(other)){if(std::abs(before)<.08)return;weight*=.15;}
            double separation=std::min(.12,std::abs(before)*scale*.5),signedAfter=before>0?after:-after;
            cost+=weight*std::clamp((separation-signedAfter)/std::max(.001,separation),0.,2.);weights+=weight;
        };
        axis(ox,(boxes[a].cx()-boxes[b].cx())/area.w,oy);
        axis(oy,(boxes[a].cy()-boxes[b].cy())/area.h,ox);
    }
    return cost/std::max(1.,weights);
}
struct LayoutTree;
using Tree=std::shared_ptr<const LayoutTree>;
struct LayoutTree {int item=-1;bool across=false;Tree first,second;};
static Tree Leaf(int item){return std::make_shared<LayoutTree>(LayoutTree{item});}
static Tree Join(bool across,Tree first,Tree second){
    if(!first)return second;if(!second)return first;
    return std::make_shared<LayoutTree>(LayoutTree{-1,across,std::move(first),std::move(second)});
}
static Box TreeSize(const Tree&t,const std::vector<Box>&src,double scale,double gap,double caption){
    if(t->item>=0){auto b=src[t->item];return {0,0,b.w*scale,b.h*scale+caption};}
    auto a=TreeSize(t->first,src,scale,gap,caption),b=TreeSize(t->second,src,scale,gap,caption);
    return {0,0,t->across?a.w+b.w+gap:std::max(a.w,b.w),t->across?std::max(a.h,b.h):a.h+b.h+gap};
}
static void PlaceTree(const Tree&t,const std::vector<Box>&src,double scale,double gap,double caption,double x,double y,std::vector<Box>&boxes){
    if(t->item>=0){auto b=src[t->item];boxes[t->item]={x,y,b.w*scale,b.h*scale};return;}
    auto a=TreeSize(t->first,src,scale,gap,caption),b=TreeSize(t->second,src,scale,gap,caption);
    if(t->across){double h=std::max(a.h,b.h);PlaceTree(t->first,src,scale,gap,caption,x,y+(h-a.h)*.5,boxes);PlaceTree(t->second,src,scale,gap,caption,x+a.w+gap,y+(h-b.h)*.5,boxes);}
    else{double w=std::max(a.w,b.w);PlaceTree(t->first,src,scale,gap,caption,x+(w-a.w)*.5,y,boxes);PlaceTree(t->second,src,scale,gap,caption,x+(w-b.w)*.5,y+a.h+gap,boxes);}
}
static StackPacking PackTree(const Tree&t,const std::vector<Box>&src,Box area,double gap,double caption){
    if(!t)return {};double low=0,high=.85;
    for(int step=0;step<28;++step){double scale=(low+high)*.5;auto size=TreeSize(t,src,scale,gap,caption);if(size.w<=area.w&&size.h<=area.h)low=scale;else high=scale;}
    if(low<=1e-8)return {};auto size=TreeSize(t,src,low,gap,caption);
    StackPacking pack;pack.scale=low;pack.boxes.resize(src.size());
    // Center the completed footprint once. No preview is attracted to a
    // screen-wide center line independently of the rest of its group.
    PlaceTree(t,src,low,gap,caption,area.cx()-size.w*.5,area.cy()-size.h*.5,pack.boxes);
    double occupied=0;for(auto b:pack.boxes)occupied+=b.w*(b.h+caption);pack.waste=1-occupied/std::max(1.,size.w*size.h);return pack;
}
static void SaveTreePaths(const Tree&t,std::string path,std::vector<LayoutSlot>&slots){
    if(t->item>=0){slots[t->item].path=std::move(path);return;}
    SaveTreePaths(t->first,path+(t->across?'l':'t'),slots);SaveTreePaths(t->second,path+(t->across?'r':'b'),slots);
}
static std::vector<LayoutSlot> SaveTree(const Tree&t,const std::vector<Box>&src,Box area){
    std::vector<LayoutSlot> slots(src.size());for(size_t i=0;i<src.size();++i)slots[i]={0,int(i),int(src.size()),false,0,src[i],area,true,{}};
    SaveTreePaths(t,{},slots);return slots;
}
// Bounded spatial beam search. Every subgroup can split on either axis, and
// candidates with distinct footprints survive for use in larger subgroups.
// Large scenes retain the O(n)-storage stack solver rather than an exponential
// subset search. This code runs only after a scene change, never per frame.
static std::vector<Tree> MixedCandidates(const std::vector<Box>&src,Box area,double gap,double caption){
    if(src.empty()||src.size()>10)return {};
    struct Candidate{Tree tree;Box size;std::vector<Box> boxes;double score=0;};
    const int count=int(src.size());std::map<unsigned,std::vector<Candidate>> memo;
    double sourceArea=0;for(auto b:src)sourceArea+=b.w*b.h;
    const double scale=std::min(.85,std::sqrt(area.w*area.h/sourceArea)*.7);
    auto search=[&](auto&&self,unsigned mask)->const std::vector<Candidate>&{
        if(auto found=memo.find(mask);found!=memo.end())return found->second;
        std::vector<int> ids;for(int i=0;i<count;++i)if(mask&(1u<<i))ids.push_back(i);
        std::vector<Candidate> all;
        if(ids.size()==1){int i=ids[0];Candidate c{Leaf(i),{0,0,src[i].w*scale,src[i].h*scale+caption},std::vector<Box>(count)};c.boxes[i]={0,0,c.size.w,c.size.h-caption};all.push_back(std::move(c));}
        else for(bool across:{true,false}){
            auto order=ids;std::stable_sort(order.begin(),order.end(),[&](int a,int b){double x=across?src[a].cx():src[a].cy(),y=across?src[b].cx():src[b].cy();
                if(x!=y)return x<y;double u=across?src[a].cy():src[a].cx(),v=across?src[b].cy():src[b].cx();if(u!=v)return u<v;
                if(src[a].w!=src[b].w)return src[a].w>src[b].w;if(src[a].h!=src[b].h)return src[a].h>src[b].h;return a<b;});
            unsigned left=0;for(size_t split=1;split<order.size();++split){left|=1u<<order[split-1];unsigned right=mask^left;
                const auto&first=self(self,left);const auto&second=self(self,right);
                for(const auto&a:first)for(const auto&b:second){
                    Candidate c{Join(across,a.tree,b.tree),{0,0,across?a.size.w+b.size.w+gap:std::max(a.size.w,b.size.w),across?std::max(a.size.h,b.size.h):a.size.h+b.size.h+gap},std::vector<Box>(count)};
                    double occupied=0,spread=0;for(int i:ids){bool inLeft=(left&(1u<<i))!=0;auto box=inLeft?a.boxes[i]:b.boxes[i];auto own=inLeft?a.size:b.size;
                        box.x+=across?(inLeft?0:a.size.w+gap):(c.size.w-own.w)*.5;box.y+=across?(c.size.h-own.h)*.5:(inLeft?0:a.size.h+gap);c.boxes[i]=box;occupied+=box.w*(box.h+caption);}
                    for(int i:ids){auto b=c.boxes[i];double dx=(b.cx()-c.size.w*.5)/area.w,dy=(b.cy()+caption*.5-c.size.h*.5)/area.h;spread+=b.w*(b.h+caption)*(dx*dx+dy*dy);}
                    double fit=std::min(area.w/c.size.w,area.h/c.size.h),waste=1-occupied/(c.size.w*c.size.h);
                    c.score=std::log(fit)-.18*waste-.5*spread/occupied-1.4*SourceRelationCost(src,c.boxes,area,scale);all.push_back(std::move(c));
                }
            }
        }
        std::stable_sort(all.begin(),all.end(),[](const Candidate&a,const Candidate&b){return a.score>b.score;});
        std::vector<Candidate> kept;std::vector<std::pair<int,int>> shapes;
        for(auto&c:all){auto shape=std::pair{int(c.size.w/std::max(1.,area.w*.012)),int(c.size.h/std::max(1.,area.h*.012))};
            if(std::find(shapes.begin(),shapes.end(),shape)!=shapes.end())continue;shapes.push_back(shape);kept.push_back(std::move(c));if(kept.size()>=size_t(count<=6?32:count<=8?6:3))break;}
        return memo.emplace(mask,std::move(kept)).first->second;
    };
    std::vector<Tree> result;for(const auto&c:search(search,(1u<<count)-1))result.push_back(c.tree);return result;
}
static std::vector<Box> PackedLayout(const std::vector<Box>&src,Box area,double gap,double caption=0,std::vector<LayoutSlot>*slots=nullptr){
    auto best=StackLayout(src,area,gap,caption,slots);if(best.empty())return best;
    gap=std::min(gap,std::sqrt(area.w*area.h/src.size())*.10);
    double bestScore=LayoutQuality(src,best,area,caption);Tree winner;
    for(const auto&t:MixedCandidates(src,area,gap,caption)){auto candidate=PackTree(t,src,area,gap,caption);double score=LayoutQuality(src,candidate.boxes,area,caption);
        if(score>bestScore+1e-8){bestScore=score;best=std::move(candidate.boxes);winner=t;}}
    if(winner&&slots)*slots=SaveTree(winner,src,area);return best;
}
// For a fixed common scale, a separation chooses which side of another window
// to use, not a row or column. The remaining coordinates solve a weighted
// convex projection. Window area is its resistance to leaving the scaled center.
struct PositionEdge {int first,second;double distance;};
// Retain the same feasible region while removing constraints already implied
// by a longer path. Cyclic graphs are left to the existing infeasibility check.
static void ReducePositionEdges(int count,std::vector<PositionEdge>&edges){
    if(count<=24||edges.size()<size_t(count*2))return;
    std::vector<std::vector<size_t>> outgoing(count);std::vector<int> degree(count),order;
    for(size_t k=0;k<edges.size();++k){outgoing[edges[k].first].push_back(k);++degree[edges[k].second];}
    for(int i=0;i<count;++i)if(!degree[i])order.push_back(i);
    for(size_t n=0;n<order.size();++n)for(size_t k:outgoing[order[n]])if(!--degree[edges[k].second])order.push_back(edges[k].second);
    if(order.size()!=size_t(count))return;
    std::vector<int> rank(count);for(int n=0;n<count;++n)rank[order[n]]=n;
    std::vector<bool> keep(edges.size(),true);std::vector<double> distance(count);
    for(int sourceRank=count-1;sourceRank>=0;--sourceRank){int source=order[sourceRank];
        std::fill(distance.begin(),distance.end(),-1e100);
        auto direct=outgoing[source];std::sort(direct.begin(),direct.end(),[&](size_t a,size_t b){return rank[edges[a].second]<rank[edges[b].second];});
        size_t next=0;
        for(int n=rank[source]+1;n<count;++n){int node=order[n];
            while(next<direct.size()&&edges[direct[next]].second==node){auto k=direct[next++];if(distance[node]>=edges[k].distance-1e-9)keep[k]=false;else distance[node]=edges[k].distance;}
            if(distance[node]<=-1e99)continue;
            for(size_t k:outgoing[node])distance[edges[k].second]=std::max(distance[edges[k].second],distance[node]+edges[k].distance);
        }
        std::erase_if(outgoing[source],[&](size_t k){return !keep[k];});
    }
    size_t write=0;for(size_t k=0;k<edges.size();++k)if(keep[k])edges[write++]=edges[k];edges.resize(write);
}

struct PositionSeparation {int first,second;bool vertical;};
struct PositionFit {std::vector<Box> boxes;double cost=1e100;};
static bool ProjectPositions(const std::vector<double>&target,const std::vector<double>&weight,
        std::vector<double> low,std::vector<double> high,const std::vector<PositionEdge>&edges,std::vector<double>&position){
    const int count=int(target.size());
    // Propagate earliest/latest positions first. Positive cycles and overfull
    // chains are rejected without spending time on the quadratic solver.
    for(int pass=0;pass<count;++pass){bool changed=false;
        for(const auto&e:edges){double a=low[e.first]+e.distance,b=high[e.second]-e.distance;
            if(a>low[e.second]+1e-8){low[e.second]=a;changed=true;}
            if(b<high[e.first]-1e-8){high[e.first]=b;changed=true;}}
        for(int i=0;i<count;++i)if(low[i]>high[i]+1e-6)return false;
        if(!changed)break;if(pass==count-1)return false;
    }
    position=target;std::vector<double> inverse(count),lower(count),upper(count),dual(edges.size());
    for(int i=0;i<count;++i)inverse[i]=1/std::max(1e-6,weight[i]);
    // Hildreth dual coordinate projection, including bounds as constraints.
    // All corrections use inverse area, so small windows yield more readily.
    bool settled=false;
    for(int pass=0;pass<512;++pass){double movement=0;
        for(int i=0;i<count;++i){
            double next=std::max(0.,lower[i]+(low[i]-position[i])/inverse[i]),delta=next-lower[i];lower[i]=next;position[i]+=delta*inverse[i];movement=std::max(movement,std::abs(delta*inverse[i]));
            next=std::max(0.,upper[i]+(position[i]-high[i])/inverse[i]);delta=next-upper[i];upper[i]=next;position[i]-=delta*inverse[i];movement=std::max(movement,std::abs(delta*inverse[i]));
        }
        for(size_t k=0;k<edges.size();++k){const auto&e=edges[k];double next=std::max(0.,dual[k]+(position[e.first]+e.distance-position[e.second])/(inverse[e.first]+inverse[e.second]));
            double delta=next-dual[k];dual[k]=next;position[e.first]-=delta*inverse[e.first];position[e.second]+=delta*inverse[e.second];
            movement=std::max(movement,std::abs(delta)*std::max(inverse[e.first],inverse[e.second]));}
        if(movement<.00001){settled=true;break;}
    }
    if(!settled)return false;
    for(int i=0;i<count;++i)if(position[i]<low[i]-.001||position[i]>high[i]+.001)return false;
    for(const auto&e:edges)if(position[e.second]<position[e.first]+e.distance-.001)return false;
    return true;
}
static unsigned SeparationDirections(const Box&a,const Box&b,double gap,double caption){
    unsigned result=0;
    if(a.right()+gap<=b.x+.001)result|=1;
    if(b.right()+gap<=a.x+.001)result|=2;
    if(a.bottom()+caption+gap<=b.y+.001)result|=4;
    if(b.bottom()+caption+gap<=a.y+.001)result|=8;
    return result;
}
static PositionFit FreePositions(const std::vector<Box>&src,const std::vector<Box>&seed,Box area,double gap,double caption,
        double scale,const std::vector<Box>&anchors={},double history=0){
    const int count=int(src.size());if(!count||seed.size()!=src.size()||scale<=0)return {};
    std::vector<double> weight(count),tx(count),ty(count),lx(count),ly(count),hx(count),hy(count);
    double largest=0;for(auto b:src)largest=std::max(largest,b.w*b.h);
    for(int i=0;i<count;++i){double mass=std::max(.005,src[i].w*src[i].h/largest);
        bool retained=anchors.size()==src.size()&&anchors[i].w>0;
        // A maximized (or larger) source covers the available work area. Its
        // previous preview still preserves separating motion paths below,
        // but an obsolete offset must not pull it away from its scaled center.
        bool fullArea=retained&&Contains(src[i],area);
        double past=retained&&!fullArea?history:0;
        // The tiny seed term breaks ties between identical maximized windows;
        // it does not constrain the independently solved coordinates.
        double tie=fullArea?0:.002;weight[i]=mass*(1+past)+tie;
        auto target=ScaledSource(src[i],area,scale);
        tx[i]=(mass*target.cx()+mass*past*(past?anchors[i].cx():0)+tie*seed[i].cx())/weight[i];
        ty[i]=(mass*target.cy()+mass*past*(past?anchors[i].cy():0)+tie*seed[i].cy())/weight[i];
        lx[i]=area.x+src[i].w*scale*.5;hx[i]=area.right()-src[i].w*scale*.5;
        ly[i]=area.y+src[i].h*scale*.5;hy[i]=area.bottom()-caption-src[i].h*scale*.5;
        if(lx[i]>hx[i]||ly[i]>hy[i])return {};
    }
    struct Pair {int first,second;unsigned allowed;};std::vector<Pair> pairs;
    std::vector<PositionEdge> sourceX,sourceY;
    for(int i=0;i<count;++i)for(int j=i+1;j<count;++j){
        bool retained=anchors.size()==src.size()&&anchors[i].w>0&&anchors[j].w>0;
        unsigned allowed=retained?SeparationDirections(anchors[i],anchors[j],0,caption):15u;
        // Identical physical windows are interchangeable before live reflow.
        // Order them by normalized diagonal position in the seed: every
        // arrangement has such a labeling, so this removes duplicate window
        // permutations without restricting its occupied shape or coordinates.
        if(!retained&&std::abs(src[i].x-src[j].x)<.01&&std::abs(src[i].y-src[j].y)<.01&&std::abs(src[i].w-src[j].w)<.01&&std::abs(src[i].h-src[j].h)<.01){
            double dx=src[i].w*scale+gap,dy=src[i].h*scale+caption+gap;
            double first=seed[i].cx()/dx+seed[i].cy()/dy,second=seed[j].cx()/dx+seed[j].cy()/dy;
            allowed&=first<=second?5u:10u;
        }
        if(!allowed)return {};pairs.push_back({i,j,allowed});
        // Preserve a clearly dominant physical direction without imposing an
        // alignment or requiring separation along that particular axis.
        double dx=(src[j].cx()-src[i].cx())/area.w,dy=(src[j].cy()-src[i].cy())/area.h;
        if(!retained){
            if(std::abs(dx)>.08&&std::abs(dx)>1.25*std::abs(dy))sourceX.push_back({dx>0?i:j,dx>0?j:i,.001});
            else if(std::abs(dy)>.08&&std::abs(dy)>1.25*std::abs(dx))sourceY.push_back({dy>0?i:j,dy>0?j:i,.001});
        }
    }
    ReducePositionEdges(count,sourceX);ReducePositionEdges(count,sourceY);
    auto project=[&](const std::vector<PositionSeparation>&separations){
        auto ex=sourceX,ey=sourceY;
        for(auto s:separations){double distance=(s.vertical?(src[s.first].h+src[s.second].h)*scale*.5+caption:(src[s.first].w+src[s.second].w)*scale*.5)+gap;
            (s.vertical?ey:ex).push_back({s.first,s.second,distance});}
        if(separations.size()>size_t(count*2)){ReducePositionEdges(count,ex);ReducePositionEdges(count,ey);}
        std::vector<double> x,y;PositionFit fit;
        if(!ProjectPositions(tx,weight,lx,hx,ex,x)||!ProjectPositions(ty,weight,ly,hy,ey,y))return fit;
        fit.cost=0;for(int i=0;i<count;++i){fit.boxes.push_back({x[i]-src[i].w*scale*.5,y[i]-src[i].h*scale*.5,src[i].w*scale,src[i].h*scale});
            fit.cost+=weight[i]*((x[i]-tx[i])*(x[i]-tx[i])+(y[i]-ty[i])*(y[i]-ty[i]));}
        return fit;
    };
    auto separation=[](const Pair&p,int direction){return PositionSeparation{direction%2?p.second:p.first,direction%2?p.first:p.second,direction>=2};};
    PositionFit best;
    // Start with a complete feasible candidate when available. Optimizing all
    // its positions already removes group-center and cross-axis alignment.
    auto trySeed=[&](const std::vector<Box>&positions,bool completeGaps=false){
        if(positions.size()!=src.size())return;
        std::vector<PositionSeparation> complete;
        for(const auto&p:pairs){auto a=positions[p.first],b=positions[p.second];
            if(a.w<=0){if(!completeGaps)return;a=seed[p.first];}
            if(b.w<=0){if(!completeGaps)return;b=seed[p.second];}
            unsigned directions=SeparationDirections(a,b,gap,caption)&p.allowed;
            int direction=0;
            if(!directions){if(!completeGaps)return;
                double bestShift=1e100,shifts[]{(a.right()+gap-b.x)/area.w,(b.right()+gap-a.x)/area.w,(a.bottom()+caption+gap-b.y)/area.h,(b.bottom()+caption+gap-a.y)/area.h};
                for(int d=0;d<4;++d)if((p.allowed&(1u<<d))&&shifts[d]<bestShift){bestShift=shifts[d];direction=d;}}
            else while(!(directions&(1u<<direction)))++direction;
            complete.push_back(separation(p,direction));}
        auto fit=project(complete);if(fit.cost<best.cost)best=std::move(fit);
    };
    trySeed(seed);trySeed(anchors,count<=64);
    struct Node {std::vector<PositionSeparation> separations;PositionFit fit;};
    std::vector<Node> pending;auto root=project({});if(!root.boxes.empty())pending.push_back({{},std::move(root)});
    // Branch only on an actual conflict. The budget is deterministic, so many
    // windows cannot turn this into an unbounded combinatorial search.
    const int budget=count<=4?256:count<=8?512:count<=10?96:count<=24?32:8;
    for(int step=0;step<budget&&!pending.empty();++step){
        auto chosen=std::min_element(pending.begin(),pending.end(),[](const Node&a,const Node&b){return a.fit.cost<b.fit.cost;});
        Node node=std::move(*chosen);pending.erase(chosen);if(node.fit.cost>=best.cost-1e-6)continue;
        int conflict=-1;double worst=-1;
        for(size_t k=0;k<pairs.size();++k){const auto&p=pairs[k];auto a=node.fit.boxes[p.first],b=node.fit.boxes[p.second];
            if(SeparationDirections(a,b,gap,caption)&p.allowed)continue;
            double distance=1e100;double shifts[]{a.right()+gap-b.x,b.right()+gap-a.x,a.bottom()+caption+gap-b.y,b.bottom()+caption+gap-a.y};
            for(int d=0;d<4;++d)if(p.allowed&(1u<<d))distance=std::min(distance,std::max(0.,shifts[d]));
            double cost=distance*distance/(1/weight[p.first]+1/weight[p.second]);if(cost>worst){worst=cost;conflict=int(k);}
        }
        if(conflict<0){best=std::move(node.fit);continue;}
        const auto&p=pairs[conflict];
        for(int direction=0;direction<4;++direction)if(p.allowed&(1u<<direction)){
            auto edges=node.separations;edges.push_back(separation(p,direction));auto fit=project(edges);
            if(!fit.boxes.empty()&&fit.cost<best.cost-1e-6)pending.push_back({std::move(edges),std::move(fit)});
        }
    }
    return best;
}
static std::vector<LayoutSlot> SavePositions(const std::vector<Box>&src,Box area){
    std::vector<LayoutSlot> slots(src.size());for(size_t i=0;i<src.size();++i)slots[i]={0,int(i),int(src.size()),false,0,src[i],area};return slots;
}
static std::vector<Box> Layout(const std::vector<Box>&src,Box area,double gap,double caption=0,std::vector<LayoutSlot>*slots=nullptr){
    auto seed=PackedLayout(src,area,gap,caption);if(seed.empty())return seed;
    gap=std::min(gap,std::sqrt(area.w*area.h/src.size())*.10);
    auto fit=FreePositions(src,seed,area,gap,caption,seed[0].w/src[0].w);
    if(slots)*slots=SavePositions(src,area);
    return fit.boxes.empty()?seed:fit.boxes;
}
static std::vector<Box> StableLayout(const std::vector<Box>&src,const std::vector<Box>&anchors,Box area,double gap,double caption,
        const std::vector<LayoutSlot>&previous={},std::vector<LayoutSlot>*next=nullptr,bool liveReflow=true){
    if(src.empty()||area.w<=0||area.h<=0)return {};
    // A completed session's locally adjusted layout is not an optimization seed.
    // Only previews already on screen need stable positions and separated paths.
    if(!liveReflow)return Layout(src,area,gap,caption,next);
    if(anchors.size()!=src.size()||previous.size()!=src.size())return Layout(src,area,gap,caption,next);
    bool unchanged=true;int known=0;double oldScale=.85;
    for(size_t i=0;i<src.size();++i){auto p=previous[i];if(anchors[i].w<=0||p.group<0){unchanged=false;continue;}++known;
        if(std::abs(p.original.x-src[i].x)>.5||std::abs(p.original.y-src[i].y)>.5)
            return Layout(src,area,gap,caption,next);
        oldScale=std::min({oldScale,anchors[i].w/p.original.w,anchors[i].h/p.original.h});
        unchanged&=p.count==int(src.size())&&std::abs(p.original.w-src[i].w)<.01&&std::abs(p.original.h-src[i].h)<.01&&p.area.x==area.x&&p.area.y==area.y&&p.area.w==area.w&&p.area.h==area.h;
    }
    if(!known)return Layout(src,area,gap,caption,next);
    if(unchanged){if(next)*next=previous;return anchors;}
    auto seed=PackedLayout(src,area,gap,caption);if(seed.empty())return seed;
    gap=std::min(gap,std::sqrt(area.w*area.h/src.size())*.10);
    double seedScale=seed[0].w/src[0].w,low=0,high=.85,bodyArea=0,captionArea=0;PositionFit best;
    bool allKnown=known==int(src.size());
    // Retain a directly verified feasible layout before searching. Removing a
    // window cannot lose its old scale merely because a new packing seed or a
    // bounded position search chooses a poorer arrangement.
    if(allKnown){std::vector<Box> retained;bool valid=true;
        for(size_t i=0;i<src.size();++i){Box b{anchors[i].cx()-src[i].w*oldScale*.5,anchors[i].cy()-src[i].h*oldScale*.5,src[i].w*oldScale,src[i].h*oldScale};
            auto footprint=b;footprint.h+=caption;valid&=Contains(area,footprint);
            for(size_t j=0;j<retained.size();++j)valid&=SeparationDirections(retained[j],b,gap,caption)!=0;
            retained.push_back(b);}
        if(valid&&ClearReflow(anchors,retained,caption)){best.boxes=std::move(retained);low=oldScale;}
    }
    if(seedScale>low&&ClearReflow(anchors,seed,caption)){best.boxes=seed;low=seedScale;}
    for(auto b:src){high=std::min({high,area.w/b.w,(area.h-caption)/b.h});bodyArea+=b.w*b.h;captionArea+=b.w*caption;}
    high=std::min(high,2*area.w*area.h/(captionArea+std::sqrt(captionArea*captionArea+4*bodyArea*area.w*area.h)));
    // Larger scenes retain the packing estimate as their search ceiling. Small
    // scenes can use the free space beyond it without an unbounded search.
    if(src.size()>24)high=std::min(high,std::max(seedScale,allKnown?oldScale:0.));
    double history=allKnown||src.size()<=24?.35:3.;
    auto attempt=[&](double scale){
        auto fit=FreePositions(src,seed,area,gap,caption,scale,anchors,history);
        if(!fit.boxes.empty()){low=scale;best=std::move(fit);seed=best.boxes;return true;}
        high=scale;return false;
    };
    double initial=std::clamp(std::max(seedScale,allKnown?oldScale:0.),low,std::max(low,high));
    attempt(initial);
    if(allKnown&&oldScale>low+1e-8&&oldScale<high-1e-8)attempt(oldScale);
    // Establish a close feasible lower bound before refining. Starting a
    // crowded insertion halfway to zero would waste most of the screen.
    for(int step=0;best.boxes.empty()&&step<8;++step)attempt(high*.9);
    int refinements=src.size()<=12?6:src.size()<=24?4:src.size()<=64?3:src.size()<=128?1:0;
    for(int step=0;step<refinements&&high-low>std::max(.001,high*.01);++step)attempt((low+high)*.5);
    for(int step=0;best.boxes.empty()&&step<8;++step)attempt(high*.5);
    if(!best.boxes.empty()&&best.cost==1e100){auto fit=FreePositions(src,seed,area,gap,caption,low,anchors,history);if(!fit.boxes.empty())best=std::move(fit);}
    if(next)*next=SavePositions(src,area);
    return best.boxes.empty()?Layout(src,area,gap,caption,next):best.boxes;
}

static double Curve(double t) {
    t=std::clamp(t,0.0,1.0);
    return (1-(1+9*t)*std::exp(-9*t))/(1-10*std::exp(-9.0));
}
static double CurveDerivative(double t){return 81*t*std::exp(-9*t)/(1-10*std::exp(-9.0));}

enum class MemoryMode{Automatic,Resident,Saver};
static std::wstring WindowGroupKey(HWND window){
    ComPtr<IPropertyStore> properties;PROPVARIANT value{};std::wstring key;
    if(SUCCEEDED(SHGetPropertyStoreForWindow(window,IID_PPV_ARGS(&properties)))&&SUCCEEDED(properties->GetValue(PKEY_AppUserModel_ID,&value))&&value.vt==VT_LPWSTR&&value.pwszVal)key=L"app:"+std::wstring(value.pwszVal);
    PropVariantClear(&value);if(!key.empty())return key;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(process){wchar_t name[32768];UINT32 length=ARRAYSIZE(name);
        if(GetApplicationUserModelId(process,&length,name)==ERROR_SUCCESS)key=L"app:"+std::wstring(name);
        else{DWORD size=ARRAYSIZE(name);if(QueryFullProcessImageName(process,0,name,&size)){CharLowerBuff(name,size);key=L"exe:"+std::wstring(name,size);}}
        CloseHandle(process);}
    return key.empty()?L"pid:"+std::to_wstring(pid):key;
}
struct TaskbarEntry{HWND window;HMONITOR monitor;std::wstring group;double x,y;};
// UI Automation can take a shell round trip. Keep all of it on an event-driven
// worker: the renderer only copies a small completed snapshot under a mutex.
class TaskbarOrder {
    HANDLE request=nullptr,stop=nullptr,thread=nullptr;HWND notify=nullptr;
    std::mutex mutex;std::vector<TaskbarEntry> snapshot;
    static DWORD WINAPI Worker(void*parameter){static_cast<TaskbarOrder*>(parameter)->Run();return 0;}
    static BOOL CALLBACK CollectTaskbars(HWND w,LPARAM p){wchar_t cls[64];GetClassName(w,cls,64);if(!wcscmp(cls,L"Shell_TrayWnd")||!wcscmp(cls,L"Shell_SecondaryTrayWnd"))reinterpret_cast<std::vector<HWND>*>(p)->push_back(w);return TRUE;}
    void Run(){
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        {ComPtr<IUIAutomation> automation;CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation));
        if(automation){ComPtr<IUIAutomation2> timed;if(SUCCEEDED(automation.As(&timed))){timed->put_ConnectionTimeout(200);timed->put_TransactionTimeout(200);}
            HANDLE events[]{stop,request};while(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0+1){
                std::vector<TaskbarEntry> result;std::vector<HWND> bars;
                EnumWindows(CollectTaskbars,reinterpret_cast<LPARAM>(&bars));
                bool valid=false;
                for(HWND bar:bars){
                    ComPtr<IUIAutomationElement> root;ComPtr<IUIAutomationCondition> condition;ComPtr<IUIAutomationCacheRequest> cache;ComPtr<IUIAutomationElementArray> buttons;
                    VARIANT cls{};cls.vt=VT_BSTR;cls.bstrVal=SysAllocString(L"Taskbar.TaskListButtonAutomationPeer");
                    HRESULT hr=automation->ElementFromHandle(bar,&root);
                    if(SUCCEEDED(hr))hr=automation->CreatePropertyCondition(UIA_ClassNamePropertyId,cls,&condition);VariantClear(&cls);
                    if(SUCCEEDED(hr))hr=automation->CreateCacheRequest(&cache);
                    if(SUCCEEDED(hr))hr=cache->AddProperty(UIA_AutomationIdPropertyId);
                    if(SUCCEEDED(hr))hr=cache->AddProperty(UIA_BoundingRectanglePropertyId);
                    if(SUCCEEDED(hr))hr=root->FindAllBuildCache(TreeScope_Descendants,condition.Get(),cache.Get(),&buttons);
                    if(FAILED(hr))continue;valid=true;int count=0;buttons->get_Length(&count);HMONITOR monitor=MonitorFromWindow(bar,MONITOR_DEFAULTTONEAREST);
                    for(int n=0;n<count;++n){ComPtr<IUIAutomationElement> button;BSTR id=nullptr;RECT bounds{};
                        if(FAILED(buttons->GetElement(n,&button)))continue;
                        button->get_CachedAutomationId(&id);button->get_CachedBoundingRectangle(&bounds);
                        if(id&&wcsncmp(id,L"Window: ",8)==0){HWND source=reinterpret_cast<HWND>(static_cast<UINT_PTR>(wcstoull(id+8,nullptr,0)));
                            if(IsWindow(source))result.push_back({source,monitor,WindowGroupKey(source),double(bounds.left),double(bounds.top)});}
                        SysFreeString(id);
                    }
                }
                if(valid){std::stable_sort(result.begin(),result.end(),[](const auto&a,const auto&b){if(a.monitor!=b.monitor)return reinterpret_cast<UINT_PTR>(a.monitor)<reinterpret_cast<UINT_PTR>(b.monitor);return a.y!=b.y?a.y<b.y:a.x<b.x;});
                    {std::lock_guard<std::mutex> lock(mutex);snapshot=std::move(result);}PostMessage(notify,kTaskbarOrder,0,0);}
            }
        }}CoUninitialize();
    }
public:
    void Start(HWND hwnd){if(thread)return;notify=hwnd;request=CreateEvent(nullptr,FALSE,FALSE,nullptr);stop=CreateEvent(nullptr,TRUE,FALSE,nullptr);
        if(request&&stop)thread=CreateThread(nullptr,0,Worker,this,0,nullptr);if(!thread){Stop();return;}Refresh();}
    void Refresh(){if(request)SetEvent(request);}
    std::vector<TaskbarEntry> Read(){std::lock_guard<std::mutex> lock(mutex);return snapshot;}
    bool Running()const{return thread!=nullptr;}
    void Stop(){if(stop)SetEvent(stop);if(thread){WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;}
        if(request){CloseHandle(request);request=nullptr;}if(stop){CloseHandle(stop);stop=nullptr;}}
    ~TaskbarOrder(){Stop();}
};
struct Settings{bool winTab=true;int duration=320;MemoryMode memoryMode=MemoryMode::Automatic;};
static std::atomic<bool> g_winTab{true};
static std::atomic<int> g_duration{320};
static std::atomic<int> g_memoryMode{0};
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

using AccentEvent=ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::UI::ViewManagement::UISettings*,IInspectable*>;
struct AccentWatcher final:AccentEvent {
    std::atomic<ULONG> refs{1};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void**out) override{
        if(!out)return E_POINTER;*out=nullptr;
        if(iid!=IID_IUnknown&&iid!=__uuidof(AccentEvent)&&iid!=__uuidof(IAgileObject))return E_NOINTERFACE;
        *out=static_cast<AccentEvent*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Invoke(ABI::Windows::UI::ViewManagement::IUISettings*,IInspectable*) override{
        if(HWND hwnd=g_controller.load())PostMessage(hwnd,kAccentChanged,0,0);return S_OK;
    }
};

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
enum class WindowButton{None,Minimize,Close};
struct Item {
    Item()=default;
    Item(const Item&)=delete;
    Item& operator=(const Item&)=delete;
    Item(Item&&) noexcept=default;
    Item& operator=(Item&& other) noexcept{
        if(this!=&other){std::destroy_at(this);std::construct_at(this,std::move(other));}
        return *this;
    }
    HWND source=nullptr;DWORD sourceProcess=0;
    std::wstring groupKey;int taskbarRank=INT_MAX;
    bool minimized=false,canMinimize=false,iconQueried=false;
    ComPtr<IWICBitmap> iconBitmap;
    Box original,target,minimizedOrigin;
    Box layoutAnchor;bool anchorMinimized=false;LayoutSlot anchorSlot,layoutSlot;
    Box motionFrom,motionTo;
    float radiusFrom=0,radiusTo=0;
    double labelFrom=0,labelTo=0,labelSeconds=0;
    SIZE pixels{};
    RECT restoredSample{};
    bool restoredSampleValid=false;
    float nativeRadius=0;
    HTHUMBNAIL thumbnail=nullptr;
    ComPtr<IDCompositionVisual2> surface,position,content,border,borderFill,label,buttons;
    ComPtr<IDCompositionScaleTransform> scale,borderScale;
    ComPtr<IDCompositionRectangleClip> clip;
    ComPtr<IDCompositionEffectGroup> borderEffect,labelEffect;
    ComPtr<IDCompositionSurface> labelSurface,borderSurface;
    UINT32 borderColor=0;bool borderAttached=false,buttonsAttached=false;
    WindowButton hotButton=WindowButton::None;
    float stroke=3,gap=4,borderWidth=1,borderHeight=1;
    float titleLeft=0,titleRight=0,titleHeight=0;
    std::wstring titleText;double titleWidth=0,titleRadius=0;float titleDpi=0;
    float Outset()const{return stroke+gap;}
};
struct View {
    HWND hwnd=nullptr;HMONITOR monitor=nullptr;RECT bounds{};float dpi=1;
    std::vector<Item> items;int selected=-1;
    std::vector<int> hitOrder; // Front to back, matching the visual tree.
    std::vector<HWND> visualOrder;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual2> root,windows,desktop;
    ComPtr<IDCompositionSurface> wallpaperSurface;
    ComPtr<IDCompositionSurface> buttonSurfaces[2][3];
};
struct DesktopCache {
    HMONITOR monitor=nullptr;RECT bounds{},display{};std::wstring path;
    FILETIME modified{};COLORREF color=0;DESKTOP_WALLPAPER_POSITION mode=DWPOS_FILL;
    ComPtr<IDCompositionSurface> surface;
};
enum class Phase{Hidden,Opening,Settled,Closing};

class App {
public:
    static App* instance;
    HWND controller=nullptr;HANDLE timer=nullptr,lowMemoryEvent=nullptr;
    ComPtr<ABI::Windows::UI::ViewManagement::IUISettings3> uiSettings;
    EventRegistrationToken accentToken{};bool accentSubscribed=false;
    HWINEVENTHOOK foregroundHook=nullptr,destroyHook=nullptr,restoreHook=nullptr,windowStateHook=nullptr,geometryHook=nullptr;
    ComPtr<IVirtualDesktopManager> desktops;
    ComPtr<ID3D11Device> d3d;
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDCompositionDesktopDevice> device;
    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<IDCompositionSurfaceFactory> surfaceFactory;
    ComPtr<IDCompositionSurface> minimizedPlaceholder;
    UINT32 accentColor=0x0078D4;bool accentValid=false;
    ComPtr<IDWriteFactory> textFactory;
    struct TitleStyle {ComPtr<IDWriteTextFormat> format;ComPtr<IDWriteInlineObject> ellipsis;};
    std::map<float,TitleStyle> titleStyles;
    ComPtr<IWICImagingFactory> imageFactory;
    std::vector<DesktopCache> desktopCache;
    TaskbarOrder taskbarOrder;
    PrivateDwm api;
    std::vector<std::unique_ptr<View>> views;
    Settings settings;Phase phase=Phase::Hidden;
    HWND previous=nullptr,chosen=nullptr,selectedSource=nullptr,pressedButtonSource=nullptr,restoreSource=nullptr;
    WindowButton pressedButton=WindowButton::None;
    POINT lastPointer{};
    bool keyboardSelection=false;
    UINT gestureToken=0;
    double gestureOriginX=0,gestureOriginY=0,gestureWidth=1,gestureHeight=1;
    ULONGLONG gestureStarted=0;
    bool gestureNavigating=false,gestureReady=false,gestureOpenedAlready=false;
    double gestureBaseX=0,gestureBaseY=0,gestureLastX=0,gestureLastY=0,gestureMinY=0,gestureReadyX=0,gestureReadyY=0;
    struct GestureMessage {UINT message;WPARAM token;LPARAM data;};
    std::vector<GestureMessage> deferredGestures;
    bool idleTimer=false,building=false,deferredWindowState=false,deferredTaskbar=false;
    bool memoryPressure=false;
    UINT deferredCommand=0;
    std::vector<HWND> deferredRemovals;
    std::vector<HWND> deferredSceneChanges;
    bool sceneNeedsLayout=false;
#ifdef SCATTER_TEST
    unsigned layoutPasses=0;
#endif
    struct BuildGuard {
        App&a;bool wasBuilding;explicit BuildGuard(App&app):a(app),wasBuilding(app.building){a.building=true;}
        ~BuildGuard(){a.building=wasBuilding;if(!wasBuilding){if(UINT msg=std::exchange(a.deferredCommand,0u))PostMessage(a.controller,msg,0,0);
            if(std::exchange(a.deferredWindowState,false))PostMessage(a.controller,kWindowStateChanged,0,0);
            if(std::exchange(a.deferredTaskbar,false))PostMessage(a.controller,kTaskbarOrder,0,0);
            for(HWND source:a.deferredSceneChanges)PostMessage(a.controller,kSceneChanged,reinterpret_cast<WPARAM>(source),0);a.deferredSceneChanges.clear();
            for(HWND source:a.deferredRemovals)PostMessage(a.controller,kWindowGone,reinterpret_cast<WPARAM>(source),0);a.deferredRemovals.clear();
            for(auto event:a.deferredGestures)PostMessage(a.controller,event.message,event.token,event.data);a.deferredGestures.clear();}}
    };
    LARGE_INTEGER frequency{};
    double start=0,seconds=0,from=0,to=1,progress=0;
    LONGLONG animationStart=0;
    struct AnimationEntry {double a,b,duration;ComPtr<IDCompositionAnimation> animation;};
    std::vector<AnimationEntry> animations;
    std::map<std::array<double,3>,size_t> animationIndex;
    double frameSeconds=1./60;

    double Now()const{LARGE_INTEGER t;QueryPerformanceCounter(&t);return double(t.QuadPart)/frequency.QuadPart;}
    void LoadSettings(){settings={g_winTab.load(),std::clamp(g_duration.load(),0,900),MemoryMode(std::clamp(g_memoryMode.load(),0,2))};}
    bool LowMemory()const{
        BOOL low=FALSE;if(lowMemoryEvent&&QueryMemoryResourceNotification(lowMemoryEvent,&low))return low!=FALSE;
        MEMORYSTATUSEX status{sizeof(status)};
        return GlobalMemoryStatusEx(&status)&&status.ullAvailPhys<std::min<ULONGLONG>(1024ull*1024*1024,status.ullTotalPhys/20);
    }
    bool ShouldReleaseGraphics()const{
        return settings.memoryMode==MemoryMode::Saver||
            (settings.memoryMode==MemoryMode::Automatic&&(memoryPressure||LowMemory()));
    }
    void ReleaseInactiveGraphics(){
        if(building||phase!=Phase::Hidden||!ShouldReleaseGraphics())return;
        BuildGuard guard(*this);if(timer)CancelWaitableTimer(timer);idleTimer=false;
        ReleaseDevice();desktops.Reset();taskbarOrder.Stop();memoryPressure=false;
        CoFreeUnusedLibrariesEx(0,0);
        HEAP_OPTIMIZE_RESOURCES_INFORMATION info{HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION,0};
        HeapSetInformation(nullptr,HeapOptimizeResources,&info,sizeof(info));
        // Graphics ownership is already gone. Under the low-memory/saver policy,
        // let Windows reclaim also the now-unused DLL/allocator resident pages.
        // This trims physical residency; it does not pretend to remove commit.
        SetProcessWorkingSetSize(GetCurrentProcess(),SIZE_T(-1),SIZE_T(-1));
    }
    void TrimIdleResources(){
        if(building||phase!=Phase::Hidden)return;
        idleTimer=false;taskbarOrder.Stop();
        if(ShouldReleaseGraphics()){ReleaseInactiveGraphics();return;}
        // Re-decoding and uploading full wallpapers on the next keypress is
        // much slower than retaining a bounded set of immutable surfaces.
        // This is a one-shot cleanup, with no polling or background rendering.
        uint64_t remaining=64ull*1024*1024;
        std::erase_if(desktopCache,[&](const auto&cached){
            uint64_t bytes=uint64_t(std::max<LONG>(0,cached.bounds.right-cached.bounds.left))*
                           uint64_t(std::max<LONG>(0,cached.bounds.bottom-cached.bounds.top))*4;
            if(bytes>remaining)return true;remaining-=bytes;return false;
        });
        if(d2dDevice)d2dDevice->ClearResources(0);
        if(device)device->Commit();
    }
    void MemoryBecameLow(){
        if(settings.memoryMode!=MemoryMode::Automatic)return;
        memoryPressure=true;
        if(phase==Phase::Hidden)ReleaseInactiveGraphics();
    }
    static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){return instance?instance->Message(hwnd,msg,wp,lp):DefWindowProc(hwnd,msg,wp,lp);}
    void FocusOverview(){
        HWND focus=nullptr;POINT p{};GetCursorPos(&p);HMONITOR monitor=MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST);
        for(auto&v:views)if(v->hwnd&&(!focus||v->monitor==monitor))focus=v->hwnd;
        if(focus){SetForegroundWindow(focus);SetFocus(focus);}
    }
    static void CALLBACK Event(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG child,DWORD,DWORD){
        App*a=instance;if(!a||a->phase==Phase::Hidden)return;
        if(event==EVENT_SYSTEM_FOREGROUND)PostMessage(a->controller,a->phase==Phase::Closing?kSyncOrder:kDismiss,0,0);
        else if(event==EVENT_OBJECT_SHOW&&object==OBJID_WINDOW&&child==0&&GetAncestor(hwnd,GA_ROOT)==hwnd)PostMessage(a->controller,kSceneChanged,reinterpret_cast<WPARAM>(hwnd),0);
        else if((event==EVENT_OBJECT_DESTROY||event==EVENT_OBJECT_HIDE)&&object==OBJID_WINDOW&&child==0)
            for(auto&v:a->views)for(auto&i:v->items)if(i.source==hwnd){
                if(event==EVENT_OBJECT_HIDE&&IsWindowVisible(hwnd)&&IsIconic(hwnd))PostMessage(a->controller,kWindowStateChanged,0,0);
                else PostMessage(a->controller,kWindowGone,reinterpret_cast<WPARAM>(hwnd),0);
                return;
            }
    }
    static void CALLBACK WindowStateEvent(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG child,DWORD,DWORD){
        App*a=instance;if(!a||a->phase==Phase::Hidden)return;
        bool stateEvent=event==EVENT_SYSTEM_MINIMIZESTART||event==EVENT_SYSTEM_MINIMIZEEND;
        if(!stateEvent&&(object!=OBJID_WINDOW||child!=0))return;
        for(auto&v:a->views)for(auto&i:v->items)if(i.source==hwnd){
            PostMessage(a->controller,stateEvent||i.minimized?kWindowStateChanged:kSceneChanged,stateEvent||i.minimized?0:reinterpret_cast<WPARAM>(hwnd),0);return;
        }
    }
    bool IsOurWindow(HWND hwnd)const{if(hwnd==controller)return true;for(auto&v:views)if(v->hwnd==hwnd)return true;return false;}
    bool Initialize(){
        QueryPerformanceFrequency(&frequency);SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);LoadSettings();
        if(!api.Load()){Log(L"Shared DWM APIs unavailable",E_NOINTERFACE);return false;}
        WNDCLASSEX wc{sizeof(wc)};wc.lpfnWndProc=WndProc;wc.hInstance=g_instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=kController;
        if(!RegisterClassEx(&wc))return false;wc.lpszClassName=kOverlay;if(!RegisterClassEx(&wc))return false;
        controller=CreateWindowEx(WS_EX_TOOLWINDOW,kController,L"Window Scatter Controller",0,0,0,0,0,nullptr,nullptr,g_instance,nullptr);
        if(!controller)return false;g_controller.store(controller);InitializeAccent();RefreshAccent();
        timer=CreateWaitableTimer(nullptr,FALSE,nullptr);if(!timer)return false;
        lowMemoryEvent=CreateMemoryResourceNotification(LowMemoryResourceNotification);
        if(!lowMemoryEvent)Log(L"Memory notifications unavailable; using idle fallback",HRESULT_FROM_WIN32(GetLastError()));
#ifdef SCATTER_STANDALONE
        if(g_probe)return true;
#endif
        RegisterHotKey(controller,2,MOD_WIN|MOD_SHIFT|MOD_NOREPEAT,VK_TAB);
        ChangeWindowMessageFilterEx(controller,kShellTaskView,MSGFLT_ALLOW,nullptr);
        ChangeWindowMessageFilterEx(controller,kNativeConsumed,MSGFLT_ALLOW,nullptr);
        for(UINT message:{kGestureBegin,kGestureUpdate,kGestureEnd,kGestureCancel})ChangeWindowMessageFilterEx(controller,message,MSGFLT_ALLOW,nullptr);
        SetProp(controller,kShellReadyProperty,reinterpret_cast<HANDLE>(kShellAck));
        taskbarOrder.Start(controller);
        PostMessage(controller,kWarmup,0,0);
        return true;
    }
    bool EnsureDevice(){
        if(device&&d3d&&SUCCEEDED(d3d->GetDeviceRemovedReason()))return true;
        if(device)ReleaseDevice();
        HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d3d,nullptr,nullptr);
        if(SUCCEEDED(hr))hr=d3d.As(&dxgi);
        if(SUCCEEDED(hr))hr=DCompositionCreateDevice3(dxgi.Get(),IID_PPV_ARGS(&device));
        if(SUCCEEDED(hr))hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,IID_PPV_ARGS(&d2dFactory));
        if(SUCCEEDED(hr))hr=d2dFactory->CreateDevice(dxgi.Get(),&d2dDevice);
        if(SUCCEEDED(hr))hr=device->CreateSurfaceFactory(d2dDevice.Get(),&surfaceFactory);
        if(SUCCEEDED(hr))hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory.GetAddressOf()));
        if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imageFactory));
        if(FAILED(hr)){Log(L"Create compositor",hr);ReleaseDevice();return false;}return true;
    }
    void ReleaseDevice(){
        BuildGuard guard(*this);
        desktopCache.clear();animations.clear();animationIndex.clear();minimizedPlaceholder.Reset();imageFactory.Reset();titleStyles.clear();textFactory.Reset();
        surfaceFactory.Reset();d2dDevice.Reset();d2dFactory.Reset();
        if(device){HRESULT hr=device->Commit();if(SUCCEEDED(hr))device->WaitForCommitCompletion();}
        device.Reset();dxgi.Reset();d3d.Reset();
    }
    template<class Draw> HRESULT Paint(ComPtr<IDCompositionSurface>&surface,UINT width,UINT height,bool opaque,Draw draw){
        HRESULT hr=surfaceFactory->CreateSurface(width,height,DXGI_FORMAT_B8G8R8A8_UNORM,
            opaque?DXGI_ALPHA_MODE_IGNORE:DXGI_ALPHA_MODE_PREMULTIPLIED,surface.ReleaseAndGetAddressOf());
        if(FAILED(hr))return hr;
        ComPtr<ID2D1DeviceContext> ctx;POINT offset{};
        hr=surface->BeginDraw(nullptr,IID_PPV_ARGS(&ctx),&offset);if(FAILED(hr))return hr;
        ctx->SetDpi(96,96);ctx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        ctx->SetTransform(D2D1::Matrix3x2F::Translation(float(offset.x),float(offset.y)));
        ctx->PushAxisAlignedClip(D2D1::RectF(0,0,float(width),float(height)),D2D1_ANTIALIAS_MODE_ALIASED);
        ctx->Clear(D2D1::ColorF(0,0.f));draw(ctx.Get());ctx->PopAxisAlignedClip();
        ctx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        return surface->EndDraw();
    }
    bool PrepareDesktop(View&v){
        MONITORINFO mi{sizeof(mi)};GetMonitorInfo(v.monitor,&mi);
        // Display/settings notifications invalidate the metadata. Between them,
        // validate the file timestamp without repeating shell COM enumeration.
        for(const auto&cached:desktopCache)if(cached.monitor==v.monitor&&EqualRect(&cached.bounds,&v.bounds)){
            RECT expected=mi.rcMonitor;if(cached.mode==DWPOS_SPAN)expected={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN)};
            WIN32_FILE_ATTRIBUTE_DATA info{};if(!cached.path.empty())GetFileAttributesEx(cached.path.c_str(),GetFileExInfoStandard,&info);
            if(EqualRect(&cached.display,&expected)&&CompareFileTime(&cached.modified,&info.ftLastWriteTime)==0){v.wallpaperSurface=cached.surface;return true;}
        }
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
        if(mode==DWPOS_SPAN)display={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN)};
        WIN32_FILE_ATTRIBUTE_DATA fileInfo{};if(!path.empty())GetFileAttributesEx(path.c_str(),GetFileExInfoStandard,&fileInfo);
        auto found=std::find_if(desktopCache.begin(),desktopCache.end(),[&](const auto&c){return c.monitor==v.monitor;});
        if(found!=desktopCache.end()&&found->path==path&&found->mode==mode&&found->color==color&&
           EqualRect(&found->bounds,&v.bounds)&&EqualRect(&found->display,&display)&&CompareFileTime(&found->modified,&fileInfo.ftLastWriteTime)==0){v.wallpaperSurface=found->surface;return true;}
        ComPtr<IWICBitmap> bitmapSource;
        {
            ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
            HRESULT hr=path.empty()?E_FAIL:imageFactory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
            if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
            if(SUCCEEDED(hr))hr=imageFactory->CreateFormatConverter(&converter);
            if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
            if(SUCCEEDED(hr))imageFactory->CreateBitmapFromSource(converter.Get(),WICBitmapCacheOnLoad,&bitmapSource);
        }
        UINT width=v.bounds.right-v.bounds.left,height=v.bounds.bottom-v.bounds.top;
        HRESULT hr=Paint(v.wallpaperSurface,width,height,true,[&](ID2D1DeviceContext*ctx){
            // This opaque base is mandatory: excluding a live window may leave
            // transparent pixels, which otherwise expose the ORIGINAL window.
            ctx->Clear(D2D1::ColorF(GetRValue(color)/255.f,GetGValue(color)/255.f,GetBValue(color)/255.f,1.f));
            if(!bitmapSource)return;ComPtr<ID2D1Bitmap> bitmap;
            if(FAILED(ctx->CreateBitmapFromWicBitmap(bitmapSource.Get(),nullptr,&bitmap)))return;
            auto size=bitmap->GetSize();if(size.width<1||size.height<1)return;
            float mw=float(display.right-display.left),mh=float(display.bottom-display.top);
            float x=float(display.left-v.bounds.left),y=float(display.top-v.bounds.top),w=size.width,h=size.height;
            if(mode==DWPOS_TILE){for(float ty=y;ty<float(height);ty+=h)for(float tx=x;tx<float(width);tx+=w)ctx->DrawBitmap(bitmap.Get(),D2D1::RectF(tx,ty,tx+w,ty+h));return;}
            if(mode==DWPOS_STRETCH){w=mw;h=mh;}
            else if(mode!=DWPOS_CENTER){float scale=mode==DWPOS_FIT?std::min(mw/w,mh/h):std::max(mw/w,mh/h);w*=scale;h*=scale;}
            x+=(mw-w)*.5f;y+=(mh-h)*.5f;ctx->DrawBitmap(bitmap.Get(),D2D1::RectF(x,y,x+w,y+h));
        });
        if(FAILED(hr)){Log(L"Prepare wallpaper",hr);return false;}
        DesktopCache entry{v.monitor,v.bounds,display,path,fileInfo.ftLastWriteTime,color,mode,v.wallpaperSurface};
        // COM/painting can dispatch a settings notification that invalidates
        // the cache; never retain an iterator across those calls.
        std::erase_if(desktopCache,[&](const auto&c){return c.monitor==v.monitor;});desktopCache.push_back(std::move(entry));
        return true;
    }
    static BOOL CALLBACK WarmMonitor(HMONITOR monitor,HDC,LPRECT,LPARAM data){
        auto&a=*reinterpret_cast<App*>(data);MONITORINFO mi{sizeof(mi)};if(GetMonitorInfo(monitor,&mi)){View v;v.monitor=monitor;v.bounds=mi.rcWork;a.PrepareDesktop(v);}return TRUE;
    }
    void Warmup(){
        if(phase!=Phase::Hidden||building||ShouldReleaseGraphics())return;BuildGuard guard(*this);
        if(!EnsureDevice())return;
        EnumDisplayMonitors(nullptr,nullptr,WarmMonitor,reinterpret_cast<LPARAM>(this));
        device->Commit();idleTimer=true;Schedule(60);
    }
    bool CreateDesktop(View&v){
        if(!PrepareDesktop(v))return false;
        HRESULT hr=device->CreateVisual(&v.desktop);
        if(SUCCEEDED(hr))hr=v.desktop->SetContent(v.wallpaperSurface.Get());
        // With a null reference, AddVisual(FALSE) inserts ABOVE all siblings.
        // An explicit sibling reference keeps the wallpaper below the windows.
        if(SUCCEEDED(hr))hr=v.root->AddVisual(v.desktop.Get(),FALSE,v.windows.Get());
        if(FAILED(hr)){Log(L"Opaque desktop",hr);return false;}
        return true;
    }
    void InitializeAccent(){
        constexpr wchar_t name[]=L"Windows.UI.ViewManagement.UISettings";
        HSTRING value=nullptr;ComPtr<IInspectable> settingsObject;
        HRESULT hr=WindowsCreateString(name,ARRAYSIZE(name)-1,&value);
        if(SUCCEEDED(hr))hr=RoActivateInstance(value,&settingsObject);
        WindowsDeleteString(value);
        if(SUCCEEDED(hr))hr=settingsObject.As(&uiSettings);
        if(FAILED(hr)){Log(L"Read system accent",hr);return;}
        ComPtr<AccentWatcher> watcher;watcher.Attach(new(std::nothrow) AccentWatcher);
        if(watcher)accentSubscribed=SUCCEEDED(uiSettings->add_ColorValuesChanged(watcher.Get(),&accentToken));
    }
    static D2D1_COLOR_F OutlineColor(UINT32 rgb){
        return D2D1::ColorF(float((rgb>>16)&255)/255.f,float((rgb>>8)&255)/255.f,float(rgb&255)/255.f,1.f);
    }
    void RefreshAccent(){
        ABI::Windows::UI::Color color{};UINT32 rgb=accentColor;
        if(uiSettings&&SUCCEEDED(uiSettings->GetColorValue(ABI::Windows::UI::ViewManagement::UIColorType_Accent,&color))&&color.A){
            rgb=(UINT32(color.R)<<16)|(UINT32(color.G)<<8)|color.B;accentValid=true;
        }else if(!accentValid){COLORREF c=GetSysColor(COLOR_HIGHLIGHT);rgb=(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c);}
        if(rgb==accentColor)return;accentColor=rgb;
        if(!building&&phase!=Phase::Hidden&&device){
            for(auto&v:views)for(auto&i:v->items)if(i.borderAttached&&!CreateOutline(*v,i)){End(false);return;}
            device->Commit();
        }
    }
    static float SystemCornerRadius(HWND source,float dpi){
        // Read native policy once when creating the preview; never modify the
        // source window. This initializes the opening/closing clip radius.
        // The live DWM visual supplies the window content and its alpha.
        using IsArranged=BOOL(WINAPI*)(HWND);
        static auto arranged=reinterpret_cast<IsArranged>(GetProcAddress(GetModuleHandle(L"user32.dll"),"IsWindowArranged"));
        if(IsZoomed(source)||(arranged&&arranged(source)))return 0;
        DWORD preference=DWMWCP_DEFAULT;
        if(FAILED(DwmGetWindowAttribute(source,DWMWA_WINDOW_CORNER_PREFERENCE,&preference,sizeof(preference))))return 0;
        if(preference==DWMWCP_DONOTROUND)return 0;
        if(GetWindowLongPtr(source,GWL_EXSTYLE)&WS_EX_LAYERED)return 0;
        if(preference==DWMWCP_DEFAULT&&!(GetWindowLongPtr(source,GWL_STYLE)&(WS_CAPTION|WS_THICKFRAME)))return 0;
        return (preference==DWMWCP_ROUNDSMALL?4.f:8.f)*dpi;
    }
    static float OverviewRadius(const View&v,const Box&b){return std::min({8.f*v.dpi,float(b.w*.5),float(b.h*.5)});}
    static float TitleProtrusion(const View&v,const Item&i){return std::max(0.f,i.titleHeight-v.dpi);}
    static float TitleShoulderRadius(const View&v,const Item&i){
        float clearance=std::min(i.titleLeft,float(i.target.w)-i.titleRight)-OverviewRadius(v,i.target);
        return std::max(0.f,std::min({8*v.dpi,clearance,(i.titleHeight-v.dpi)*.5f}));
    }
    HRESULT TitleGeometry(const View&v,const Item&i,float x,float y,ComPtr<ID2D1PathGeometry>&path){
        HRESULT hr=d2dFactory->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;
        if(SUCCEEDED(hr))hr=path->Open(&sink);if(FAILED(hr))return hr;
        float left=x+i.titleLeft,right=x+i.titleRight,bottom=y+i.titleHeight;
        float r=std::min({6*v.dpi,(right-left)*.5f,i.titleHeight*.5f});
        float shoulder=TitleShoulderRadius(v,i),edge=y+v.dpi;
        // The overlap is hidden behind the preview; the exposed shoulder starts
        // exactly at its bottom edge and joins it with a tangent circular arc.
        sink->BeginFigure(D2D1::Point2F(left-shoulder,y),D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(right+shoulder,y));sink->AddLine(D2D1::Point2F(right+shoulder,edge));
        if(shoulder>0)sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(right,edge+shoulder),D2D1::SizeF(shoulder,shoulder),0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
        sink->AddLine(D2D1::Point2F(right,bottom-r));
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(right-r,bottom),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
        sink->AddLine(D2D1::Point2F(left+r,bottom));
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(left,bottom-r),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
        sink->AddLine(D2D1::Point2F(left,edge+shoulder));
        if(shoulder>0)sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(left-shoulder,edge),D2D1::SizeF(shoulder,shoulder),0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);return sink->Close();
    }
    template<class Value> static void SetCorners(Item&i,Value radius){
        // Reuse the clip already needed for rectangular bounds. Native rounded
        // clipping needs no bitmap mask, filter inputs or per-frame CPU updates.
        auto*c=i.clip.Get();
        c->SetTopLeftRadiusX(radius);c->SetTopLeftRadiusY(radius);
        c->SetTopRightRadiusX(radius);c->SetTopRightRadiusY(radius);
        c->SetBottomLeftRadiusX(radius);c->SetBottomLeftRadiusY(radius);
        c->SetBottomRightRadiusX(radius);c->SetBottomRightRadiusY(radius);
    }
    bool EnsureDecorations(View&v,Item&i){
        if(i.border&&i.borderFill&&i.buttons&&i.borderScale&&i.borderEffect)return true;
        HRESULT hr=device->CreateVisual(i.border.ReleaseAndGetAddressOf());
        if(SUCCEEDED(hr))hr=device->CreateVisual(i.borderFill.ReleaseAndGetAddressOf());
        if(SUCCEEDED(hr))hr=device->CreateVisual(i.buttons.ReleaseAndGetAddressOf());
        if(SUCCEEDED(hr))hr=device->CreateScaleTransform(i.borderScale.ReleaseAndGetAddressOf());
        if(SUCCEEDED(hr))hr=device->CreateEffectGroup(i.borderEffect.ReleaseAndGetAddressOf());
        if(FAILED(hr))return false;
        i.border->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);i.borderFill->SetTransform(i.borderScale.Get());
        i.border->AddVisual(i.borderFill.Get(),TRUE,nullptr);i.border->SetOffsetX(-i.Outset());i.border->SetOffsetY(-i.Outset());
        i.borderEffect->SetOpacity(0.f);i.border->SetEffect(i.borderEffect.Get());i.buttons->SetOffsetY(CloseInset(v));return true;
    }
    bool CreateOutline(View&v,Item&i){
        if(!EnsureDecorations(v,i))return false;
        // A hollow premultiplied surface leaves the requested gap transparent.
        // Rasterize once at overview size, then animate its transform on the GPU.
        i.borderWidth=float(i.target.w)+2*i.Outset();i.borderHeight=float(i.target.h)+TitleProtrusion(v,i)+2*i.Outset();
        float o=i.Outset(),w=float(i.target.w),h=float(i.target.h);
        float r=OverviewRadius(v,i.target);
        ComPtr<ID2D1RoundedRectangleGeometry> body;ComPtr<ID2D1Geometry> path;
        HRESULT hr=d2dFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(o,o,o+w,o+h),r,r),&body);
        if(SUCCEEDED(hr))path=body;
        if(SUCCEEDED(hr)&&i.titleHeight>0&&i.titleRight>i.titleLeft){
            ComPtr<ID2D1PathGeometry> title,combined;ComPtr<ID2D1GeometrySink> sink;
            hr=TitleGeometry(v,i,o,o+h-v.dpi,title);
            if(SUCCEEDED(hr))hr=d2dFactory->CreatePathGeometry(&combined);
            if(SUCCEEDED(hr))hr=combined->Open(&sink);
            if(SUCCEEDED(hr))hr=body->CombineWithGeometry(title.Get(),D2D1_COMBINE_MODE_UNION,nullptr,.1f,sink.Get());
            if(SUCCEEDED(hr))hr=sink->Close();if(SUCCEEDED(hr))path=combined;
        }
        if(FAILED(hr))return false;
        HRESULT drawResult=S_OK;
        hr=Paint(i.borderSurface,UINT(std::ceil(i.borderWidth)),UINT(std::ceil(i.borderHeight)),false,[&](ID2D1DeviceContext*ctx){
            ComPtr<ID2D1SolidColorBrush> brush,clear;
            drawResult=ctx->CreateSolidColorBrush(OutlineColor(accentColor),&brush);if(FAILED(drawResult))return;
            drawResult=ctx->CreateSolidColorBrush(D2D1::ColorF(0,0.f),&clear);if(FAILED(drawResult))return;
            ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            ctx->DrawGeometry(path.Get(),brush.Get(),2*i.Outset());
            // Erase the interior and gap. This creates a true constant-distance
            // offset of the native circular clip, including on transparent windows.
            ctx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            ctx->FillGeometry(path.Get(),clear.Get());ctx->DrawGeometry(path.Get(),clear.Get(),2*i.gap);
        });
        if(SUCCEEDED(hr))hr=drawResult;
        if(SUCCEEDED(hr))hr=i.borderFill->SetContent(i.borderSurface.Get());
        if(SUCCEEDED(hr))i.borderColor=accentColor;
        return SUCCEEDED(hr);
    }
    void PrepareIcon(View&v,Item&i){
        if(i.iconQueried)return;i.iconQueried=true;
        // Read an application's icon once per scene, with a bounded wait for
        // unresponsive windows. Reflow reuses the CPU bitmap, not another visual.
        HICON icon=nullptr;DWORD_PTR result=0;
        bool responsive=SendMessageTimeout(i.source,WM_GETICON,ICON_SMALL,LPARAM(std::lround(96*v.dpi)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,12,&result)!=0;
        if(responsive)icon=reinterpret_cast<HICON>(result);
        if(!icon)icon=reinterpret_cast<HICON>(GetClassLongPtr(i.source,GCLP_HICONSM));
        if(!icon)icon=reinterpret_cast<HICON>(GetClassLongPtr(i.source,GCLP_HICON));
        if(!icon&&responsive&&SendMessageTimeout(i.source,WM_GETICON,ICON_BIG,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,12,&result))
            icon=reinterpret_cast<HICON>(result);
        if(!icon)return;
        HICON owned=CopyIcon(icon);if(!owned)return;
        ComPtr<IWICBitmap> bitmap;HRESULT hr=imageFactory->CreateBitmapFromHICON(owned,&bitmap);DestroyIcon(owned);
        ComPtr<IWICFormatConverter> converter;
        if(SUCCEEDED(hr))hr=imageFactory->CreateFormatConverter(&converter);
        if(SUCCEEDED(hr))hr=converter->Initialize(bitmap.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        if(SUCCEEDED(hr))imageFactory->CreateBitmapFromSource(converter.Get(),WICBitmapCacheOnLoad,&i.iconBitmap);
    }
    bool CreateTitle(View&v,Item&i){
        if(i.minimized){
            // Slide the whole tile from below its display, with only a subtle
            // uniform scale change. Keep the off-screen pose at a useful size.
            constexpr double scale=.94;
            double y=v.bounds.bottom-v.bounds.top;
            MONITORINFO monitor{sizeof(monitor)};
            if(GetMonitorInfo(v.monitor,&monitor))y=std::max(y,double(monitor.rcMonitor.bottom-v.bounds.top));
            i.minimizedOrigin={i.target.cx()-i.target.w*scale*.5,y+12*v.dpi,i.target.w*scale,i.target.h*scale};
        }
        // Only the selected preview needs a border surface. Avoid allocating
        // and rasterizing one full-size transparent bitmap for every window.
        i.borderWidth=float(i.target.w)+2*i.Outset();i.borderHeight=float(i.target.h)+2*i.Outset();
        wchar_t title[512]{};GetWindowText(i.source,title,480);if(!title[0])wcscpy_s(title,L"Window");
        if(i.label&&i.titleText==title&&i.titleWidth==i.target.w&&i.titleRadius==OverviewRadius(v,i.target)&&i.titleDpi==v.dpi){
            i.borderHeight=float(i.target.h)+TitleProtrusion(v,i)+2*i.Outset();return true;
        }
        if(i.label)i.position->RemoveVisual(i.label.Get());
        PrepareIcon(v,i);
        UINT width=UINT(std::max(1.,std::ceil(i.target.w))),height=UINT(std::ceil(24*v.dpi));
        i.titleHeight=float(height);i.titleLeft=i.titleRight=0;
        i.borderHeight=float(i.target.h)+TitleProtrusion(v,i)+2*i.Outset();
        HRESULT hr=Paint(i.labelSurface,width,height,false,[&](ID2D1DeviceContext*ctx){
            ComPtr<IDWriteTextLayout> layout;
            ComPtr<ID2D1SolidColorBrush> textBrush,bgBrush;ComPtr<ID2D1Bitmap1> icon;
            if(i.iconBitmap&&width>=48*v.dpi)ctx->CreateBitmapFromWicBitmap(i.iconBitmap.Get(),nullptr,&icon);
            float iconSize=icon?16*v.dpi:0,iconSpace=icon?iconSize+6*v.dpi:0,padding=8*v.dpi;
            float inset=std::min(16*v.dpi,float(width)*.2f);
            float available=std::max(1.f,float(width)-2*(padding+inset)-iconSpace);
            auto&style=titleStyles[v.dpi];
            if(!style.format){
                if(FAILED(textFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13*v.dpi,L"",&style.format)))return;
                style.format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);style.format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);style.format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                textFactory->CreateEllipsisTrimmingSign(style.format.Get(),&style.ellipsis);
            }
            if(FAILED(textFactory->CreateTextLayout(title,UINT32(wcslen(title)),style.format.Get(),available,float(height),&layout)))return;
            DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};layout->SetTrimming(&trimming,style.ellipsis.Get());
            ctx->CreateSolidColorBrush(D2D1::ColorF(0x20252B),&textBrush);ctx->CreateSolidColorBrush(D2D1::ColorF(0xF2F4F7),&bgBrush);
            if(!textBrush||!bgBrush)return;
            DWRITE_TEXT_METRICS metrics{};layout->GetMetrics(&metrics);float groupWidth=std::min(available,metrics.width)+iconSpace;
            float groupLeft=(float(width)-groupWidth)*.5f,half=std::min(float(width)*.5f-inset,groupWidth*.5f+padding),mid=float(width)*.5f;
            i.titleLeft=mid-half;i.titleRight=mid+half;
            ComPtr<ID2D1PathGeometry> titlePath;
            if(FAILED(TitleGeometry(v,i,0,0,titlePath)))return;
            ctx->FillGeometry(titlePath.Get(),bgBrush.Get());
            if(icon){float top=(float(height)-iconSize)*.5f;ctx->DrawBitmap(icon.Get(),D2D1::RectF(groupLeft,top,groupLeft+iconSize,top+iconSize),1.f,D2D1_INTERPOLATION_MODE_LINEAR);}
            ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);ctx->DrawTextLayout(D2D1::Point2F(groupLeft+iconSpace,0),layout.Get(),textBrush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        });
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.label);
        if(SUCCEEDED(hr))hr=device->CreateEffectGroup(&i.labelEffect);
        if(SUCCEEDED(hr))hr=i.label->SetContent(i.labelSurface.Get());
        if(SUCCEEDED(hr))hr=i.label->SetEffect(i.labelEffect.Get());
        if(SUCCEEDED(hr))hr=i.position->AddVisual(i.label.Get(),FALSE,i.content.Get());
        if(SUCCEEDED(hr)){i.label->SetOffsetY(float(i.target.h-v.dpi));i.labelEffect->SetOpacity(0.f);
            i.titleText=title;i.titleWidth=i.target.w;i.titleRadius=OverviewRadius(v,i.target);i.titleDpi=v.dpi;}
        return SUCCEEDED(hr);
    }
    static float CloseSize(const View&v){return 28*v.dpi;}
    static float CloseInset(const View&v){return 6*v.dpi;}
    static Box CloseBox(const View&v,const Box&window){
        double side=CloseSize(v),inset=CloseInset(v);
        return {window.right()-side-inset,window.y+inset,side,side};
    }
    static bool HasMinimizeButton(const Item&i){return i.canMinimize&&!i.minimized;}
    static float ButtonsWidth(const View&v,const Item&i){return CloseSize(v)+(HasMinimizeButton(i)?CloseSize(v)+6*v.dpi:0);}
    static Box MinimizeBox(const View&v,const Box&window){auto b=CloseBox(v,window);b.x-=CloseSize(v)+6*v.dpi;return b;}
    bool PrepareButtons(View&v,Item&i){
        if(!EnsureDecorations(v,i))return false;
        bool minimize=HasMinimizeButton(i);float width=ButtonsWidth(v,i);
        // Both controls share the existing visual and cached small surfaces.
        // No additional per-preview visual or animation layer is needed.
        for(auto hot:{WindowButton::None,WindowButton::Minimize,WindowButton::Close}){
            if(!minimize&&hot==WindowButton::Minimize)continue;
            auto&surface=v.buttonSurfaces[minimize][int(hot)];if(surface)continue;
            HRESULT drawResult=S_OK;
            HRESULT hr=Paint(surface,UINT(std::ceil(width)),UINT(std::ceil(CloseSize(v))),false,[&](ID2D1DeviceContext*ctx){
                ComPtr<ID2D1SolidColorBrush> background,mark;
                drawResult=ctx->CreateSolidColorBrush(D2D1::ColorF(0x292C32,.98f),&background);if(FAILED(drawResult))return;
                drawResult=ctx->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&mark);if(FAILED(drawResult))return;
                float mid=CloseSize(v)*.5f,d=4*v.dpi;
                ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                auto circle=[&](float x,UINT32 color){background->SetColor(D2D1::ColorF(color,.98f));ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x,mid),mid-.5f*v.dpi,mid-.5f*v.dpi),background.Get());};
                float closeX=width-mid;circle(closeX,hot==WindowButton::Close?0xC42B1C:0x292C32);
                ctx->DrawLine(D2D1::Point2F(closeX-d,mid-d),D2D1::Point2F(closeX+d,mid+d),mark.Get(),1.7f*v.dpi);
                ctx->DrawLine(D2D1::Point2F(closeX-d,mid+d),D2D1::Point2F(closeX+d,mid-d),mark.Get(),1.7f*v.dpi);
                if(minimize){circle(mid,hot==WindowButton::Minimize?0x454A54:0x292C32);ctx->DrawLine(D2D1::Point2F(mid-d,mid),D2D1::Point2F(mid+d,mid),mark.Get(),1.7f*v.dpi);}
            });
            if(FAILED(hr)||FAILED(drawResult)){surface.Reset();return false;}
        }
        i.hotButton=WindowButton::None;return SUCCEEDED(i.buttons->SetContent(v.buttonSurfaces[minimize][0].Get()));
    }
    WindowButton ButtonAt(const View&v,int index,POINT point)const{
        if(index<0||index>=int(v.items.size()))return WindowButton::None;
        const auto&i=v.items[index];if(!i.buttonsAttached||i.source!=selectedSource)return WindowButton::None;
        auto contains=[&](const Box&b){return point.x>=b.x&&point.x<b.right()&&point.y>=b.y&&point.y<b.bottom();};
        Box window=CurrentBox(i);if(contains(CloseBox(v,window)))return WindowButton::Close;
        if(HasMinimizeButton(i)&&contains(MinimizeBox(v,window)))return WindowButton::Minimize;
        return WindowButton::None;
    }
    void HoverButtons(View&v,int index,POINT point){
        bool changed=false,hand=false;
        for(size_t n=0;n<v.items.size();++n){auto&i=v.items[n];auto hot=int(n)==index?ButtonAt(v,int(n),point):WindowButton::None;
            if(hot!=WindowButton::None)hand=true;if(i.hotButton==hot)continue;i.hotButton=hot;
            if(i.buttons)i.buttons->SetContent(v.buttonSurfaces[HasMinimizeButton(i)][int(hot)].Get());changed=true;
        }
        SetCursor(LoadCursor(nullptr,hand?IDC_HAND:IDC_ARROW));if(changed)device->Commit();
    }
    static bool RequestClose(const Item&i){
        DWORD process=0;GetWindowThreadProcessId(i.source,&process);
        if(!IsWindow(i.source)||(i.sourceProcess&&process!=i.sourceProcess))return false;
        // Let the application handle save prompts and cancellation. Never kill it.
        return PostMessage(i.source,WM_SYSCOMMAND,SC_CLOSE,0)!=FALSE;
    }
    static bool RequestMinimize(const Item&i){
        DWORD process=0;GetWindowThreadProcessId(i.source,&process);
        if(!HasMinimizeButton(i)||!IsWindow(i.source)||IsIconic(i.source)||(i.sourceProcess&&process!=i.sourceProcess))return false;
        // The source application handles the request; its native state event
        // moves the retained preview to the shelf after minimization succeeds.
        return PostMessage(i.source,WM_SYSCOMMAND,SC_MINIMIZE,0)!=FALSE;
    }
    void RemoveWindowFromOverview(HWND source){
        if(building){if(std::find(deferredRemovals.begin(),deferredRemovals.end(),source)==deferredRemovals.end())deferredRemovals.push_back(source);return;}
        if(phase==Phase::Hidden)return;
        if(phase==Phase::Closing){End(false);return;}
        bool found=false;for(auto&v:views)for(auto&i:v->items)if(i.source==source)found=true;
        if(!found)return;
        HWND candidate=keyboardSelection?selectedSource:nullptr;
        CaptureMotion();ClearSelection();if(pressedButtonSource==source){pressedButtonSource=nullptr;pressedButton=WindowButton::None;}
        BuildGuard guard(*this);size_t remaining=0;
        for(auto&v:views){
            for(auto&i:v->items)if(i.source==source)v->windows->RemoveVisual(i.position.Get());
            std::erase_if(v->items,[&](const Item&i){return i.source==source;});remaining+=v->items.size();
            if(v->items.empty())continue;
            AnchorCurrentLayout(*v);
            auto layout=Arrange(*v);
            if(layout.size()!=v->items.size()){End(false);return;}
            for(size_t n=0;n<layout.size();++n){auto&i=v->items[n];i.target=layout[n];if(i.borderFill)i.borderFill->SetContent(nullptr);i.borderSurface.Reset();
                if(!CreateTitle(*v,i)){End(false);return;}
            }
        }
        if(!remaining){End(true);return;}
        RememberLayout();FollowNativeZOrder();progress=1;Animate(1,true,true);
        RestoreKeyboardSelection(candidate);
    }
    void Cleanup(){
        taskbarOrder.Stop();
        if(controller){RemoveProp(controller,kShellReadyProperty);RemoveProp(controller,kNativeBypassProperty);}
        if(uiSettings&&accentSubscribed)uiSettings->remove_ColorValuesChanged(accentToken);
        accentSubscribed=false;uiSettings.Reset();
        if(lowMemoryEvent){CloseHandle(lowMemoryEvent);lowMemoryEvent=nullptr;}
        End(false,false);
        if(controller){UnregisterHotKey(controller,2);g_controller.store(nullptr);DestroyWindow(controller);controller=nullptr;}
        if(timer){CloseHandle(timer);timer=nullptr;}ReleaseDevice();desktops.Reset();UnregisterClass(kOverlay,g_instance);UnregisterClass(kController,g_instance);
    }
    void Schedule(double delay){LARGE_INTEGER due;due.QuadPart=-std::max<LONGLONG>(1,LONGLONG(delay*10000000));SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);}
    void Loop(){
        for(;;){
            // Do not wait on an already-signaled pressure event after recording
            // it, or after releasing graphics: that would spin while memory is low.
            bool watch=settings.memoryMode==MemoryMode::Automatic&&device&&!memoryPressure;
            HANDLE handles[]{g_stopEvent,timer,lowMemoryEvent};DWORD count=watch&&lowMemoryEvent?3:2;
            DWORD timeout=watch&&!lowMemoryEvent?5000:INFINITE;
            DWORD r=MsgWaitForMultipleObjectsEx(count,handles,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(r==WAIT_OBJECT_0||r==WAIT_FAILED)break;
            if(r==WAIT_OBJECT_0+1){if(idleTimer&&phase==Phase::Hidden)TrimIdleResources();else FinishTransition();}
            if((count==3&&r==WAIT_OBJECT_0+2)||(r==WAIT_TIMEOUT&&LowMemory()))MemoryBecameLow();
            MSG msg;for(int n=0;n<64&&PeekMessage(&msg,nullptr,0,0,PM_REMOVE);++n){if(msg.message==WM_QUIT)return;TranslateMessage(&msg);DispatchMessage(&msg);}
        }
    }
    static BOOL CALLBACK AddMonitor(HMONITOR monitor,HDC,LPRECT,LPARAM param){
        auto&a=*reinterpret_cast<App*>(param);MONITORINFO mi{sizeof(mi)};if(!GetMonitorInfo(monitor,&mi))return TRUE;
        auto v=std::make_unique<View>();v->monitor=monitor;v->bounds=mi.rcWork;a.views.push_back(std::move(v));return TRUE;
    }
    static bool Bounds(HWND hwnd,RECT&r){
        if(!IsIconic(hwnd))return SUCCEEDED(DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))||GetWindowRect(hwnd,&r);
        // Iconic bounds describe the taskbar icon, sometimes at (-32000,-32000).
        // Use the saved restore placement only for layout, without restoring it.
        WINDOWPLACEMENT placement{sizeof(placement)};MONITORINFO monitor{sizeof(monitor)};
        if(!GetWindowPlacement(hwnd,&placement)||!GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor))return false;
        r=placement.rcNormalPosition;
        if(placement.flags&WPF_RESTORETOMAXIMIZED)r=monitor.rcWork;
        else if(!(GetWindowLongPtr(hwnd,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))
            OffsetRect(&r,monitor.rcWork.left-monitor.rcMonitor.left,monitor.rcWork.top-monitor.rcMonitor.top);
        return r.right>r.left&&r.bottom>r.top;
    }
    static void AssignTaskbarOrder(View&v,const std::vector<TaskbarEntry>&order){
        for(auto&i:v.items){i.taskbarRank=INT_MAX;
            // Prefer the taskbar on this monitor, then the main taskbar if it
            // shows all monitors. Group rank keeps sibling windows together.
            for(size_t n=0;n<order.size();++n)if(order[n].group==i.groupKey)
                i.taskbarRank=std::min(i.taskbarRank,int(n)+(order[n].monitor==v.monitor?0:10000));
        }
    }
    void ApplyTaskbarOrder(View&v){AssignTaskbarOrder(v,taskbarOrder.Read());}
    void TaskbarChanged(){
        if(phase!=Phase::Opening&&phase!=Phase::Settled)return;
        bool changed=false;for(auto&v:views){ApplyTaskbarOrder(*v);auto layout=Arrange(*v);
            for(size_t n=0;n<layout.size();++n)if(std::abs(layout[n].x-v->items[n].target.x)>.1||std::abs(layout[n].y-v->items[n].target.y)>.1)changed=true;}
        if(changed)Relayout();
    }
    void Relayout(){
#ifdef SCATTER_TEST
        ++layoutPasses;
#endif
        HWND candidate=keyboardSelection?selectedSource:nullptr;
        progress=CurrentProgress();CaptureMotion();ClearSelection();BuildGuard guard(*this);
        for(auto&v:views){AnchorCurrentLayout(*v);auto layout=Arrange(*v);if(layout.size()!=v->items.size()){End(false);return;}
            for(size_t n=0;n<layout.size();++n){auto&i=v->items[n];i.target=layout[n];if(i.borderFill)i.borderFill->SetContent(nullptr);i.borderSurface.Reset();if(!CreateTitle(*v,i)){End(false);return;}}}
        RememberLayout();FollowNativeZOrder();Animate(1,true,true);
        RestoreKeyboardSelection(candidate);
    }
    void SceneChanged(HWND source,bool deferLayout=false){
        if(phase!=Phase::Opening&&phase!=Phase::Settled)return;
        for(auto&v:views)for(auto&i:v->items)if(i.source==source){
            if(i.minimized!=(IsIconic(source)!=FALSE)){ReconcileWindowStates();return;}
            RECT r{};if(i.minimized||!Bounds(source,r))return;
            Box actual{double(r.left-v->bounds.left),double(r.top-v->bounds.top),double(r.right-r.left),double(r.bottom-r.top)};
            bool resize=std::abs(actual.w-i.original.w)>.5||std::abs(actual.h-i.original.h)>.5;
            bool moved=std::abs(actual.x-i.original.x)>.5||std::abs(actual.y-i.original.y)>.5;
            i.original=actual;i.nativeRadius=SystemCornerRadius(source,v->dpi);
            if(resize)RebindRestoredSample(*v,i,r);
            if(resize||moved){if(deferLayout)sceneNeedsLayout=true;else Relayout();}return;
        }
        if(!IsWindowVisible(source)||GetAncestor(source,GA_ROOT)!=source)return;
        BuildGuard guard(*this);std::vector<size_t> counts;for(auto&v:views)counts.push_back(v->items.size());AddWindow(source,reinterpret_cast<LPARAM>(this));
        bool added=false;
        for(size_t n=0;n<views.size();++n){auto&v=*views[n];if(v.items.size()==counts[n])continue;
            if(!v.hwnd){if(!CreateView(v)||!CreateDesktop(v)){End(false);return;}}
            auto&i=v.items.back();if(!CreateItem(v,i)){v.items.pop_back();continue;}
            ApplyTaskbarOrder(v);auto layout=Arrange(v);if(layout.size()!=v.items.size()){End(false);return;}
            i.target=layout.back();if(!CreateTitle(v,i)){End(false);return;}
            i.motionFrom=HiddenPose(i);i.radiusFrom=i.radiusTo=HiddenRadius(v,i);i.labelFrom=i.labelTo=0;i.labelSeconds=0;
            // CaptureMotion reads motion endpoints for the newly attached item.
            i.motionTo=i.motionFrom;i.target=i.motionFrom;added=true;
        }
        if(added){taskbarOrder.Refresh();Relayout();if(phase!=Phase::Hidden){device->WaitForCommitCompletion();for(auto&v:views)if(v->hwnd&&!IsWindowVisible(v->hwnd))ShowWindow(v->hwnd,SW_SHOWNOACTIVATE);}}
    }
    void RememberLayout(){
        for(auto&v:views)for(auto&i:v->items){i.layoutAnchor=i.target;i.anchorMinimized=i.minimized;i.anchorSlot=i.layoutSlot;}
    }
    void AnchorCurrentLayout(View&v){
        if(phase!=Phase::Opening)return;
        int count=0;for(auto&i:v.items)count+=!i.minimized;
        bool changed=false;std::vector<Item*> retained;double caption=26*v.dpi;
        for(auto&i:v.items)if(!i.minimized){
            if(i.anchorMinimized||i.layoutAnchor.w<=0){changed=true;continue;}
            const auto&p=i.anchorSlot;
            changed|=p.count!=count||std::abs(p.original.w-i.original.w)>.01||std::abs(p.original.h-i.original.h)>.01;
            // Initial expansion can still contain overlapping source windows.
            // Only use a captured pose once its retained previews are separated.
            auto a=i.motionFrom;a.h+=caption;
            for(auto*j:retained){auto b=j->motionFrom;b.h+=caption;if(Overlap(a,b))return;}
            retained.push_back(&i);
        }
        if(changed)for(auto*i:retained)i->layoutAnchor=i->motionFrom;
    }
    static std::vector<Box> Arrange(View&v,bool liveReflow=true){
        double pad=28*v.dpi,gap=12*v.dpi,caption=26*v.dpi;
        Box area{pad,pad,std::max(1.,double(v.bounds.right-v.bounds.left)-2*pad),std::max(1.,double(v.bounds.bottom-v.bounds.top)-2*pad)};
        std::vector<int> normal,small;std::vector<Box> sources,anchors,result(v.items.size());
        for(size_t n=0;n<v.items.size();++n)(v.items[n].minimized?small:normal).push_back(int(n));
        std::stable_sort(small.begin(),small.end(),[&](int a,int b){
            const auto&x=v.items[a];const auto&y=v.items[b];bool oldX=liveReflow&&x.anchorMinimized&&x.layoutAnchor.w>0,oldY=liveReflow&&y.anchorMinimized&&y.layoutAnchor.w>0;
            if(x.taskbarRank!=y.taskbarRank)return x.taskbarRank<y.taskbarRank;
            if(x.groupKey!=y.groupKey)return x.groupKey<y.groupKey;
            if(oldX!=oldY)return oldX;return oldX&&x.layoutAnchor.x<y.layoutAnchor.x;
        });
        if(!small.empty()){
            // Keep the shelf in one shallow row. Crowding reduces horizontal
            // gaps first, then uniformly scales tiles; it never adds rows.
            double height=std::min(90.*v.dpi,std::max(1.,area.h*.3-caption)),totalWidth=0,rowHeight=0;
            for(int id:small){auto b=v.items[id].original;double scale=std::min({.78,height/b.h,160.*v.dpi/b.w});b.w*=scale;b.h*=scale;result[id]=b;totalWidth+=b.w;}
            double gaps=double(small.size()-1);
            if(gaps>0)gap=std::clamp((area.w-totalWidth)/gaps,4.*v.dpi,gap);
            double scale=std::min(1.,std::max(1.,area.w-gap*gaps)/totalWidth);
            // Even unusually large counts stay inside the available width.
            if(gap*gaps>=area.w){gap=area.w/(2*small.size());scale=(area.w-gap*gaps)/totalWidth;}
            // Keep only a small clearance below the caption's selection outline;
            // the reclaimed height belongs to the main layout above the shelf.
            double x=area.cx()-(totalWidth*scale+gap*gaps)*.5,bottom=double(v.bounds.bottom-v.bounds.top)-8*v.dpi-caption;
            for(int id:small){auto&b=result[id];b.w*=scale;b.h*=scale;b.x=x;b.y=bottom-b.h;x+=b.w+gap;rowHeight=std::max(rowHeight,b.h);}
            double shelfTop=bottom-rowHeight;
            area.h=std::max(1.,shelfTop-area.y-24*v.dpi);
        }
        std::vector<LayoutSlot> previous,next;
        for(int id:normal){sources.push_back(v.items[id].original);anchors.push_back(v.items[id].anchorMinimized?Box{}:v.items[id].layoutAnchor);previous.push_back(v.items[id].anchorSlot);}
        auto layout=StableLayout(sources,anchors,area,22*v.dpi,caption,previous,&next,liveReflow);if(layout.size()!=normal.size())return {};
        for(size_t n=0;n<normal.size();++n){result[normal[n]]=layout[n];v.items[normal[n]].layoutSlot=next[n];}
        return result;
    }
    static Box HiddenPose(const Item&i){return i.minimized?i.minimizedOrigin:i.original;}
    static float HiddenRadius(const View&v,const Item&i){return i.minimized?OverviewRadius(v,i.minimizedOrigin):i.nativeRadius;}
    Box ExitPose(const Item&i)const{return i.minimized&&i.source!=chosen?i.minimizedOrigin:i.original;}
    float ExitRadius(const View&v,const Item&i)const{return i.minimized&&i.source!=chosen?HiddenRadius(v,i):i.nativeRadius;}
    static void ActivateWindow(HWND target,bool restoreMinimized){
        if(!IsWindow(target))return;
        if(!restoreMinimized&&IsIconic(target))return;
        if(restoreMinimized&&IsIconic(target)){
            WINDOWPLACEMENT placement{sizeof(placement)};int show=SW_RESTORE;
            if(GetWindowPlacement(target,&placement)&&(placement.flags&WPF_RESTORETOMAXIMIZED))show=SW_SHOWMAXIMIZED;
            ShowWindowAsync(target,show);
        }
        if(GetForegroundWindow()!=target)SetForegroundWindow(target);
    }
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
        auto&a=*reinterpret_cast<App*>(param);if(!IsWindowVisible(hwnd))return TRUE;
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
        for(auto&v:a.views)if(v->monitor==mon){Item i;i.source=hwnd;i.minimized=IsIconic(hwnd)!=FALSE;i.original={double(bounds.left-v->bounds.left),double(bounds.top-v->bounds.top),double(bounds.right-bounds.left),double(bounds.bottom-bounds.top)};v->items.push_back(std::move(i));break;}return TRUE;
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
    HRESULT CreateMinimizedPlaceholder(Item&i){
        // Rare fallback for applications without a retained DWM surface. One
        // small shared card keeps every minimized window selectable, with its
        // actual title below; no window capture or source activation is needed.
        HRESULT hr=S_OK;
        if(!minimizedPlaceholder)hr=Paint(minimizedPlaceholder,160,100,false,[](ID2D1DeviceContext*ctx){
            ctx->Clear(D2D1::ColorF(0x29313D));
            ComPtr<ID2D1SolidColorBrush> brush;
            if(SUCCEEDED(ctx->CreateSolidColorBrush(D2D1::ColorF(0xAAB5C4,.8f),&brush))){
                ctx->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(61,35,99,65),3,3),brush.Get(),1.5f);
                ctx->DrawLine(D2D1::Point2F(62,43),D2D1::Point2F(98,43),brush.Get(),1.5f);
                ctx->DrawLine(D2D1::Point2F(75,57),D2D1::Point2F(85,57),brush.Get(),1.5f);
            }
        });
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.surface);
        if(SUCCEEDED(hr))hr=i.surface->SetContent(minimizedPlaceholder.Get());
        i.pixels={160,100};return hr;
    }
    bool CreateItem(View&v,Item&i){
        GetWindowThreadProcessId(i.source,&i.sourceProcess);
        i.groupKey=WindowGroupKey(i.source);
        i.minimized=IsIconic(i.source)!=FALSE;
        HRESULT hr=api.querySize(i.source,FALSE,&i.pixels);
        bool validSize=SUCCEEDED(hr)&&i.pixels.cx>0&&i.pixels.cy>0;
        if(!validSize&&!i.minimized)return false;
        DWM_THUMBNAIL_PROPERTIES p{};
        p.dwFlags=DWM_TNP_VISIBLE|DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY|DWM_TNP_RECTDESTINATION|DWM_TNP_RECTSOURCE|0x04000000;
        // DWM already knows the retained surface's visible frame. Current iconic
        // window coordinates cannot crop that old surface without cutting edges.
        if(i.minimized)p.dwFlags&=~DWM_TNP_RECTSOURCE;
        p.fVisible=TRUE;p.opacity=255;p.fSourceClientAreaOnly=FALSE;
        // DWM preserves per-pixel alpha and color keys, but the explicit
        // thumbnail opacity replaces a layered source's whole-window alpha.
        COLORREF key=0;BYTE alpha=255;DWORD flags=0;
        if(GetLayeredWindowAttributes(i.source,&key,&alpha,&flags)&&(flags&LWA_ALPHA))p.opacity=alpha;
        RECT outer{},frame{};p.rcSource={0,0,i.pixels.cx,i.pixels.cy};
        if(!i.minimized&&GetWindowRect(i.source,&outer)&&SUCCEEDED(DwmGetWindowAttribute(i.source,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))))
            p.rcSource=SourceRect(outer,frame,i.pixels);
        // Normalize the sampled visible frame to a zero-based destination.
        i.pixels={p.rcSource.right-p.rcSource.left,p.rcSource.bottom-p.rcSource.top};
        if(i.minimized&&validSize){
            double cx=i.original.cx(),cy=i.original.cy();
            i.original={cx-i.pixels.cx*.5,cy-i.pixels.cy*.5,double(i.pixels.cx),double(i.pixels.cy)};
        }
        p.rcDestination={0,0,i.pixels.cx,i.pixels.cy};
        hr=validSize?api.createThumb(v.hwnd,i.source,2,&p,device.Get(),reinterpret_cast<void**>(i.surface.GetAddressOf()),&i.thumbnail):E_FAIL;
        if(FAILED(hr)&&i.minimized){i.surface.Reset();i.thumbnail=nullptr;hr=CreateMinimizedPlaceholder(i);}
        if(FAILED(hr)){Log(L"Create shared source",hr);return false;}
        // DWM's shared visual may reject rendering-mode setters. Apply them to
        // our own parent visual; descendants inherit the composition settings.
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.position);
        if(SUCCEEDED(hr))hr=device->CreateVisual(&i.content);
        if(SUCCEEDED(hr))hr=device->CreateScaleTransform(&i.scale);
        if(SUCCEEDED(hr))hr=device->CreateRectangleClip(&i.clip);
        if(SUCCEEDED(hr))hr=i.content->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
        if(SUCCEEDED(hr))hr=i.content->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
        if(FAILED(hr)){Log(L"Create window surface",hr);return false;}
        i.stroke=3*v.dpi;i.gap=4*v.dpi;
        i.surface->SetTransform(i.scale.Get());i.content->AddVisual(i.surface.Get(),TRUE,nullptr);i.content->SetClip(i.clip.Get());
        i.position->AddVisual(i.content.Get(),TRUE,nullptr);
        i.canMinimize=(GetWindowLongPtr(i.source,GWL_STYLE)&WS_MINIMIZEBOX)!=0;
        i.nativeRadius=SystemCornerRadius(i.source,v.dpi);
        i.clip->SetLeft(0.f);i.clip->SetTop(0.f);
        return true;
    }
    bool Begin(){
        if(phase!=Phase::Hidden||building)return false;
        CancelWaitableTimer(timer);idleTimer=false;
        previous=GetForegroundWindow();chosen=nullptr;progress=0;BuildGuard guard(*this);RefreshAccent();taskbarOrder.Start(controller);taskbarOrder.Refresh();
        memoryPressure=settings.memoryMode==MemoryMode::Automatic&&LowMemory();
        if(!EnsureDevice()){building=false;return false;}
        if(!desktops)CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&desktops));
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
            ApplyTaskbarOrder(*v);auto layout=Arrange(*v,false);
            if(layout.size()!=v->items.size()){building=false;End(false);return false;}
            for(size_t j=0;j<layout.size();++j){v->items[j].target=layout[j];if(!CreateTitle(*v,v->items[j])){building=false;End(false);return false;}}
            count+=v->items.size();
        }
        if(!count){building=false;End(false);return false;}
        RememberLayout();
        FollowNativeZOrder();
        selectedSource=nullptr;keyboardSelection=false;
        GetCursorPos(&lastPointer);
        for(auto&v:views)if(v->hwnd&&!CreateDesktop(*v)){building=false;End(false);return false;}
        StaticPose(0);
        if(FAILED(device->Commit())){building=false;End(false);return false;}
        device->WaitForCommitCompletion();
        phase=Phase::Opening;SetProp(controller,kOverviewProperty,reinterpret_cast<HANDLE>(kShellAck));
        POINT cursor{};GetCursorPos(&cursor);HMONITOR active=MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);HWND focus=nullptr;
        for(auto&v:views)if(v->hwnd){ShowWindow(v->hwnd,SW_SHOWNOACTIVATE);if(!focus||v->monitor==active)focus=v->hwnd;}
        if(focus){SetForegroundWindow(focus);SetFocus(focus);}
        foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT);
        destroyHook=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_HIDE,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        windowStateHook=SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND,nullptr,WindowStateEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        geometryHook=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,WindowStateEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        building=false;Animate(1);return true;
    }
    template<class Setter> void Property(double a,double b,Setter setter,double duration=-1){
        if(duration<0)duration=seconds;
        if(duration<=0||std::abs(b-a)<.00001){setter(float(b));return;}
        std::array<double,3> key{a,b,duration};
        if(auto found=animationIndex.find(key);found!=animationIndex.end()){setter(animations[found->second].animation.Get());return;}
        auto animation=MakeAnimation(a,b,duration);if(!animation){setter(float(b));return;}
        setter(animation.Get());animationIndex.emplace(key,animations.size());animations.push_back({a,b,duration,std::move(animation)});
    }
    ComPtr<IDCompositionAnimation> MakeAnimation(double a,double b,double duration){
        ComPtr<IDCompositionAnimation> animation;if(FAILED(device->CreateAnimation(&animation)))return {};
        // Twelve Hermite cubics approximate critically damped motion. All share
        // an absolute QPC start so position, scale and rectangular bounds stay in sync.
        constexpr int parts=12;double dt=duration/parts,delta=b-a;
        for(int j=0;j<parts;++j){
            double u=double(j)/parts,v=double(j+1)/parts;
            double f0=a+delta*Curve(u),f1=a+delta*Curve(v);
            double m0=delta*CurveDerivative(u)/duration,m1=delta*CurveDerivative(v)/duration;
            double c2=(3*(f1-f0)/dt-2*m0-m1)/dt,c3=(2*(f0-f1)/dt+m0+m1)/(dt*dt);
            animation->AddCubic(j*dt,float(f0),float(m0),float(c2),float(c3));
        }
        animation->End(duration,float(b));return animation;
    }
    void SyncDecorations(View&v,Item&i){
        Box a=phase==Phase::Settled?i.target:i.motionFrom,b=phase==Phase::Settled?i.target:i.motionTo;
        auto property=[&](double from,double to,auto setter){
            if(phase==Phase::Settled||seconds<=0||std::abs(to-from)<.00001){setter(float(to));return;}
            auto animation=MakeAnimation(from,to,seconds);if(!animation){setter(float(to));return;}
            LARGE_INTEGER begin{};begin.QuadPart=animationStart;animation->SetAbsoluteBeginTime(begin);setter(animation.Get());
        };
        // A decoration first hovered during motion joins the existing absolute
        // timeline; it must not jump to the destination or restart the scene.
        property((a.w+2*i.Outset())/i.borderWidth,(b.w+2*i.Outset())/i.borderWidth,[&](auto value){i.borderScale->SetScaleX(value);});
        property((a.h+TitleProtrusion(v,i)+2*i.Outset())/i.borderHeight,(b.h+TitleProtrusion(v,i)+2*i.Outset())/i.borderHeight,[&](auto value){i.borderScale->SetScaleY(value);});
        property(a.w-ButtonsWidth(v,i)-CloseInset(v),b.w-ButtonsWidth(v,i)-CloseInset(v),[&](auto value){i.buttons->SetOffsetX(value);});
    }
    void StaticPose(double p){
        for(auto&v:views)for(auto&i:v->items){
            Box b=Mix(ExitPose(i),i.target,p);i.position->SetOffsetX(float(b.x));i.position->SetOffsetY(float(b.y));
            i.scale->SetScaleX(float(b.w/i.pixels.cx));i.scale->SetScaleY(float(b.h/i.pixels.cy));
            i.clip->SetRight(float(b.w));i.clip->SetBottom(float(b.h));
            if(i.borderScale){i.borderScale->SetScaleX(float(b.w+2*i.Outset())/i.borderWidth);i.borderScale->SetScaleY(float(b.h+TitleProtrusion(*v,i)+2*i.Outset())/i.borderHeight);}
            if(i.buttons)i.buttons->SetOffsetX(float(b.w)-ButtonsWidth(*v,i)-CloseInset(*v));
            if(i.label){i.label->SetOffsetX(float((b.w-i.target.w)*.5));i.label->SetOffsetY(float(b.h-v->dpi));i.labelEffect->SetOpacity(float(p));}
            i.labelFrom=i.labelTo=p;i.labelSeconds=0;
            float radius=float(ExitRadius(*v,i)+(OverviewRadius(*v,i.target)-ExitRadius(*v,i))*p);
            SetCorners(i,radius);i.radiusFrom=i.radiusTo=radius;i.motionFrom=i.motionTo=b;
        }
    }
    double MotionFraction()const{return Curve(seconds<=0?1:(Now()-start)/seconds);}
    double CurrentLabelOpacity(const Item&i)const{
        return i.labelFrom+(i.labelTo-i.labelFrom)*Curve(i.labelSeconds<=0?1:(Now()-start)/i.labelSeconds);
    }
    Box CurrentBox(const Item&i)const{
        if(phase==Phase::Hidden)return HiddenPose(i);if(phase==Phase::Settled)return i.target;
        return Mix(i.motionFrom,i.motionTo,MotionFraction());
    }
    void CaptureMotion(){
        double t=MotionFraction();
        for(auto&v:views)for(auto&i:v->items){
            i.radiusFrom=phase==Phase::Hidden?HiddenRadius(*v,i):phase==Phase::Settled?OverviewRadius(*v,i.target):float(i.radiusFrom+(i.radiusTo-i.radiusFrom)*t);
            i.motionFrom=CurrentBox(i);i.labelFrom=CurrentLabelOpacity(i);
        }
    }
    double CurrentProgress(){
        if(phase==Phase::Settled)return 1;
        if(phase==Phase::Hidden)return 0;
        return from+(to-from)*Curve(seconds<=0?1:(Now()-start)/seconds);
    }
    void Animate(double destination,bool captured=false,bool reflow=false){
        if(!captured)CaptureMotion();animations.clear();animationIndex.clear();
        from=progress;to=destination;
        BOOL enabled=TRUE;SystemParametersInfo(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);
        seconds=(enabled?settings.duration:0)/1000.0*(reflow?1.:std::abs(to-from));
        phase=to==1?Phase::Opening:Phase::Closing;
        for(auto&v:views)for(auto&i:v->items){
            Box a=i.motionFrom,b=destination==1?i.target:ExitPose(i);i.motionTo=b;
            i.radiusTo=destination==1?OverviewRadius(*v,i.target):ExitRadius(*v,i);
            i.labelTo=destination;i.labelSeconds=(seconds+std::min(.12,seconds*.28))*.5;
            Property(a.x,b.x,[&](auto value){i.position->SetOffsetX(value);});
            Property(a.y,b.y,[&](auto value){i.position->SetOffsetY(value);});
            Property(a.w/i.pixels.cx,b.w/i.pixels.cx,[&](auto value){i.scale->SetScaleX(value);});
            Property(a.h/i.pixels.cy,b.h/i.pixels.cy,[&](auto value){i.scale->SetScaleY(value);});
            Property(a.w,b.w,[&](auto value){i.clip->SetRight(value);});
            Property(a.h,b.h,[&](auto value){i.clip->SetBottom(value);});
            Property(i.radiusFrom,i.radiusTo,[&](auto value){SetCorners(i,value);});
            if(i.borderAttached){
                Property((a.w+2*i.Outset())/i.borderWidth,(b.w+2*i.Outset())/i.borderWidth,[&](auto value){i.borderScale->SetScaleX(value);});
                Property((a.h+TitleProtrusion(*v,i)+2*i.Outset())/i.borderHeight,(b.h+TitleProtrusion(*v,i)+2*i.Outset())/i.borderHeight,[&](auto value){i.borderScale->SetScaleY(value);});
            }
            if(i.buttonsAttached)Property(a.w-ButtonsWidth(*v,i)-CloseInset(*v),b.w-ButtonsWidth(*v,i)-CloseInset(*v),[&](auto value){i.buttons->SetOffsetX(value);});
            if(i.label){
                Property((a.w-i.target.w)*.5,(b.w-i.target.w)*.5,[&](auto value){i.label->SetOffsetX(value);});
                Property(a.h-v->dpi,b.h-v->dpi,[&](auto value){i.label->SetOffsetY(value);});
                Property(i.labelFrom,i.labelTo,[&](auto value){i.labelEffect->SetOpacity(value);},i.labelSeconds);
            }
        }
        // Set one start AFTER building the batch. Expensive submission must not
        // consume the beginning of the animation. Use the compositor's actual
        // next-frame estimate instead of assuming a 120 Hz display.
        LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);animationStart=qpc.QuadPart;
        DCOMPOSITION_FRAME_STATISTICS stats{};
        if(SUCCEEDED(device->GetFrameStatistics(&stats))){
            if(stats.currentCompositionRate.Numerator&&stats.currentCompositionRate.Denominator)
                frameSeconds=std::clamp(double(stats.currentCompositionRate.Denominator)/stats.currentCompositionRate.Numerator,1./360,.1);
            if(stats.nextEstimatedFrameTime.QuadPart>=qpc.QuadPart&&stats.nextEstimatedFrameTime.QuadPart-qpc.QuadPart<frequency.QuadPart/10)
                animationStart=stats.nextEstimatedFrameTime.QuadPart;
        }
        start=double(animationStart)/frequency.QuadPart;LARGE_INTEGER begin{};begin.QuadPart=animationStart;
        for(auto&e:animations)e.animation->SetAbsoluteBeginTime(begin);
        HRESULT hr=device->Commit();if(FAILED(hr)){Log(L"Animate",hr);End(true);return;}
        animations.clear();animationIndex.clear();
        if(seconds<=0){FinishTransition();return;}
        Schedule(std::max(0.,start+seconds-Now())+frameSeconds);
    }
    void FinishTransition(){
        if(phase!=Phase::Opening&&phase!=Phase::Closing)return;
        if(phase==Phase::Closing)RefreshRestoredGeometry(false);
        progress=to;StaticPose(to);device->Commit();
        if(phase==Phase::Closing){End(true);return;}
        phase=Phase::Settled;
    }
    static BOOL CALLBACK CollectZOrder(HWND hwnd,LPARAM p){reinterpret_cast<std::vector<HWND>*>(p)->push_back(hwnd);return TRUE;}
    void FollowNativeZOrder(HWND preferred=nullptr){
        std::vector<HWND> native;
        EnumWindows(CollectZOrder,reinterpret_cast<LPARAM>(&native));
        if(preferred){std::erase(native,preferred);auto position=native.begin();
            if(!(GetWindowLongPtr(preferred,GWL_EXSTYLE)&WS_EX_TOPMOST))position=std::find_if(native.begin(),native.end(),[](HWND w){return !(GetWindowLongPtr(w,GWL_EXSTYLE)&WS_EX_TOPMOST);});
            native.insert(position,preferred);
        }
        for(auto&v:views)if(v->windows){
            std::unordered_map<HWND,int> indices;indices.reserve(v->items.size());for(size_t n=0;n<v->items.size();++n)indices.emplace(v->items[n].source,int(n));
            std::vector<HWND> order;order.reserve(v->items.size());v->hitOrder.clear();
            for(HWND w:native)if(auto found=indices.find(w);found!=indices.end()){order.push_back(w);v->hitOrder.push_back(found->second);}
            if(order==v->visualOrder)continue;
            v->windows->RemoveAllVisuals();
            for(auto n=v->hitOrder.rbegin();n!=v->hitOrder.rend();++n)v->windows->AddVisual(v->items[*n].position.Get(),FALSE,nullptr);
            v->visualOrder=std::move(order);
        }
    }
    bool RebindRestoredSample(View&v,Item&i,const RECT&r){
        // An iconic thumbnail starts with DWM's implicit retained crop.
        // When its source becomes live again, explicitly sample the restored
        // visible frame. Keep the existing destination normalization so the
        // in-flight compositor scale does not jump with the source size.
        RECT outer{};SIZE size{};
        bool rebound=false;
        if(GetWindowRect(i.source,&outer)&&SUCCEEDED(api.querySize(i.source,FALSE,&size))&&size.cx>0&&size.cy>0){
            RECT sample=SourceRect(outer,r,size);
            if(!i.restoredSampleValid||!EqualRect(&sample,&i.restoredSample)){
                DWM_THUMBNAIL_PROPERTIES properties{};
                properties.dwFlags=DWM_TNP_RECTSOURCE|DWM_TNP_RECTDESTINATION|DWM_TNP_VISIBLE|DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY|0x04000000;
                properties.rcSource=sample;properties.rcDestination={0,0,i.pixels.cx,i.pixels.cy};
                properties.fVisible=TRUE;properties.opacity=255;
                COLORREF key=0;BYTE alpha=255;DWORD flags=0;
                if(GetLayeredWindowAttributes(i.source,&key,&alpha,&flags)&&(flags&LWA_ALPHA))properties.opacity=alpha;
                // Shared DComp thumbnails cannot use the public update API.
                // Replace only their source visual in the same commit; the
                // animated transform, clip and position remain attached.
                ComPtr<IDCompositionVisual2> surface;HTHUMBNAIL thumbnail=nullptr;
                HRESULT hr=api.createThumb(v.hwnd,i.source,2,&properties,device.Get(),reinterpret_cast<void**>(surface.GetAddressOf()),&thumbnail);
                if(SUCCEEDED(hr))hr=surface->SetTransform(i.scale.Get());
                if(SUCCEEDED(hr))hr=i.content->AddVisual(surface.Get(),FALSE,i.surface.Get());
                if(SUCCEEDED(hr)){
                    i.surface->SetTransform(nullptr);i.content->RemoveVisual(i.surface.Get());i.surface=std::move(surface);i.thumbnail=thumbnail;
                    i.restoredSample=sample;i.restoredSampleValid=true;rebound=true;
                }else{if(surface)surface->SetTransform(nullptr);Log(L"Rebind restored thumbnail sampling",hr);}
            }
        }
        return rebound;
    }
    bool ReconcileWindowStates(bool freshLayout=false){
        bool changed=false;
        for(auto&v:views)for(auto&i:v->items)if(IsWindow(i.source)&&(freshLayout||i.minimized!=(IsIconic(i.source)!=FALSE))){
            bool minimized=IsIconic(i.source)!=FALSE;
            RECT r{};if(!Bounds(i.source,r))continue;
            if(i.minimized==minimized){
                if(freshLayout&&!minimized){
                    Box current{double(r.left-v->bounds.left),double(r.top-v->bounds.top),double(r.right-r.left),double(r.bottom-r.top)};
                    if(std::abs(current.w-i.original.w)>.5||std::abs(current.h-i.original.h)>.5)RebindRestoredSample(*v,i,r);
                    i.original=current;i.nativeRadius=SystemCornerRadius(i.source,v->dpi);
                }
                continue;
            }
            if(!changed&&!freshLayout){progress=CurrentProgress();CaptureMotion();}
            changed=true;
            if(!minimized){
                RebindRestoredSample(*v,i,r);
                i.original={double(r.left-v->bounds.left),double(r.top-v->bounds.top),double(r.right-r.left),double(r.bottom-r.top)};
                i.nativeRadius=SystemCornerRadius(i.source,v->dpi);
            }else i.restoredSampleValid=false;
            i.minimized=minimized;
        }
        if(!changed&&!freshLayout)return false;
        if(restoreSource&&!IsIconic(restoreSource)){
            if(restoreHook){UnhookWinEvent(restoreHook);restoreHook=nullptr;}restoreSource=nullptr;
        }
        HWND candidate=keyboardSelection?selectedSource:nullptr;
        ClearSelection();BuildGuard guard(*this);bool targetsChanged=false;
        for(auto&v:views){
            if(freshLayout)ApplyTaskbarOrder(*v);
            else AnchorCurrentLayout(*v);
            auto layout=Arrange(*v,!freshLayout);
            if(layout.size()!=v->items.size()){End(false);return true;}
            for(size_t n=0;n<layout.size();++n){auto&i=v->items[n];auto target=layout[n];
                targetsChanged|=std::abs(target.x-i.target.x)>.01||std::abs(target.y-i.target.y)>.01||std::abs(target.w-i.target.w)>.01||std::abs(target.h-i.target.h)>.01;
                i.target=target;if(i.borderFill)i.borderFill->SetContent(nullptr);i.borderSurface.Reset();
                if(!CreateTitle(*v,i)){End(false);return true;}
            }
        }
        RememberLayout();FollowNativeZOrder();
        // Layout and title preparation may take time while DWM keeps closing.
        // Capture afterwards so the new animation never rewinds to an old pose.
        if(freshLayout){progress=CurrentProgress();CaptureMotion();}
        Animate(1,true,changed||targetsChanged);RestoreKeyboardSelection(candidate);return true;
    }
    void RefreshRestoredGeometry(bool animate=true){
        if(phase==Phase::Opening||phase==Phase::Settled){ReconcileWindowStates();return;}
        if(phase!=Phase::Closing||!chosen||IsIconic(chosen))return;
        for(auto&v:views)for(auto&i:v->items)if(i.source==chosen&&i.minimized){
            RECT r{};if(!Bounds(i.source,r))return;
            Box restored{double(r.left-v->bounds.left),double(r.top-v->bounds.top),double(r.right-r.left),double(r.bottom-r.top)};
            float radius=SystemCornerRadius(i.source,v->dpi);
            bool rebound=RebindRestoredSample(*v,i,r);
            if(std::abs(restored.x-i.original.x)<.1&&std::abs(restored.y-i.original.y)<.1&&std::abs(restored.w-i.original.w)<.1&&std::abs(restored.h-i.original.h)<.1&&radius==i.nativeRadius){if(rebound)device->Commit();return;}
            if(animate){progress=CurrentProgress();CaptureMotion();}
            i.original=restored;i.nativeRadius=radius;
            if(animate)Animate(0,true);
            return;
        }
    }
    void Close(HWND selection){
        if(phase==Phase::Hidden||phase==Phase::Closing)return;
        ResetGesture();RemoveProp(controller,kOverviewProperty);
        progress=CurrentProgress();CaptureMotion();chosen=selection;
        if(restoreHook){UnhookWinEvent(restoreHook);restoreHook=nullptr;}restoreSource=nullptr;
        HWND target=chosen?chosen:previous;
        // Match the current native rectangle at handoff, including app-driven
        // size changes that may have happened while the overview was open.
        for(auto&v:views)for(auto&i:v->items){RECT r{};if(Bounds(i.source,r))i.original={double(r.left-v->bounds.left),double(r.top-v->bounds.top),double(r.right-r.left),double(r.bottom-r.top)};
            if(i.minimized&&i.source==chosen){
                // The selected tile grows directly into its restored window;
                // only the other minimized tiles retreat to the taskbar edge.
                WINDOWPLACEMENT placement{sizeof(placement)};
                i.nativeRadius=GetWindowPlacement(i.source,&placement)&&(placement.flags&WPF_RESTORETOMAXIMIZED)?0.f:SystemCornerRadius(i.source,v->dpi);
                DWORD pid=0,tid=GetWindowThreadProcessId(i.source,&pid);
                restoreSource=i.source;
                restoreHook=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,WindowStateEvent,pid,tid,WINEVENT_OUTOFCONTEXT);
            }
        }
        ClearSelection();
        // Anticipate foreground order while preserving topmost windows; the
        // foreground event then reconciles the exact native order.
        FollowNativeZOrder(target);
        Animate(0,true);
        // Submit motion before calling the native activation API: another
        // input queue may take ~100 ms to respond, while DWM keeps animating.
        if(phase==Phase::Closing){ActivateWindow(target,chosen!=nullptr);RefreshRestoredGeometry();}
    }
    void Reopen(){
        BuildGuard guard(*this);
        chosen=nullptr;previous=GetForegroundWindow();
        SetProp(controller,kOverviewProperty,reinterpret_cast<HANDLE>(kShellAck));FocusOverview();
        ReconcileWindowStates(true);
    }
    void End(bool restore,bool idle=true){
        ResetGesture();RemoveProp(controller,kOverviewProperty);
        if(timer)CancelWaitableTimer(timer);idleTimer=false;
        animations.clear();animationIndex.clear();
        bool visible=std::any_of(views.begin(),views.end(),[](const auto&v){return v->hwnd&&IsWindowVisible(v->hwnd);});
        phase=Phase::Hidden;
        if(foregroundHook){UnhookWinEvent(foregroundHook);foregroundHook=nullptr;}
        if(destroyHook){UnhookWinEvent(destroyHook);destroyHook=nullptr;}
        if(restoreHook){UnhookWinEvent(restoreHook);restoreHook=nullptr;}
        if(windowStateHook){UnhookWinEvent(windowStateHook);windowStateHook=nullptr;}restoreSource=nullptr;
        if(geometryHook){UnhookWinEvent(geometryHook);geometryHook=nullptr;}
        HWND target=chosen?chosen:previous;
        if(restore)ActivateWindow(target,chosen!=nullptr);
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
        views.clear();previous=chosen=selectedSource=pressedButtonSource=nullptr;pressedButton=WindowButton::None;keyboardSelection=false;progress=0;
        if(device)device->Commit(); // Submit the filter-input detachments from Item destruction.
        if(idle&&timer&&device){
            if(ShouldReleaseGraphics())PostMessage(controller,kReleaseInactive,0,0);
            else{idleTimer=true;Schedule(60);}
        }
    }
    int Hit(const View&v,POINT p){
        auto hit=[&](int n){const auto&i=v.items[n];Box b=CurrentBox(i);
            if(p.x>=b.x&&p.x<b.right()&&p.y>=b.y&&p.y<b.bottom())return true;
            double x=b.x+(b.w-i.target.w)*.5,y=b.bottom()-v.dpi;
            double r=TitleShoulderRadius(v,i),dy=std::clamp(y+v.dpi+r-p.y,0.,r);
            double flare=r-std::sqrt(std::max(0.,r*r-dy*dy));
            return i.label&&p.x>=x+i.titleLeft-flare&&p.x<x+i.titleRight+flare&&p.y>=y&&p.y<y+i.titleHeight;
        };
        if(!v.hitOrder.empty()){for(int n:v.hitOrder)if(hit(n))return n;}
        else for(size_t n=0;n<v.items.size();++n)if(hit(int(n)))return int(n);
        return -1;
    }
    // Gesture selection uses final layout coordinates, so opening animations do
    // not move the target out from under a held gesture.
    HWND GestureChoice(double x,double y)const{
        HWND result=nullptr;double best=std::numeric_limits<double>::infinity();
        for(auto&v:views)for(auto&i:v->items){
            Box b=i.target;b.x+=v->bounds.left;b.y+=v->bounds.top;
            double dx=std::max({b.x-x,0.,x-b.right()}),dy=std::max({b.y-y,0.,y-b.bottom()});
            double cx=x-b.cx(),cy=y-b.cy();double score=dx*dx+dy*dy+.02*(cx*cx+cy*cy);
            if(score<best){best=score;result=i.source;}
        }
        return result;
    }
    HWND KeyboardStart()const{
        for(HWND candidate:{selectedSource,previous})if(candidate)
            for(auto&v:views)for(auto&i:v->items)if(i.source==candidate)return candidate;
        return GestureChoice(lastPointer.x,lastPointer.y);
    }
    HWND KeyboardChoice(UINT key)const{
        HWND initial=KeyboardStart();if(!initial)return nullptr;
        Box origin{};bool minimized=false;
        for(auto&v:views)for(auto&i:v->items)if(i.source==initial){
            origin=i.target;origin.x+=v->bounds.left;origin.y+=v->bounds.top;minimized=i.minimized;
        }
        bool horizontal=key==VK_LEFT||key==VK_RIGHT;
        double direction=key==VK_LEFT||key==VK_UP?-1.:1.;
        HWND result=initial;double best=std::numeric_limits<double>::infinity();
        double firstEnd=std::numeric_limits<double>::infinity();
        // Use final preview coordinates across all monitors. The choice stays
        // stable during motion, and the minimized shelf is part of navigation.
        auto candidates=[&](auto visit){for(auto&v:views)for(auto&i:v->items){
            if(i.source==initial)continue;
            if(horizontal&&i.minimized!=minimized)continue;
            Box b=i.target;b.x+=v->bounds.left;b.y+=v->bounds.top;
            double forward=direction*(horizontal?b.cx()-origin.cx():b.cy()-origin.cy());
            if(forward<=.5)continue;
            double cross=horizontal?b.cy()-origin.cy():b.cx()-origin.cx();
            double overlap=horizontal?std::min(b.bottom(),origin.bottom())-std::max(b.y,origin.y):
                                      std::min(b.right(),origin.right())-std::max(b.x,origin.x);
            bool aligned=overlap>.5;
            // Prefer the same row/column before comparing distances. A diagonal
            // fallback must lie mainly in the requested direction, and only
            // up/down may move between ordinary previews and the minimized shelf.
            if(!aligned&&std::abs(cross)>forward)continue;
            double gap=horizontal?std::max({b.y-origin.bottom(),origin.y-b.bottom(),0.}):
                                  std::max({b.x-origin.right(),origin.x-b.right(),0.});
            double score=forward*forward+4*gap*gap+.25*cross*cross;
            double leading=direction*(horizontal?(direction>0?b.x:b.right()):(direction>0?b.y:b.bottom()));
            double trailing=direction*(horizontal?(direction>0?b.right():b.x):(direction>0?b.bottom():b.y));
            visit(i.source,leading,trailing,overlap,score);
        }};
        candidates([&](HWND source,double,double trailing,double overlap,double score){
            if(overlap>.5)firstEnd=std::min(firstEnd,trailing);
            else if(score<best){best=score;result=source;}
        });
        if(!std::isfinite(firstEnd))return result;
        // Only compare the first group of overlapping axis intervals. A window
        // entirely beyond a nearer preview belongs to the next column/row.
        // Within this group, projected overlap takes precedence over distance.
        double bestOverlap=-1,bestLeading=std::numeric_limits<double>::infinity();
        result=initial;best=std::numeric_limits<double>::infinity();
        candidates([&](HWND source,double leading,double,double overlap,double score){
            if(overlap<=.5||leading>=firstEnd-.5)return;
            bool sameOverlap=std::abs(overlap-bestOverlap)<=.5;
            if(overlap>bestOverlap+.5||(sameOverlap&&(leading<bestLeading-.5||
               (std::abs(leading-bestLeading)<=.5&&score<best)))){
                bestOverlap=overlap;bestLeading=leading;best=score;result=source;
            }
        });
        return result;
    }
    void NavigateKeyboard(UINT key){
        if(building||gestureToken||pressedButtonSource||
           (phase!=Phase::Opening&&phase!=Phase::Settled))return;
        GetCursorPos(&lastPointer);keyboardSelection=true;
        Select(KeyboardChoice(key));
    }
    void RestoreKeyboardSelection(HWND candidate){
        if(!keyboardSelection)return;
        for(auto&v:views)for(auto&i:v->items)if(i.source==candidate){Select(candidate);return;}
        Select(KeyboardStart());
    }
    void CountGesture(LPCWSTR property){if(controller)SetProp(controller,property,reinterpret_cast<HANDLE>(reinterpret_cast<ULONG_PTR>(GetProp(controller,property))+1));}
    void ResetGesture(){gestureToken=0;gestureNavigating=false;gestureReady=false;if(controller)RemoveProp(controller,kGestureProperty);}
    void BeginGesture(UINT token,bool alreadyOpen=false){
        if(!token||!settings.winTab)return;
        if(phase==Phase::Hidden&&!Begin())return;
        if(phase==Phase::Closing)Reopen();
        if(phase!=Phase::Opening&&phase!=Phase::Settled)return;
        HWND initial=selectedSource?selectedSource:previous;
        bool found=false;for(auto&v:views)for(auto&i:v->items)if(i.source==initial)found=true;
        if(!found){POINT p{};GetCursorPos(&p);initial=GestureChoice(p.x,p.y);}
        if(!initial)return;
        keyboardSelection=false;
        for(auto&v:views)for(auto&i:v->items)if(i.source==initial){
            gestureOriginX=v->bounds.left+i.target.cx();gestureOriginY=v->bounds.top+i.target.cy();
            gestureWidth=std::max(1L,v->bounds.right-v->bounds.left);gestureHeight=std::max(1L,v->bounds.bottom-v->bounds.top);
        }
        gestureToken=token;gestureStarted=GetTickCount64();gestureNavigating=gestureReady=false;gestureOpenedAlready=alreadyOpen;
        gestureBaseX=gestureBaseY=gestureLastX=gestureLastY=gestureMinY=gestureReadyX=gestureReadyY=0;
        SetProp(controller,kGestureProperty,reinterpret_cast<HANDLE>(ULONG_PTR(token)));
        CountGesture(L"WindowScatter.GestureStarts.v5");Select(initial);
    }
    void UpdateGesture(UINT token,LPARAM data){
        if(!gestureToken||token!=gestureToken||(phase!=Phase::Opening&&phase!=Phase::Settled))return;
        double nx=double(GET_X_LPARAM(data))/16384.,ny=double(GET_Y_LPARAM(data))/16384.;
        CountGesture(L"WindowScatter.GestureUpdates.v5");
        if(!gestureNavigating){
            ULONGLONG elapsed=GetTickCount64()-gestureStarted;
            gestureMinY=std::min(gestureMinY,ny);
            // Separate the opening stroke from deliberate navigation. A short
            // upward flick must not activate a window just because it generated
            // several updates. A clear turn, or movement after holding, arms
            // selection; the initial upward travel is then removed.
            if(!gestureReady&&elapsed>=350){gestureReady=true;gestureReadyX=gestureLastX;gestureReadyY=gestureLastY;}
            bool turned=!gestureOpenedAlready&&elapsed>=160&&(std::abs(nx)>=.035||ny-gestureMinY>=.035);
            bool movedAfterHold=gestureReady&&std::hypot(nx-gestureReadyX,ny-gestureReadyY)>=.02;
            if(gestureOpenedAlready&&std::hypot(nx,ny)>=.02){gestureBaseX=gestureBaseY=0;gestureNavigating=true;}
            else if(turned){gestureBaseX=0;gestureBaseY=gestureMinY;gestureNavigating=true;}
            else if(movedAfterHold){gestureBaseX=gestureReadyX;gestureBaseY=gestureReadyY;gestureNavigating=true;}
            gestureLastX=nx;gestureLastY=ny;
            if(!gestureNavigating)return;
        }
        double x=gestureOriginX+(nx-gestureBaseX)*gestureWidth*2.25;
        double y=gestureOriginY+(ny-gestureBaseY)*gestureHeight*2.25;
        HWND choice=GestureChoice(x,y);if(choice!=selectedSource)CountGesture(L"WindowScatter.GestureSelections.v5");Select(choice);
    }
    void FinishGesture(UINT token,bool commit){
        if(!gestureToken||token!=gestureToken)return;
        if(commit&&!gestureNavigating){CountGesture(L"WindowScatter.GestureOpens.v5");ResetGesture();return;}
        HWND target=commit?selectedSource:nullptr;ResetGesture();
        CountGesture(commit?L"WindowScatter.GestureCommits.v5":L"WindowScatter.GestureCancels.v5");
        Close(target);
    }
    void ClearSelection(){
        selectedSource=nullptr;for(auto&v:views){v->selected=-1;for(auto&i:v->items){
            if(i.borderAttached){i.position->RemoveVisual(i.border.Get());i.borderAttached=false;i.borderEffect->SetOpacity(0.f);}
            if(i.buttonsAttached){i.position->RemoveVisual(i.buttons.Get());i.buttonsAttached=false;i.hotButton=WindowButton::None;}
        }}
    }
    void Select(HWND source){
        if(phase==Phase::Closing||phase==Phase::Hidden)return;
        if(selectedSource==source)return;ClearSelection();
        selectedSource=source;
        for(auto&v:views)for(size_t n=0;n<v->items.size();++n){auto&i=v->items[n];if(i.source!=source)continue;
            if((!i.borderSurface||i.borderColor!=accentColor)&&!CreateOutline(*v,i)){End(false);return;}
            if(!PrepareButtons(*v,i)){End(false);return;}
            SyncDecorations(*v,i);
            i.position->AddVisual(i.border.Get(),FALSE,i.label?i.label.Get():i.content.Get());i.borderAttached=true;i.borderEffect->SetOpacity(1.f);v->selected=int(n);
            i.position->AddVisual(i.buttons.Get(),FALSE,nullptr);i.buttonsAttached=true;
        }
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
        SetProp(controller,kNativeBypassProperty,reinterpret_cast<HANDLE>(ULONG_PTR(GetTickCount()+2000)));
        auto keys=NativeTaskViewInputs((GetAsyncKeyState(VK_LWIN)&0x8000)!=0,(GetAsyncKeyState(VK_RWIN)&0x8000)!=0,
                                      (GetAsyncKeyState(VK_LSHIFT)&0x8000)!=0,(GetAsyncKeyState(VK_RSHIFT)&0x8000)!=0);
        if(SendInput(UINT(keys.size()),keys.data(),sizeof(INPUT))!=keys.size()){RemoveProp(controller,kNativeBypassProperty);Log(L"Open Windows Task View",HRESULT_FROM_WIN32(GetLastError()));}
    }
    LRESULT Message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
        if(msg==WM_NCCREATE){auto cs=reinterpret_cast<CREATESTRUCT*>(lp);if(cs->lpCreateParams)SetWindowLongPtr(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));}
        auto v=reinterpret_cast<View*>(GetWindowLongPtr(hwnd,GWLP_USERDATA));
        if(building){
            if(msg==kWindowStateChanged){deferredWindowState=true;return 0;}
            if(msg==kTaskbarOrder){deferredTaskbar=true;return 0;}
            if(msg==kSceneChanged){HWND source=reinterpret_cast<HWND>(wp);if(std::find(deferredSceneChanges.begin(),deferredSceneChanges.end(),source)==deferredSceneChanges.end())deferredSceneChanges.push_back(source);return 0;}
            if(msg>=kGestureBegin&&msg<=kGestureCancel){
                if(msg==kGestureUpdate&&!deferredGestures.empty()&&deferredGestures.back().message==msg&&deferredGestures.back().token==wp)deferredGestures.back().data=lp;
                else if(deferredGestures.size()<32)deferredGestures.push_back({msg,wp,lp});
                return 0;
            }
            if(msg==kShow){if(!deferredCommand)deferredCommand=kShow;return 0;}
            if(msg==kToggle){deferredCommand=deferredCommand==kToggle?0:kToggle;return 0;}
            if(msg==kSettings||msg==kNativeTaskView){deferredCommand=msg;return 0;}
        }
        switch(msg){
        case WM_HOTKEY:if(wp==2)PostMessage(controller,kNativeTaskView,0,0);return 0;
        case kShellTaskView:{
            if(!settings.winTab)return 0;
            ULONG_PTR count=reinterpret_cast<ULONG_PTR>(GetProp(controller,L"WindowScatter.NativeRequests.v4"));
            SetProp(controller,L"WindowScatter.NativeRequests.v4",reinterpret_cast<HANDLE>(count+1));
            if(wp)SetProp(controller,L"WindowScatter.GestureRequests.v4",reinterpret_cast<HANDLE>(reinterpret_cast<ULONG_PTR>(GetProp(controller,L"WindowScatter.GestureRequests.v4"))+1));
            return PostMessage(controller,wp?kShow:kToggle,0,0)?kShellAck:0;
        }
        case kNativeConsumed:if(reinterpret_cast<ULONG_PTR>(GetProp(controller,kNativeBypassProperty))==wp)RemoveProp(controller,kNativeBypassProperty);return 0;
        case kToggle:if(phase==Phase::Hidden)Begin();else if(phase==Phase::Closing)Reopen();else Close(nullptr);return 0;
        case kShow:if(phase==Phase::Hidden)Begin();else if(phase==Phase::Closing)Reopen();return 0;
        case kGestureBegin:BeginGesture(UINT(wp),lp!=0);return 0;
        case kGestureUpdate:UpdateGesture(UINT(wp),lp);return 0;
        case kGestureEnd:FinishGesture(UINT(wp),lp!=0);return 0;
        case kGestureCancel:CountGesture(L"WindowScatter.GestureCancels.v5");Close(nullptr);return 0;
        case kNativeTaskView:NativeTaskView();return 0;
        case kSettings:End(false);LoadSettings();memoryPressure=false;
            if(ShouldReleaseGraphics())PostMessage(controller,kReleaseInactive,0,0);
            else if(!device)PostMessage(controller,kWarmup,0,0);return 0;
        case kReleaseInactive:ReleaseInactiveGraphics();return 0;
        case kWindowGone:RemoveWindowFromOverview(reinterpret_cast<HWND>(wp));return 0;
        case kWarmup:Warmup();return 0;
        case kWindowStateChanged:RefreshRestoredGeometry();return 0;
        case kTaskbarOrder:TaskbarChanged();return 0;
        case kSceneChanged:{
            // Coalesce already queued geometry notifications without a polling
            // timer or an extra frame of debounce latency.
            std::vector<HWND> sources{reinterpret_cast<HWND>(wp)};MSG pending{};
            for(int n=0;n<256&&PeekMessage(&pending,controller,kSceneChanged,kSceneChanged,PM_REMOVE);++n){HWND source=reinterpret_cast<HWND>(pending.wParam);
                if(std::find(sources.begin(),sources.end(),source)==sources.end())sources.push_back(source);}
            sceneNeedsLayout=false;for(HWND source:sources)SceneChanged(source,true);
            if(std::exchange(sceneNeedsLayout,false)&&(phase==Phase::Opening||phase==Phase::Settled))Relayout();return 0;
        }
        case kSyncOrder:if(phase==Phase::Closing){HWND foreground=GetForegroundWindow();if(foreground==(chosen?chosen:previous)){RefreshRestoredGeometry();FollowNativeZOrder();device->Commit();}else if(!IsOurWindow(foreground))End(false);}return 0;
        case kDismiss:if(!building&&phase!=Phase::Hidden&&(wp||(phase!=Phase::Closing&&!IsOurWindow(GetForegroundWindow()))))End(false);return 0;
        case kAccentChanged:case WM_DWMCOLORIZATIONCOLORCHANGED:if(hwnd==controller)RefreshAccent();return 0;
        case WM_THEMECHANGED:desktopCache.clear();if(hwnd==controller)RefreshAccent();return 0;
        case WM_DISPLAYCHANGE:case WM_DPICHANGED:case WM_SETTINGCHANGE:desktopCache.clear();if(!building&&phase!=Phase::Hidden)PostMessage(controller,kDismiss,1,0);return 0;
        case WM_QUERYENDSESSION:return TRUE;
        case WM_ENDSESSION:if(wp)SetEvent(g_stopEvent);return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);if(!building&&d3d&&FAILED(d3d->GetDeviceRemovedReason())){End(false,false);ReleaseDevice();PostMessage(controller,kWarmup,0,0);}return 0;}
        case WM_MOUSEMOVE:if(!gestureToken&&v&&(phase==Phase::Settled||phase==Phase::Opening)){POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)},screen=point;ClientToScreen(hwnd,&screen);
            if(keyboardSelection&&screen.x==lastPointer.x&&screen.y==lastPointer.y)return 0;
            keyboardSelection=false;lastPointer=screen;
            int i=Hit(*v,point);HWND hover=i>=0?v->items[i].source:nullptr;if(selectedSource!=hover)Select(hover);
            if(phase!=Phase::Hidden)HoverButtons(*v,i,point);TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0};TrackMouseEvent(&track);}return 0;
        case WM_MOUSELEAVE:if(!gestureToken&&!keyboardSelection&&v&&v->selected>=0)Select(nullptr);return 0;
        case WM_LBUTTONDOWN:if(v&&(phase==Phase::Settled||phase==Phase::Opening)){POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};int i=Hit(*v,point);
            auto button=ButtonAt(*v,i,point);if(button!=WindowButton::None){pressedButtonSource=v->items[i].source;pressedButton=button;SetCapture(hwnd);}}return 0;
        case WM_LBUTTONUP:if(v&&(phase==Phase::Settled||phase==Phase::Opening)){POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};int i=Hit(*v,point);
            HWND pressed=std::exchange(pressedButtonSource,nullptr);auto button=std::exchange(pressedButton,WindowButton::None);if(GetCapture()==hwnd)ReleaseCapture();
            if(pressed){if(i>=0&&v->items[i].source==pressed&&ButtonAt(*v,i,point)==button){if(button==WindowButton::Close)RequestClose(v->items[i]);else if(button==WindowButton::Minimize)RequestMinimize(v->items[i]);}}
            else if(ButtonAt(*v,i,point)==WindowButton::None)Close(i<0?nullptr:v->items[i].source);}return 0;
        case WM_CAPTURECHANGED:pressedButtonSource=nullptr;pressedButton=WindowButton::None;return 0;
        case WM_RBUTTONUP:Close(nullptr);return 0;
        case WM_SYSKEYDOWN:case WM_KEYDOWN:
            if(building)return 0;
            if(wp==VK_ESCAPE){Close(nullptr);return 0;}
            if(v&&(phase==Phase::Settled||phase==Phase::Opening)){
                if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_UP||wp==VK_DOWN){NavigateKeyboard(UINT(wp));return 0;}
                if(wp==VK_TAB)return 0;
                if(wp==VK_RETURN){Close(KeyboardStart());return 0;}
            }break;
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
        if(ok){ok=CreateOutline(view,view.items[0]);view.items[0].position->AddVisual(view.items[0].border.Get(),FALSE,view.items[0].content.Get());view.items[0].borderAttached=true;view.items[0].borderEffect->SetOpacity(1.f);}
        if(ok){StaticPose(0);Animate(1);HRESULT hr=device->WaitForCommitCompletion();ok=SUCCEEDED(hr);Log(L"Hidden composition probe",hr);}
        End(false,false);DestroyWindow(fixture);return ok;
    }
#endif
};
App* App::instance=nullptr;
static DWORD WINAPI ThreadMain(void*){
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    unsigned failures=0;bool ok=false;
    while(WaitForSingleObject(g_stopEvent,0)!=WAIT_OBJECT_0){
        App app;App::instance=&app;
        try{ok=app.Initialize();}catch(...){ok=false;}
        g_ready.store(ok);SetEvent(g_readyEvent);
#ifndef SCATTER_STANDALONE
        Wh_SetStringValue(L"renderer-startup-state",ok?L"ready":L"retrying");
        Wh_SetIntValue(L"renderer-startup-pid",GetCurrentProcessId());
#endif
        if(ok)app.Loop();app.Cleanup();App::instance=nullptr;
        if(ok)break;
#ifdef SCATTER_STANDALONE
        break;
#else
        DWORD delay=std::min<DWORD>(30000,1000u<<std::min<unsigned>(failures++,5));
        if(WaitForSingleObject(g_stopEvent,delay)!=WAIT_TIMEOUT)break;
#endif
    }
    if(SUCCEEDED(co))CoUninitialize();return ok?0:1;
}
static bool StartAsync(){
    g_stopEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);g_readyEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);if(!g_stopEvent||!g_readyEvent)return false;
    g_ready=false;g_thread=CreateThread(nullptr,0,ThreadMain,nullptr,0,nullptr);return g_thread!=nullptr;
}
static bool Start(){
    if(!StartAsync())return false;HANDLE waits[]{g_readyEvent,g_thread};return WaitForMultipleObjects(2,waits,FALSE,10000)==WAIT_OBJECT_0&&g_ready;
}
static void Stop(){if(g_stopEvent)SetEvent(g_stopEvent);if(g_thread){WaitForSingleObject(g_thread,INFINITE);CloseHandle(g_thread);g_thread=nullptr;}if(g_readyEvent){CloseHandle(g_readyEvent);g_readyEvent=nullptr;}if(g_stopEvent){CloseHandle(g_stopEvent);g_stopEvent=nullptr;}}
} // namespace scatter

// Explorer recognizes the input. Win+Tab and precision-touchpad Task View use
// different request paths; redirect both before native view creation.
namespace scatter::shell {
constexpr wchar_t kBridgeProperty[]=L"WindowScatter.NativeBridge.v4";
using ToggleRequest=HRESULT(WINAPI*)(void*,HMONITOR,int);
static ToggleRequest originalToggle=nullptr;
using GestureRequest=HRESULT(WINAPI*)(void*,int,HWND const*);
static GestureRequest originalGesture=nullptr;
static std::atomic<bool> stopping{false};
using SwipeStart=HRESULT(WINAPI*)(void*,int,int,UINT,ULONGLONG);
using SwipeMove=HRESULT(WINAPI*)(void*,int,int,int,int,UINT);
using SwipeCancel=HRESULT(WINAPI*)(void*);
static SwipeStart originalStart=nullptr;
static SwipeMove originalContinue=nullptr,originalFinish=nullptr;
static SwipeCancel originalCancel=nullptr;
struct Swipe {
    HWND target=nullptr;UINT token=0;int mode=0;bool finishing=false;
    double x=0,y=0,width=10000,height=6000;
};
static std::mutex swipeMutex;
static std::map<void*,Swipe> swipes;
static std::atomic<UINT> nextToken{0};
static thread_local Swipe* activeSwipe=nullptr;
struct SwipeScope {Swipe*previous;explicit SwipeScope(Swipe&s):previous(std::exchange(activeSwipe,&s)){}~SwipeScope(){activeSwipe=previous;}};
static bool TakeSwipe(void*self,Swipe&value,bool remove=false){std::lock_guard lock(swipeMutex);auto i=swipes.find(self);if(i==swipes.end())return false;value=i->second;if(remove)swipes.erase(i);return true;}
static void SaveSwipe(void*self,const Swipe&value){std::lock_guard lock(swipeMutex);swipes[self]=value;}
static void EndSwipe(Swipe&s,bool commit){if(s.mode==1&&s.target)PostMessage(s.target,kGestureEnd,s.token,commit);s.mode=2;}
static void ReadSwipeDimensions(Swipe&s,UINT pointer){
    POINTER_INFO info{};RECT device{},display{};
    if(GetPointerInfo(pointer,&info)&&GetPointerDeviceRects(info.sourceDevice,&device,&display)){
        if(device.right>device.left&&device.bottom>device.top){s.width=double(device.right)-device.left;s.height=double(device.bottom)-device.top;}
    }
}
static LPARAM SwipePosition(const Swipe&s){
    int x=int(std::clamp(std::round(s.x/s.width*16384.),-32768.,32767.));
    int y=int(std::clamp(std::round(s.y/s.height*16384.),-32768.,32767.));
    return MAKELPARAM(short(x),short(y));
}
static HRESULT WINAPI StartHook(void*self,int x,int y,UINT pointer,ULONGLONG context){
    Swipe previous;if(TakeSwipe(self,previous,true))EndSwipe(previous,false);
    Swipe s;ReadSwipeDimensions(s,pointer);SwipeScope scope(s);
    HRESULT hr=originalStart(self,x,y,pointer,context);
    if(SUCCEEDED(hr))SaveSwipe(self,s);else EndSwipe(s,false);
    return hr;
}
static HRESULT WINAPI ContinueHook(void*self,int x,int y,int vx,int vy,UINT pointer){
    Swipe s;if(!TakeSwipe(self,s))return originalContinue(self,x,y,vx,vy,pointer);
    bool selecting=s.mode==1;SwipeScope scope(s);HRESULT hr=originalContinue(self,x,y,vx,vy,pointer);
    if(FAILED(hr))EndSwipe(s,false);
    else if(selecting&&s.mode==1){s.x+=x;s.y+=y;if(!PostMessage(s.target,kGestureUpdate,s.token,SwipePosition(s)))EndSwipe(s,false);}
    SaveSwipe(self,s);return hr;
}
static HRESULT WINAPI FinishHook(void*self,int x,int y,int vx,int vy,UINT pointer){
    Swipe s;if(!TakeSwipe(self,s,true))return originalFinish(self,x,y,vx,vy,pointer);
    s.finishing=true;SwipeScope scope(s);HRESULT hr=originalFinish(self,x,y,vx,vy,pointer);
    EndSwipe(s,SUCCEEDED(hr)&&!stopping.load()&&g_winTab.load());return hr;
}
static HRESULT WINAPI CancelHook(void*self){
    Swipe s;if(!TakeSwipe(self,s,true))return originalCancel(self);
    s.finishing=true;SwipeScope scope(s);HRESULT hr=originalCancel(self);EndSwipe(s,false);return hr;
}
static HWND RequestTarget(){
    if(stopping.load()||!g_winTab.load())return nullptr;
    HWND target=FindWindow(kController,L"Window Scatter Controller");
    if(!target||reinterpret_cast<ULONG_PTR>(GetProp(target,kShellReadyProperty))!=kShellAck)return nullptr;
    ULONG_PTR until=reinterpret_cast<ULONG_PTR>(GetProp(target,kNativeBypassProperty));
    if(until&&LONG(DWORD(until)-GetTickCount())>0){PostMessage(target,kNativeConsumed,until,0);return nullptr;}
    return target;
}
static bool QueueRequest(HWND target,bool showOnly){
    DWORD process=0;GetWindowThreadProcessId(target,&process);if(process)AllowSetForegroundWindow(process);
    return PostMessage(target,kShellTaskView,showOnly,0)!=FALSE;
}
static HRESULT WINAPI ToggleHook(void*self,HMONITOR monitor,int flags){
    if(HWND target=RequestTarget();target&&QueueRequest(target,false))return S_OK;
    return originalToggle(self,monitor,flags);
}
static HRESULT WINAPI GestureHook(void*self,int action,HWND const*window){
    // GestureAction 1 requests Task View; 0 performs normal gesture bookkeeping
    // without scheduling a native view. Preserve that cleanup through the original
    // method. In particular, never skip TaskViewHost::Show after its view exists.
    if(activeSwipe&&activeSwipe->mode)return originalGesture(self,0,window);
    if(action>=1&&action<=5){
        if(HWND target=RequestTarget()){
            bool visible=reinterpret_cast<ULONG_PTR>(GetProp(target,kOverviewProperty))==kShellAck;
            bool cancel=(action==2||action==3)&&visible;
            bool select=action==1||((action==4||action==5)&&visible&&activeSwipe);
            if(!select&&!cancel)return originalGesture(self,action,window);
            HRESULT cleanup=originalGesture(self,0,window);
            if(FAILED(cleanup))return cleanup;
            if(cancel){
                if(PostMessage(target,kGestureCancel,0,0)){if(activeSwipe){activeSwipe->target=target;activeSwipe->mode=2;}return S_OK;}
            }else if(activeSwipe&&!activeSwipe->finishing){
                UINT token=++nextToken;if(!token)token=++nextToken;
                DWORD process=0;GetWindowThreadProcessId(target,&process);if(process)AllowSetForegroundWindow(process);
                if(PostMessage(target,kGestureBegin,token,visible)){activeSwipe->target=target;activeSwipe->token=token;activeSwipe->mode=1;return S_OK;}
            }else if(QueueRequest(target,true))return S_OK;
        }
    }
    return originalGesture(self,action,window);
}
#ifndef SCATTER_STANDALONE
static HWND taskbar=nullptr;
static StartupSupport::Worker startupWorker;
static StartupSupport::Result AttemptInitialize(HANDLE stop){
    using namespace StartupSupport;
#ifndef _WIN64
    return Result::Retry;
#else
    HWND shellWindow=OwnedTaskbar();
    HMODULE module=GetModuleHandleW(L"twinui.pcshell.dll");
    if(!shellWindow||!module)return Result::Waiting;
    void*target=nullptr;void*gesture=nullptr;void*determine=nullptr;void*start=nullptr;void*update=nullptr;void*finish=nullptr;void*cancel=nullptr;
    std::vector<SymbolHook> symbols{
        {{L"?ToggleAllUpView@CAllUpViewService@@UEAAJPEAUHMONITOR__@@W4ALL_UP_VIEW_FLAGS@@@Z"},&target},
        {{L"?ActivateGestureAction@MultitaskingViewGestureHandler@@AEAAJW4GestureAction@MultitaskingViewGestureHandlerHelpers@@PEBQEAUHWND__@@@Z"},&gesture,nullptr,true},
        {{L"?DetermineTouchpadGestureAction@MultitaskingViewGestureHandlerHelpers@@YA?AW4GestureAction@1@W4GestureDirection@1@_NW4MULTITASKING_VIEW_TYPES@@@Z"},&determine,nullptr,true},
        {{L"?StartSwipe@MultitaskingViewGestureHandler@@UEAAJHHIUTOUCHPAD_GESTURE_CONTEXT@@@Z"},&start,nullptr,true},
        {{L"?ContinueSwipe@MultitaskingViewGestureHandler@@UEAAJHHHHI@Z"},&update,nullptr,true},
        {{L"?FinishSwipe@MultitaskingViewGestureHandler@@UEAAJHHHHI@Z"},&finish,nullptr,true},
        {{L"?CancelSwipe@MultitaskingViewGestureHandler@@UEAAJXZ"},&cancel,nullptr,true},
    };
    if(!Resolve(module,L"startup-symbols-taskview-v1",symbols,true,stop))return Result::Retry;
    for(auto&symbol:symbols)if(symbol.original)*symbol.original=symbol.address;
    std::vector<SymbolHook> hooks;
    auto add=[&](void*address,auto**original,auto hook){
        hooks.emplace_back(std::initializer_list<std::wstring_view>{},original,hook);hooks.back().address=address;
    };
    using Determine=int(WINAPI*)(int,bool,int);
    auto action=reinterpret_cast<Determine>(determine);
    if(gesture&&action&&action(1,false,0)==1&&action(1,true,0)==4){
        if(!start||!update||!finish||!cancel)return Result::Retry;
        add(start,&originalStart,StartHook);add(update,&originalContinue,ContinueHook);
        add(finish,&originalFinish,FinishHook);add(cancel,&originalCancel,CancelHook);
        add(gesture,&originalGesture,GestureHook);
    }else Wh_Log(L"Native Task View gesture mapping unavailable; preserving native gestures");
    add(target,&originalToggle,ToggleHook);
    if(!Install(hooks,stop))return Result::Retry;
    taskbar=shellWindow;SetProp(taskbar,kBridgeProperty,reinterpret_cast<HANDLE>(kShellAck));
    return Result::Ready;
#endif
}
static bool Initialize(){
#ifndef _WIN64
    return false;
#else
    stopping=false;return startupWorker.Prepare(AttemptInitialize,L"shell-startup");
#endif
}
static void AfterInit(){startupWorker.Begin();}
static void BeforeUninit(){stopping=true;startupWorker.Stop();std::lock_guard lock(swipeMutex);for(auto&[self,swipe]:swipes)EndSwipe(swipe,false);}
static void Uninit(){if(taskbar)RemoveProp(taskbar,kBridgeProperty);taskbar=nullptr;std::lock_guard lock(swipeMutex);swipes.clear();}
#endif
} // namespace scatter::shell


#ifndef SCATTER_STANDALONE
static void ReadSettings(){
    scatter::g_winTab=Wh_GetIntSetting(L"replaceWinTab")!=0;scatter::g_duration=Wh_GetIntSetting(L"durationMs");
    auto mode=Wh_GetStringSetting(L"memoryMode");
    scatter::g_memoryMode=mode&&!wcscmp(mode,L"resident")?1:mode&&!wcscmp(mode,L"saver")?2:0;
    if(mode)Wh_FreeStringSetting(mode);
}
BOOL WhTool_ModInit(){GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&WhTool_ModInit),&scatter::g_instance);ReadSettings();if(!scatter::StartAsync()){scatter::Stop();return FALSE;}return TRUE;}
void WhTool_ModSettingsChanged(){ReadSettings();if(auto hwnd=scatter::g_controller.load())PostMessage(hwnd,scatter::kSettings,0,0);}
void WhTool_ModUninit(){scatter::Stop();}
// Keep the official launcher for windhawk.exe; explorer.exe only hosts the
// native Task View bridge, with no renderer or extra keyboard listener.
#define Wh_ModInit ToolLauncher_ModInit
#define Wh_ModAfterInit ToolLauncher_ModAfterInit
#define Wh_ModSettingsChanged ToolLauncher_ModSettingsChanged
#define Wh_ModUninit ToolLauncher_ModUninit
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
static StartupSupport::Worker g_launcherWorker;
static HANDLE g_launchedProcess=nullptr;
static StartupSupport::Result LaunchToolAttempt(HANDLE stop);

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
    return g_launcherWorker.Prepare(LaunchToolAttempt,L"launcher-startup");
}

static StartupSupport::Result LaunchToolAttempt(HANDLE stop) {
    using StartupSupport::Result;
    if(StartupSupport::Cancelled(stop))return Result::Waiting;
    HWND controller=FindWindowW(scatter::kController,L"Window Scatter Controller");
    if(controller&&reinterpret_cast<ULONG_PTR>(GetPropW(controller,scatter::kShellReadyProperty))==scatter::kShellAck){
        if(g_launchedProcess){CloseHandle(g_launchedProcess);g_launchedProcess=nullptr;}
        return Result::Ready;
    }
    if(g_launchedProcess){
        if(WaitForSingleObject(g_launchedProcess,0)==WAIT_TIMEOUT)return Result::Waiting;
        CloseHandle(g_launchedProcess);g_launchedProcess=nullptr;
        return Result::Retry;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return Result::Retry;
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
            return Result::Retry;
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
        return Result::Retry;
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
        return Result::Retry;
    }

    g_launchedProcess=pi.hProcess;
    CloseHandle(pi.hThread);
    return Result::Waiting;
}

void Wh_ModAfterInit() {
    if(g_isToolModProcessLauncher)g_launcherWorker.Begin();
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) {
        g_launcherWorker.Stop();
        if(g_launchedProcess){CloseHandle(g_launchedProcess);g_launchedProcess=nullptr;}
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}

#undef Wh_ModInit
#undef Wh_ModAfterInit
#undef Wh_ModSettingsChanged
#undef Wh_ModUninit
static bool g_shellProcess=false;
BOOL Wh_ModInit(){
    wchar_t path[MAX_PATH]{};GetModuleFileName(nullptr,path,ARRAYSIZE(path));
    const wchar_t*name=wcsrchr(path,L'\\');name=name?name+1:path;
    g_shellProcess=_wcsicmp(name,L"explorer.exe")==0;
    if(g_shellProcess){ReadSettings();return scatter::shell::Initialize();}
    return ToolLauncher_ModInit();
}
void Wh_ModAfterInit(){if(g_shellProcess)scatter::shell::AfterInit();else ToolLauncher_ModAfterInit();}
void Wh_ModSettingsChanged(){if(g_shellProcess)ReadSettings();else ToolLauncher_ModSettingsChanged();}
void Wh_ModBeforeUninit(){if(g_shellProcess)scatter::shell::BeforeUninit();else if(g_isToolModProcessLauncher)g_launcherWorker.Stop();}
void Wh_ModUninit(){if(g_shellProcess)scatter::shell::Uninit();else ToolLauncher_ModUninit();}
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
