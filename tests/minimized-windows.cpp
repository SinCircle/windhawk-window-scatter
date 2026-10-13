#define SCATTER_STANDALONE
#define SCATTER_TEST
#include "../window-scatter.wh.cpp"
#include <string>
using namespace scatter;
namespace {
constexpr wchar_t kFixtureClass[]=L"WindowScatter.MinimizedFixture";
HWND fixtures[6]{};
std::wstring FixtureTitle(DWORD id,int n){return L"Window Scatter fixture "+std::to_wstring(id)+L" / "+std::to_wstring(n);}
LRESULT CALLBACK FixtureProc(HWND w,UINT m,WPARAM p,LPARAM l){
    if(m==WM_GETICON)SetProp(w,L"IconQueries",reinterpret_cast<HANDLE>(reinterpret_cast<UINT_PTR>(GetProp(w,L"IconQueries"))+1));
    if(m==WM_SYSCOMMAND&&(p&0xFFF0)==SC_CLOSE){SetProp(w,L"CloseRequests",reinterpret_cast<HANDLE>(reinterpret_cast<UINT_PTR>(GetProp(w,L"CloseRequests"))+1));return 0;}
    if(m==WM_SYSCOMMAND&&(p&0xFFF0)==SC_MINIMIZE&&GetProp(w,L"RejectMinimize"))return 0;
    if(m==WM_PAINT){
        PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
        HBRUSH red=CreateSolidBrush(RGB(255,0,0)),blue=CreateSolidBrush(RGB(0,0,255));RECT left=r,right=r;left.right=left.left+24;right.left=right.right-24;
        FillRect(dc,&left,blue);FillRect(dc,&right,red);DeleteObject(red);DeleteObject(blue);EndPaint(w,&ps);return 0;
    }
    if(m==WM_APP+1){PostQuitMessage(0);return 0;}
    if(m==WM_APP+2){ShowWindow(w,SW_SHOWMINNOACTIVE);return 0;}
    return DefWindowProc(w,m,p,l);
}
int RunFixtures(DWORD id){
    WNDCLASS cls{};cls.hInstance=GetModuleHandle(nullptr);cls.lpfnWndProc=FixtureProc;cls.lpszClassName=kFixtureClass;RegisterClass(&cls);
    BYTE mask[32]{};DWORD colors[256];std::fill(std::begin(colors),std::end(colors),0xFF00D060);
    HICON icon=CreateIcon(cls.hInstance,16,16,1,32,mask,reinterpret_cast<BYTE*>(colors));
    for(int n=0;n<6;n++){
        fixtures[n]=CreateWindowEx(n==5?WS_EX_TOOLWINDOW:WS_EX_APPWINDOW,kFixtureClass,FixtureTitle(id,n).c_str(),WS_OVERLAPPEDWINDOW,70,70,800,450,nullptr,nullptr,cls.hInstance,nullptr);
        if(n<2)SendMessage(fixtures[n],WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(icon));
        if(n!=3&&n!=4){SetWindowPos(fixtures[n],nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);UpdateWindow(fixtures[n]);}
    }
    DwmFlush();Sleep(150);
    for(int n:{1,2,3,5})ShowWindow(fixtures[n],SW_SHOWMINNOACTIVE);
    WINDOWPLACEMENT placement{sizeof(placement)};GetWindowPlacement(fixtures[2],&placement);placement.flags|=WPF_RESTORETOMAXIMIZED;SetWindowPlacement(fixtures[2],&placement);
    auto name=L"Local\\WindowScatter.MinimizedTest."+std::to_wstring(id);HANDLE ready=OpenEvent(EVENT_MODIFY_STATE,FALSE,name.c_str());if(ready){SetEvent(ready);CloseHandle(ready);}
    MSG message;while(GetMessage(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessage(&message);}
    for(auto w:fixtures)DestroyWindow(w);DestroyIcon(icon);return 0;
}
struct FixtureProcess {
    PROCESS_INFORMATION process{};HANDLE ready=nullptr;DWORD id=GetCurrentProcessId();
    bool Start(){
        auto name=L"Local\\WindowScatter.MinimizedTest."+std::to_wstring(id);ready=CreateEvent(nullptr,TRUE,FALSE,name.c_str());
        wchar_t path[MAX_PATH];GetModuleFileName(nullptr,path,MAX_PATH);std::wstring command=L"\""+std::wstring(path)+L"\" --fixture "+std::to_wstring(id);
        STARTUPINFO si{sizeof(si)};si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
        if(!CreateProcess(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&process))return false;
        return WaitForSingleObject(ready,10000)==WAIT_OBJECT_0;
    }
    HWND Window(int n){return FindWindow(kFixtureClass,FixtureTitle(id,n).c_str());}
    ~FixtureProcess(){if(HWND w=Window(0))PostMessage(w,WM_APP+1,0,0);if(process.hProcess){WaitForSingleObject(process.hProcess,3000);CloseHandle(process.hProcess);CloseHandle(process.hThread);}if(ready)CloseHandle(ready);}
};
bool passed=true;
void Check(bool value,const char*message){printf("%s %s\n",value?"PASS":"FAIL",message);fflush(stdout);passed&=value;}
void Pump(int milliseconds){ULONGLONG end=GetTickCount64()+milliseconds;do{MSG m;while(PeekMessage(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}Sleep(10);}while(GetTickCount64()<end);}
PrivateDwm::QuerySize realSize=nullptr;PrivateDwm::CreateThumb realThumb=nullptr;HWND failSize=nullptr,failThumb=nullptr;
HRESULT WINAPI QuerySize(HWND source,BOOL client,SIZE*size){return source==failSize?E_FAIL:realSize(source,client,size);}
HRESULT WINAPI CreateThumb(HWND a,HWND source,DWORD c,DWM_THUMBNAIL_PROPERTIES*p,void*d,void**e,HTHUMBNAIL*f){return source==failThumb?E_FAIL:realThumb(a,source,c,p,d,e,f);}
COLORREF Pixel(View&v,int x,int y){HDC dc=GetDC(nullptr);COLORREF color=GetPixel(dc,v.bounds.left+x,v.bounds.top+y);ReleaseDC(nullptr,dc);return color;}
int GreenPixels(View&v,int x,int y,int width,int height){
    HDC dc=GetDC(nullptr),copy=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};DWORD*data=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&data),nullptr,0);if(!bitmap){DeleteDC(copy);ReleaseDC(nullptr,dc);return 0;}
    auto old=SelectObject(copy,bitmap);BitBlt(copy,0,0,width,height,dc,v.bounds.left+x,v.bounds.top+y,SRCCOPY);GdiFlush();int count=0;
    for(int n=0;n<width*height;n++){DWORD c=data[n];if(((c>>8)&255)>160&&((c>>16)&255)<40&&(c&255)>50&&(c&255)<140)count++;}
    SelectObject(copy,old);DeleteObject(bitmap);DeleteDC(copy);ReleaseDC(nullptr,dc);return count;
}
std::vector<DWORD> ScreenPixels(RECT rect){
    int width=rect.right-rect.left,height=rect.bottom-rect.top;std::vector<DWORD> pixels(width*height);
    HDC dc=GetDC(nullptr),copy=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};DWORD*data=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&data),nullptr,0);auto old=SelectObject(copy,bitmap);
    BitBlt(copy,0,0,width,height,dc,rect.left,rect.top,SRCCOPY);GdiFlush();std::copy(data,data+pixels.size(),pixels.begin());
    SelectObject(copy,old);DeleteObject(bitmap);DeleteDC(copy);ReleaseDC(nullptr,dc);return pixels;
}
void Capture(View&v,const wchar_t*path){
    int width=v.bounds.right-v.bounds.left,height=v.bounds.bottom-v.bounds.top;HDC dc=GetDC(nullptr),copy=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};void*data=nullptr;
    HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);auto old=SelectObject(copy,bmp);BitBlt(copy,0,0,width,height,dc,v.bounds.left,v.bounds.top,SRCCOPY);
    FILE*f=nullptr;_wfopen_s(&f,path,L"wb");if(f){BITMAPFILEHEADER header{};header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+width*height*4;fwrite(&header,sizeof(header),1,f);fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,f);fwrite(data,width*height*4,1,f);fclose(f);}
    SelectObject(copy,old);DeleteObject(bmp);DeleteDC(copy);ReleaseDC(nullptr,dc);
}
}
int wmain(int argc,wchar_t**argv){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(argc>2&&!wcscmp(argv[1],L"--fixture"))return RunFixtures(wcstoul(argv[2],nullptr,10));
    CoInitialize(nullptr);g_instance=GetModuleHandle(nullptr);g_probe=true;FixtureProcess child;if(!child.Start())return 1;
    App app;App::instance=&app;if(!app.Initialize()||!app.EnsureDevice())return 2;SetWindowText(app.controller,L"Window Scatter isolated minimized test");
    realSize=app.api.querySize;realThumb=app.api.createThumb;app.api.querySize=QuerySize;app.api.createThumb=CreateThumb;
    auto pv=std::make_unique<View>();pv->monitor=MonitorFromWindow(child.Window(0),MONITOR_DEFAULTTONEAREST);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfo(pv->monitor,&monitor);
    pv->bounds={monitor.rcWork.left+60,monitor.rcWork.top+60,monitor.rcWork.left+1020,monitor.rcWork.top+800};app.views.push_back(std::move(pv));View&v=*app.views[0];
    for(int n=0;n<6;n++)App::AddWindow(child.Window(n),reinterpret_cast<LPARAM>(&app));
    Check(v.items.size()==4,"enumeration includes minimized and never-painted windows, excludes hidden/tool windows");
    if(v.items.size()!=4){app.Cleanup();App::instance=nullptr;CoUninitialize();return 3;}
    Check(v.items[1].minimized&&v.items[2].minimized&&v.items[3].minimized&&!v.items[0].minimized,"minimized state is tracked independently of ordinary visibility");
    Check(v.items[1].original.w==800&&v.items[1].original.h==450&&std::abs(v.items[1].original.x)<2000,"layout uses saved restore bounds instead of iconic coordinates");
    RECT maximized{};App::Bounds(child.Window(2),maximized);Check(EqualRect(&maximized,&monitor.rcWork),"maximized-before-minimize uses the monitor work area");
    if(!app.CreateView(v))return 4;
    app.windowStateHook=SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND,nullptr,App::WindowStateEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    ComPtr<IDCompositionSurface> bg;ComPtr<IDCompositionVisual2> bgv;app.Paint(bg,960,740,true,[](ID2D1DeviceContext*c){c->Clear(D2D1::ColorF(0x202329));});app.device->CreateVisual(&bgv);bgv->SetContent(bg.Get());v.root->AddVisual(bgv.Get(),FALSE,v.windows.Get());
    // Exercise both failure paths with known child windows, without screen capture.
    failSize=child.Window(3);failThumb=child.Window(2);
    for(size_t n=0;n<v.items.size();n++){
        auto&i=v.items[n];i.target={40.+(n%2)*470.,35.+(n/2)*345.,410.,250.};
        Check(app.CreateItem(v,i),"preview creation succeeds for cached and unavailable DWM surfaces");
        double scale=n?std::min(280./i.original.w,160./i.original.h):410./i.original.w;i.target.w=i.original.w*scale;i.target.h=i.original.h*scale;
        if(!i.position)return 5;Check(app.CreateTitle(v,i),"title and minimized-state label are created");v.windows->AddVisual(i.position.Get(),FALSE,nullptr);
    }
    Check(v.items[0].iconBitmap&&v.items[1].iconBitmap&&!v.items[2].iconBitmap&&!v.items[3].iconBitmap,"application icons are included; windows without icons retain plain text");
    HANDLE queries=GetProp(child.Window(1),L"IconQueries");IWICBitmap*cachedIcon=v.items[1].iconBitmap.Get();app.CreateTitle(v,v.items[1]);
    Check(cachedIcon==v.items[1].iconBitmap.Get()&&queries==GetProp(child.Window(1),L"IconQueries"),"title reflow reuses the icon without messaging its application again");
    Check(v.items[1].thumbnail&&!v.items[2].thumbnail&&!v.items[3].thumbnail&&app.minimizedPlaceholder,"failed DWM surfaces share a small placeholder; cached surface remains native");
    auto packed=App::Arrange(v);Check(packed.size()==4,"ordinary and minimized previews both receive layout positions");
    if(packed.size()!=4)return 7;
    double leftmost=packed[1].x,rightmost=packed[1].right(),topmost=packed[1].y,bottommost=packed[1].bottom();
    for(int n:{2,3}){leftmost=std::min(leftmost,packed[n].x);rightmost=std::max(rightmost,packed[n].right());topmost=std::min(topmost,packed[n].y);bottommost=std::max(bottommost,packed[n].bottom());}
    Check(std::abs((leftmost+rightmost)*.5-480)<.1&&std::abs(bottommost+26*v.dpi-(740-8*v.dpi))<.1,"minimized previews sit near the bottom center with captions and selection outline clear of the taskbar");
    Check(packed[0].bottom()+26*v.dpi<topmost&&packed[1].w>100*v.dpi&&bottommost-topmost<=90*v.dpi,"the main layout keeps its space above a shallow bottom shelf");
    View crowded;crowded.bounds=v.bounds;crowded.dpi=v.dpi;
    for(int n=0;n<10;n++){Item item;item.original={0,0,800,450};item.minimized=n!=0;crowded.items.push_back(std::move(item));}
    auto row=App::Arrange(crowded);bool singleRow=row.size()==10;
    if(singleRow)for(size_t n=1;n<row.size();++n)singleRow&=std::abs(row[n].bottom()-row[1].bottom())<.1&&row[n].h<=90*v.dpi&&(n==1||row[n].x>=row[n-1].right());
    Check(singleRow&&row[1].y>=740-8*v.dpi-116*v.dpi,"nine minimized windows remain in one shallow row without overlapping or adding rows");
    for(size_t n=0;n<v.items.size();++n){v.items[n].target=packed[n];app.CreateTitle(v,v.items[n]);}
    auto hidden=App::HiddenPose(v.items[1]);
    Check(std::abs(hidden.w/v.items[1].target.w-.94)<.001&&std::abs(hidden.h/v.items[1].target.h-.94)<.001&&hidden.y>=monitor.rcMonitor.bottom-v.bounds.top,"minimized previews start fully below the display at 94 percent uniform scale");
    // Keep the ordinary preview's full-size starting pose out of this pixel
    // sample; otherwise its white surface correctly shows behind the small tile.
    v.items[0].original.x=v.items[0].original.y=-2000;
    POINT minCenter{LONG(v.items[1].target.cx()),LONG(v.items[1].target.cy())};
    app.StaticPose(0);app.device->Commit();app.device->WaitForCommitCompletion();SetWindowPos(v.hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);DwmFlush();Sleep(250);
    Check(Pixel(v,minCenter.x,minCenter.y)==RGB(0x20,0x23,0x29),"minimized tile starts at the taskbar edge instead of popping into its destination");
    app.StaticPose(1);app.device->Commit();app.device->WaitForCommitCompletion();DwmFlush();Sleep(250);
    COLORREF normal=Pixel(v,LONG(v.items[0].target.cx()),LONG(v.items[0].target.cy())),small=Pixel(v,minCenter.x,minCenter.y);
    COLORREF left=Pixel(v,LONG(v.items[1].target.x)+2,minCenter.y),right=Pixel(v,LONG(v.items[1].target.right())-3,minCenter.y);
    Check(GetRValue(normal)>245&&normal==small,"minimized preview keeps the original color and opacity");
    Check(GetBValue(left)>GetRValue(left)+70&&GetRValue(right)>GetBValue(right)+70,"retained preview preserves both colored edges without cropping");
    auto&mini=v.items[1];int labelTop=int(mini.target.bottom()-v.dpi);
    int iconPixels=GreenPixels(v,int(mini.target.x),labelTop,int(mini.target.w),int(24*v.dpi));
    Check(iconPixels>=100,"the supplied green application icon is rendered inside the existing title surface");
    Check(IsIconic(child.Window(1))&&IsIconic(child.Window(2))&&IsIconic(child.Window(3)),"rendering never restores minimized source windows");
    app.phase=Phase::Settled;app.progress=1;
    Check(app.Hit(v,minCenter)==1&&app.GestureChoice(v.bounds.left+minCenter.x,v.bounds.top+minCenter.y)==child.Window(1),"mouse and spatial gesture selection include the minimized tile");
    Check(app.Hit(v,POINT{minCenter.x,LONG(v.items[1].target.bottom()+10*v.dpi)})==1,"clicking the attached title selects its window instead of canceling overview");
    if(argc>2&&!wcscmp(argv[1],L"--capture")){
        Capture(v,argv[2]);app.Select(child.Window(1));
        if(app.phase==Phase::Hidden)return 8;
        app.device->Commit();app.device->WaitForCommitCompletion();DwmFlush();Sleep(120);
        Capture(v,(std::wstring(argv[2])+L".selected.bmp").c_str());app.ClearSelection();
    }
    // Exercise the actual press/release handlers and native state notifications.
    auto&ordinary=v.items[0];Box savedOriginal=ordinary.original;app.settings.duration=300;app.Select(child.Window(0));
    Box minimizeBox=App::MinimizeBox(v,ordinary.target),closeBox=App::CloseBox(v,ordinary.target);
    POINT minimizePoint{LONG(minimizeBox.cx()),LONG(minimizeBox.cy())},closePoint{LONG(closeBox.cx()),LONG(closeBox.cy())};
    auto mouse=[&](UINT message,POINT p){app.Message(v.hwnd,message,0,MAKELPARAM(p.x,p.y));};
    Check(app.ButtonAt(v,0,minimizePoint)==WindowButton::Minimize&&app.ButtonAt(v,0,closePoint)==WindowButton::Close&&minimizeBox.right()<closeBox.x,"minimize is a separate button directly left of close");
    app.HoverButtons(v,0,minimizePoint);Check(ordinary.hotButton==WindowButton::Minimize,"minimize has its own hover state");
    if(argc>2&&!wcscmp(argv[1],L"--capture")){app.device->Commit();app.device->WaitForCommitCompletion();DwmFlush();Sleep(120);Capture(v,(std::wstring(argv[2])+L".buttons.bmp").c_str());}
    mouse(WM_LBUTTONDOWN,minimizePoint);mouse(WM_LBUTTONUP,closePoint);Pump(60);
    Check(!IsIconic(child.Window(0))&&!GetProp(child.Window(0),L"CloseRequests")&&app.phase==Phase::Settled,"dragging from minimize onto close cancels both actions and keeps overview open");
    SetProp(child.Window(0),L"RejectMinimize",reinterpret_cast<HANDLE>(1));mouse(WM_LBUTTONDOWN,minimizePoint);mouse(WM_LBUTTONUP,minimizePoint);Pump(80);
    Check(!ordinary.minimized&&!IsIconic(child.Window(0))&&app.phase==Phase::Settled,"an application that ignores minimize remains in the main layout");RemoveProp(child.Window(0),L"RejectMinimize");
    Box beforeMinimize=ordinary.target;mouse(WM_LBUTTONDOWN,minimizePoint);mouse(WM_LBUTTONUP,minimizePoint);Pump(100);
    Check(IsIconic(child.Window(0))&&ordinary.minimized&&app.phase==Phase::Opening&&!app.chosen,"clicking minimize changes native state and keeps overview open");
    Check(std::abs(ordinary.motionFrom.x-beforeMinimize.x)<.1&&std::abs(ordinary.motionFrom.y-beforeMinimize.y)<.1&&ordinary.target.y>beforeMinimize.y&&ordinary.target.h<=90*v.dpi,"the minimized preview animates continuously from the main area into the bottom row");
    Pump(350);app.FinishTransition();app.device->WaitForCommitCompletion();DwmFlush();Sleep(100);
    COLORREF minimizedLeft=Pixel(v,LONG(ordinary.target.x)+2,LONG(ordinary.target.cy())),minimizedRight=Pixel(v,LONG(ordinary.target.right())-3,LONG(ordinary.target.cy()));
    Check(GetBValue(minimizedLeft)>GetRValue(minimizedLeft)+70&&GetRValue(minimizedRight)>GetBValue(minimizedRight)+70,"minimizing a live preview retains both complete colored edges");
    app.Select(child.Window(0));Check(!App::HasMinimizeButton(ordinary)&&!App::RequestMinimize(ordinary),"already-minimized previews do not expose or repeat the minimize action");
    closeBox=App::CloseBox(v,ordinary.target);closePoint={LONG(closeBox.cx()),LONG(closeBox.cy())};mouse(WM_LBUTTONDOWN,closePoint);mouse(WM_LBUTTONUP,closePoint);Pump(60);
    Check(reinterpret_cast<UINT_PTR>(GetProp(child.Window(0),L"CloseRequests"))==1&&IsIconic(child.Window(0)),"the adjacent close button still sends exactly one native close request");
    ShowWindowAsync(child.Window(0),SW_RESTORE);Pump(400);app.FinishTransition();app.ClearSelection();
    Check(!ordinary.minimized&&App::HasMinimizeButton(ordinary),"restoring a window returns it to the main area with the minimize control available again");ordinary.original=savedOriginal;
    app.phase=Phase::Hidden;app.progress=0;app.previous=child.Window(0);app.settings.duration=700;app.StaticPose(0);app.Animate(1);Sleep(160);
    Box opening=app.CurrentBox(v.items[1]);
    Check(opening.w>hidden.w&&opening.w<v.items[1].target.w&&opening.y>v.items[1].target.y&&opening.y<hidden.y,"opening slides upwards with only subtle scaling");
    app.Close(nullptr);
    Check(v.items[1].motionFrom.w>=opening.w&&v.items[1].motionFrom.w<v.items[1].target.w,"closing continues from the in-flight opening pose");
    Sleep(70);Box closing=app.CurrentBox(v.items[1]);app.Reopen();
    Check(v.items[1].motionFrom.w>=hidden.w&&v.items[1].motionFrom.w<=closing.w+1,"reopening continues from the in-flight closing pose without resetting");
    Check(IsIconic(child.Window(1)),"interrupting and reversing overview animations never restores minimized sources");
    // Reproduce a stale iconic crop: a tiny source region is scaled into the
    // full preview. Restoration must replace it with the actual visible frame.
    DWM_THUMBNAIL_PROPERTIES stale{};stale.dwFlags=DWM_TNP_RECTSOURCE|DWM_TNP_RECTDESTINATION|DWM_TNP_VISIBLE|DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY|0x04000000;
    stale.rcSource={0,0,160,40};stale.rcDestination={0,0,mini.pixels.cx,mini.pixels.cy};stale.opacity=255;stale.fVisible=TRUE;
    ComPtr<IDCompositionVisual2> staleSurface;HTHUMBNAIL staleThumbnail=nullptr;
    HRESULT staleResult=app.api.createThumb(v.hwnd,mini.source,2,&stale,app.device.Get(),reinterpret_cast<void**>(staleSurface.GetAddressOf()),&staleThumbnail);
    if(SUCCEEDED(staleResult)){staleSurface->SetTransform(mini.scale.Get());mini.surface->SetTransform(nullptr);mini.content->RemoveVisual(mini.surface.Get());mini.content->AddVisual(staleSurface.Get(),TRUE,nullptr);mini.surface=std::move(staleSurface);staleSurface.Reset();mini.thumbnail=staleThumbnail;}
    Check(SUCCEEDED(staleResult),"fixture installs a stale iconic source crop before restoration");
    app.phase=Phase::Settled;app.progress=1;app.StaticPose(1);app.Close(child.Window(1));
    Check(v.items[1].motionFrom.w>=v.items[1].target.w&&v.items[1].motionTo.w>v.items[1].target.w&&v.items[1].motionTo.w==v.items[1].original.w,"selecting a minimized preview expands from its tile directly to the restore rectangle");
    Check(v.items[2].motionTo.w==v.items[2].minimizedOrigin.w&&v.items[3].motionTo.w==v.items[3].minimizedOrigin.w&&v.items[2].motionTo.y>=monitor.rcMonitor.bottom-v.bounds.top,"unselected minimized previews slide below the display without collapsing");
    Pump(150);Box restoring=app.CurrentBox(v.items[1]);
    Check(restoring.w>v.items[1].target.w&&restoring.w<v.items[1].original.w,"selected minimized preview passes through intermediate expanded sizes");
    RECT restoredFrame{};App::Bounds(child.Window(1),restoredFrame);auto restoredTarget=v.items[1].motionTo;
    Check(v.items[1].restoredSampleValid&&v.items[1].restoredSample.right-v.items[1].restoredSample.left==restoredFrame.right-restoredFrame.left&&v.items[1].restoredSample.bottom-v.items[1].restoredSample.top==restoredFrame.bottom-restoredFrame.top,"restoration replaces the stale crop with the entire actual visible frame");
    Check(restoredTarget.x==restoredFrame.left-v.bounds.left&&restoredTarget.y==restoredFrame.top-v.bounds.top&&restoredTarget.w==restoredFrame.right-restoredFrame.left&&restoredTarget.h==restoredFrame.bottom-restoredFrame.top,"the endpoint is corrected to actual restored DWM visible-frame bounds");
    app.StaticPose(0);Check(v.items[1].motionFrom.w==v.items[1].original.w,"the final committed frame retains the restored size instead of resetting to the taskbar");
    // Keep both fictional surfaces above unrelated applications without
    // relying on foreground activation (the user may be typing elsewhere).
    WNDCLASS backdropClass{};backdropClass.lpfnWndProc=DefWindowProc;backdropClass.hInstance=g_instance;backdropClass.lpszClassName=L"WindowScatter.HandoffBackdrop";backdropClass.hbrBackground=CreateSolidBrush(RGB(0x20,0x23,0x29));RegisterClass(&backdropClass);
    HWND backdrop=CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,backdropClass.lpszClassName,L"WindowScatter isolated handoff background",WS_POPUP,v.bounds.left,v.bounds.top,960,740,nullptr,nullptr,g_instance,nullptr);
    SetWindowPos(backdrop,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);UpdateWindow(backdrop);
    SetWindowPos(child.Window(1),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetWindowPos(v.hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    app.device->Commit();app.device->WaitForCommitCompletion();DwmFlush();Sleep(260);
    RECT client{};GetClientRect(child.Window(1),&client);MapWindowPoints(child.Window(1),nullptr,reinterpret_cast<POINT*>(&client),2);
    View crop;crop.bounds=client;
    auto previewPixels=ScreenPixels(client);if(argc>2)Capture(crop,(std::wstring(argv[2])+L".handoff-preview.bmp").c_str());
    ShowWindow(v.hwnd,SW_HIDE);DwmFlush();Sleep(80);auto nativePixels=ScreenPixels(client);if(argc>2)Capture(crop,(std::wstring(argv[2])+L".handoff-native.bmp").c_str());int maxDelta=0;
    int bodyDelta=0,width=client.right-client.left,height=client.bottom-client.top,corner=int(std::ceil(16*v.dpi));
    for(size_t n=0;n<previewPixels.size();++n)for(int shift:{0,8,16}){
        int delta=std::abs(int((previewPixels[n]>>shift)&255)-int((nativePixels[n]>>shift)&255));maxDelta=std::max(maxDelta,delta);
        // Native frame and compositor clips have different edge AA. Compare
        // geometry using the full colored vertical edges outside corner arcs.
        int x=int(n)%width,y=int(n)/width;if(y<height-corner||(x>=corner&&x<width-corner))bodyDelta=std::max(bodyDelta,delta);
    }
    printf("Handoff channel differences: body=%d, including corner AA=%d\n",bodyDelta,maxDelta);Check(bodyDelta<=1,"the final preview matches native client geometry and body pixels without a shift or rescale");
    printf("Restored frame=(%ld,%ld)-(%ld,%ld), client=(%ld,%ld)-(%ld,%ld), sampled source=%ldx%ld\n",restoredFrame.left,restoredFrame.top,restoredFrame.right,restoredFrame.bottom,client.left,client.top,client.right,client.bottom,v.items[1].pixels.cx,v.items[1].pixels.cy);
    SetWindowPos(v.hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    SetWindowPos(child.Window(1),HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    DestroyWindow(backdrop);UnregisterClass(backdropClass.lpszClassName,g_instance);DeleteObject(backdropClass.hbrBackground);
    app.Reopen();
    Check(v.items[1].motionFrom.w==v.items[1].original.w,"interrupting restore and reopening continues from the expanded preview");
    Check(!v.items[1].minimized&&v.items[1].target.bottom()+26*v.dpi<v.items[2].target.y,"a restored window immediately rejoins the main area when reopening interrupts restoration");
    Check(v.items[2].minimized&&v.items[3].minimized&&IsIconic(child.Window(2))&&IsIconic(child.Window(3)),"promotion leaves unrelated minimized windows on the shelf");
    PostMessage(child.Window(1),WM_APP+2,0,0);Pump(150);
    v.items[1].minimized=true;
    auto pendingLayout=App::Arrange(v);for(size_t n=0;n<v.items.size();++n){v.items[n].target=pendingLayout[n];app.CreateTitle(v,v.items[n]);}
    app.phase=Phase::Settled;app.progress=1;app.StaticPose(1);app.CaptureMotion();app.chosen=child.Window(1);app.Animate(0,true);
    app.Reopen();Check(v.items[1].minimized&&IsIconic(child.Window(1)),"a restore still pending in the source application remains on the shelf temporarily");
    ShowWindowAsync(child.Window(1),SW_RESTORE);Pump(200);
    Check(!IsIconic(child.Window(1))&&!v.items[1].minimized&&v.items[1].target.bottom()+26*v.dpi<v.items[2].target.y,"a delayed native restore event promotes the window after reopening without polling");
    Check(!app.restoreHook&&!app.restoreSource,"completed restore tracking releases its temporary location hook");
    PostMessage(child.Window(1),WM_APP+2,0,0);Pump(150);v.items[1].minimized=true;
    app.phase=Phase::Settled;app.StaticPose(1);
    app.phase=Phase::Settled;app.progress=1;app.previous=child.Window(0);app.settings.duration=0;app.Close(nullptr);Pump(150);
    Check(IsIconic(child.Window(1))&&IsIconic(child.Window(2))&&IsIconic(child.Window(3)),"canceling overview leaves every minimized window minimized");
    // Zero-duration closing takes End() directly and must restore a chosen tile.
    app.phase=Phase::Settled;app.previous=child.Window(0);app.Close(child.Window(1));Pump(350);
    Check(!IsIconic(child.Window(1))&&!IsZoomed(child.Window(1)),"selecting a minimized ordinary window restores it without maximizing");
    Check(IsIconic(child.Window(2))&&IsIconic(child.Window(3)),"selection leaves other minimized windows untouched");
    app.phase=Phase::Settled;app.previous=child.Window(0);app.Close(child.Window(2));Pump(350);
    Check(!IsIconic(child.Window(2))&&IsZoomed(child.Window(2)),"selection restores a previously maximized window as maximized");
    // A nonzero animation also issues the restore request promptly, before End.
    PostMessage(child.Window(1),WM_APP+2,0,0);Pump(150);app.phase=Phase::Settled;app.progress=1;app.previous=child.Window(0);app.settings.duration=300;app.Close(child.Window(1));Pump(150);
    Check(!IsIconic(child.Window(1)),"animated selection restores the selected window before animation completion");
    app.End(false);bgv.Reset();bg.Reset();auto device=app.device;auto d3d=app.d3d;app.Cleanup();
    ULONG deviceRefs=device.Detach()->Release(),d3dRefs=d3d.Detach()->Release();
    printf("Final device references: DComp=%lu D3D=%lu\n",deviceRefs,d3dRefs);Check(deviceRefs==0&&d3dRefs==0,"minimized previews and shared fallback release all graphics device references");
    App::instance=nullptr;CoUninitialize();return passed?0:6;
}
