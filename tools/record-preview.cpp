// Documentation-only recorder. Fictional Win32 sources, production compositor.
// No application enumeration, global hotkeys, real wallpaper or settings writes.
#define SCATTER_STANDALONE
#define SCATTER_TEST
#include "../window-scatter.wh.cpp"
#include <gdiplus.h>
#include <stdexcept>
using namespace scatter;
using namespace Gdiplus;
namespace {
constexpr int W=1440,H=900,FPS=30;
constexpr wchar_t ClassName[]=L"WindowScatter.DocumentationFixture";
const wchar_t* titles[]={L"Canvas — Aurora",L"Studio — workspace.ts",L"Notes — A quieter desktop",L"Files — Project Atlas",L"Music — Night Transit",L"Calendar — This week"};
const UINT colors[]={0x517CEC,0x9477E6,0x22A694,0xDDAB55,0xDA7198,0x6C94E3};
bool stop=false;
double tick=0;
void Text(Graphics&g,const wchar_t*s,float x,float y,float size,UINT color=0x203147,bool bold=false,float width=900){
    FontFamily family(L"Segoe UI");Font font(&family,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(Color(255,(color>>16)&255,(color>>8)&255,color&255));
    g.DrawString(s,-1,&font,RectF(x,y,width,size*2),nullptr,&brush);
}
void Rect(Graphics&g,float x,float y,float w,float h,UINT color){SolidBrush b(Color(255,(color>>16)&255,(color>>8)&255,color&255));g.FillRectangle(&b,x,y,w,h);}
void Line(Graphics&g,float x,float y,float x2,float y2,UINT color,float width=1){Pen p(Color(255,(color>>16)&255,(color>>8)&255,color&255),width);g.DrawLine(&p,x,y,x2,y2);}
void PaintFixture(HWND hwnd,HDC dc,int n){
    RECT r{};GetClientRect(hwnd,&r);Bitmap image(r.right,r.bottom,PixelFormat32bppARGB);Graphics g(&image);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    if(n<0){g.Clear(Color(255,18,29,47));Pen arc(Color(255,32,49,72),1.2f);for(int k=0;k<4;k++)g.DrawEllipse(&arc,850-k*130,50-k*110,800+k*100,800+k*100);
        Text(g,L"WINDOW SCATTER",40,H-54,13,0x8A9CB7,true);Text(g,L"FICTIONAL WORKSPACE  /  NATIVE CAPTURE",W-432,H-54,12,0x8A9CB7);}
    else{
        const float width=float(r.right),height=float(r.bottom);const bool dark=n==1;
        g.Clear(dark?Color(255,29,35,49):Color(255,250,251,253));Rect(g,0,0,width,42,dark?0x252D40:0xF1F4F9);Rect(g,18,14,14,14,colors[n]);
        Text(g,titles[n],44,10,15,dark?0xD8E0EF:0x394D69,true);Text(g,L"—     □     ×",width-103,8,15,dark?0x92A3BE:0x70839E);
        if(n==0){
            Rect(g,0,42,150,height-42,0xF0F3FA);Text(g,L"AURORA",20,66,17,0x59739D,true);Text(g,L"Overview",20,115,15,0x517CEC,true);Text(g,L"Collections",20,154,15,0x718199);Text(g,L"Activity",20,193,15,0x718199);
            Text(g,L"Make room for ideas.",178,70,29,0x203147,true);Text(g,L"A fictional design workspace",180,112,16,0x8592A6);
            Rect(g,178,157,width-206,175,0xDCE8FE);Rect(g,196,176,(width-254)*.54f,135,0x9DBCF4);Rect(g,211,191,75,104,0xCEE0FE);
            Rect(g,220+(width-254)*.54f,176,(width-254)*.36f,63,0xB6ADE8);Rect(g,220+(width-254)*.54f,249,(width-254)*.36f,62,0xADCDBF);
            Text(g,L"THE WEEKLY EDIT",181,352,13,0x8592A6,true);Text(g,L"Small details. A clearer view.",181,384,23,0x203147,true);
            for(int k=0;k<3;k++){Rect(g,180+k*150,438,133,5,k==0?0x517CEC:0xDFE6F1);}
        }else if(n==1){
            Rect(g,0,42,142,height-42,0x242C3E);Text(g,L"EXPLORER",17,65,12,0x8290AB,true);Text(g,L"workspace",17,106,14,0xCCD6E8);Text(g,L"  src",18,143,14,0x93A4BF);Text(g,L"    app.ts",18,177,14,0xACB9CC);Text(g,L"    layout.ts",18,209,14,0xACB9CC);
            Text(g,L"workspace.ts",167,62,14,0xABB9D1);Line(g,159,95,width-24,95,0x36415B);
            const wchar_t*code[]={L"const workspace = {",L"  name: 'Atlas',",L"  view: 'overview',",L"  windows: activeWindows,",L"};",L"",L"function makeRoom() {",L"  return compose(workspace);",L"}",L"",L"// Keep the ideas in view."};
            for(int k=0;k<11;k++){Text(g,std::to_wstring(k+1).c_str(),164,119+k*27,14,0x556681);Text(g,code[k],199,119+k*27,16,k==0||k==6?0xC4A5F4:k==10?0x789980:0xBECDE4);}
            Rect(g,142,height-31,width-142,31,0x2F3F60);Text(g,L"main  ·  TypeScript  ·  UTF-8",164,height-26,12,0xB7CCE8);
        }else if(n==2){
            Text(g,L"FIELD NOTES",30,73,12,0x22A694,true);Text(g,L"A quieter desktop",30,107,28,0x263D46,true);Text(g,L"Tuesday, 14 October",31,154,14,0x8B9AA2);
            Line(g,30,190,width-30,190,0xDFE8E7);Text(g,L"Leave space for the next idea.",31,215,19,0x527069);
            const wchar_t*rows[]={L"Gather the open threads",L"Find the window you need",L"Return to the work"};for(int k=0;k<3;k++){Rect(g,32,274+k*39,13,13,k==0?0x22A694:0xE0EAE7);Text(g,rows[k],59,267+k*39,16,0x506A63);}
        }else if(n==3){
            Text(g,L"Project Atlas",28,73,25,0x30465E,true);Rect(g,27,123,width-54,34,0xEAF0F7);Text(g,L"Home  /  Projects  /  Atlas",39,129,13,0x7990A9);
            const wchar_t*rows[]={L"Design studies",L"Reference library",L"Project notes",L"Release assets"};for(int k=0;k<4;k++){Rect(g,31,183+k*41,25,19,0xE7BD73);Rect(g,31,179+k*41,13,5,0xE7BD73);Text(g,rows[k],74,178+k*41,16,0x48617D);Line(g,29,212+k*41,width-30,212+k*41,0xEBEFF6);}
        }else if(n==4){
            Rect(g,25,68,145,145,0xDDABC7);Rect(g,43,86,109,109,0x927AB0);Text(g,L"NIGHT",51,115,23,0xF9EAF4,true);Text(g,L"TRANSIT",50,146,20,0xF9EAF4,true);
            Text(g,L"Night Transit",198,85,27,0x3E3654,true);Text(g,L"A fictional playlist",200,132,16,0x948598);Text(g,L"Quiet Motion",200,175,18,0x655778);
            Line(g,32,260,width-33,260,0xE0D7E5,4);Line(g,32,260,190+float(fmod(tick,6))*15,260,0xA47CB5,4);Text(g,L"02:14                         04:32",31,278,12,0x9B8AA4);
        }else{
            Text(g,L"This week",27,70,25,0x324D72,true);Text(g,L"Make time for focused work",29,113,15,0x8394AA);
            const wchar_t*days[]={L"MON",L"TUE",L"WED",L"THU",L"FRI"};for(int k=0;k<5;k++){float x=28+k*(width-56)/5;Text(g,days[k],x,159,11,0x8D9DB2,true);Rect(g,x,190,(width-75)/5,73+k%2*29,k==1?0xB9D4F8:0xE8EEF7);}
        }
    }
    Graphics out(dc);out.DrawImage(&image,0,0);
}
LRESULT CALLBACK Procedure(HWND w,UINT m,WPARAM p,LPARAM l){
    if(m==WM_NCCREATE)SetWindowLongPtr(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(l)->lpCreateParams));
    if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);PaintFixture(w,dc,int(GetWindowLongPtr(w,GWLP_USERDATA))-1);EndPaint(w,&ps);return 0;}
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_KEYDOWN&&p==VK_ESCAPE){stop=true;return 0;}
    return DefWindowProc(w,m,p,l);
}
void Pump(App&app){MSG m;while(PeekMessage(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}if(app.timer&&WaitForSingleObject(app.timer,0)==WAIT_OBJECT_0&&app.phase!=Phase::Hidden)app.FinishTransition();}
bool SafeRegion(RECT area){
    for(HWND w=GetTopWindow(nullptr);w;w=GetWindow(w,GW_HWNDNEXT)){if(!IsWindowVisible(w)||IsIconic(w))continue;DWORD cloaked=0;DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)continue;
        RECT r{},intersection{};GetWindowRect(w,&r);if(!IntersectRect(&intersection,&r,&area))continue;DWORD pid=0;GetWindowThreadProcessId(w,&pid);
        if(pid!=GetCurrentProcessId()){wchar_t name[128]{};GetClassName(w,name,128);fwprintf(stderr,L"Occluding process %lu, window class %ls.\n",pid,name);return false;}
        if(r.left<=area.left&&r.top<=area.top&&r.right>=area.right&&r.bottom>=area.bottom)return true;
    }return false;
}
struct Recorder {
    HANDLE input=nullptr;PROCESS_INFORMATION process{};HDC screen=nullptr,dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;DWORD*data=nullptr;
    Recorder(const wchar_t*ffmpeg,const wchar_t*output){
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE read=nullptr;CreatePipe(&read,&input,&sa,0);SetHandleInformation(input,HANDLE_FLAG_INHERIT,0);
        HANDLE null=CreateFile(L"NUL",GENERIC_WRITE|GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        STARTUPINFO si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdInput=read;si.hStdOutput=null;si.hStdError=null;
        std::wstring command=L"\""+std::wstring(ffmpeg)+L"\" -hide_banner -loglevel error -y -f rawvideo -pixel_format bgr0 -video_size 1440x900 -framerate 30 -i pipe:0 -an -c:v ffv1 -threads 4 \""+output+L"\"";
        BOOL ok=CreateProcess(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&process);CloseHandle(read);CloseHandle(null);if(!ok)throw std::runtime_error("ffmpeg could not start");
        screen=GetDC(nullptr);dc=CreateCompatibleDC(screen);BITMAPINFO bi{};bi.bmiHeader={sizeof(BITMAPINFOHEADER),W,-H,1,32,BI_RGB};bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,reinterpret_cast<void**>(&data),nullptr,0);old=SelectObject(dc,bitmap);
    }
    bool Frame(RECT area){if(!SafeRegion(area))return false;BitBlt(dc,0,0,W,H,screen,area.left,area.top,SRCCOPY|CAPTUREBLT);GdiFlush();DWORD sent=0,total=W*H*4;while(sent<total){DWORD n=0;if(!WriteFile(input,reinterpret_cast<BYTE*>(data)+sent,total-sent,&n,nullptr))return false;sent+=n;}return true;}
    ~Recorder(){if(input)CloseHandle(input);WaitForSingleObject(process.hProcess,15000);CloseHandle(process.hThread);CloseHandle(process.hProcess);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);}
};
}
int wmain(int argc,wchar_t**argv){
    if(argc<4){fwprintf(stderr,L"Usage: record-preview ffmpeg.exe output.mkv overview|minimize\n");return 2;}
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitialize(nullptr);ULONG_PTR token;GdiplusStartupInput startup;GdiplusStartup(&token,&startup,nullptr);
    g_instance=GetModuleHandle(nullptr);g_probe=true;g_stopEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);App app;App::instance=&app;
    if(!app.Initialize()||!app.EnsureDevice())return 3;app.settings.duration=666;
    WNDCLASS wc{};wc.hInstance=g_instance;wc.lpfnWndProc=Procedure;wc.lpszClassName=ClassName;RegisterClass(&wc);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfo(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&monitor);
    if(monitor.rcWork.right-monitor.rcWork.left<W||monitor.rcWork.bottom-monitor.rcWork.top<H){fprintf(stderr,"Recording needs a 1440 x 900 work area.\n");return 11;}
    int left=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-W)/2,top=monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-H)/2;
    RECT area{left,top,left+W,top+H};HWND previousFocus=GetForegroundWindow();HWND cover=CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,ClassName,L"Window Scatter documentation capture",WS_POPUP,area.left,area.top,W,H,nullptr,nullptr,g_instance,nullptr);
    SetWindowPos(cover,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);UpdateWindow(cover);std::vector<HWND> sources;int result=0;
    const RECT geometry[]={{65,80,825,600},{545,270,1245,770},{950,62,1380,492},{118,443,598,823},{290,410,890,790},{835,352,1335,692}};
    for(int n=0;n<6;n++){auto r=geometry[n];HWND w=CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,ClassName,titles[n],WS_POPUP|WS_MINIMIZEBOX|WS_SYSMENU,area.left+r.left,area.top+r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,g_instance,reinterpret_cast<void*>(INT_PTR(n+1)));
        BOOL disabled=TRUE;DwmSetWindowAttribute(w,DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled));DWORD corner=DWMWCP_ROUND;DwmSetWindowAttribute(w,DWMWA_WINDOW_CORNER_PREFERENCE,&corner,sizeof(corner));
        sources.push_back(w);SetWindowPos(w,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);UpdateWindow(w);}
    DwmFlush();Sleep(150);ShowWindow(sources[4],SW_SHOWMINNOACTIVE);ShowWindow(sources[5],SW_SHOWMINNOACTIVE);DwmFlush();
    auto v=std::make_unique<View>();v->bounds=area;v->monitor=MonitorFromRect(&area,MONITOR_DEFAULTTONEAREST);if(!app.CreateView(*v))return 4;
    if(FAILED(app.Paint(v->wallpaperSurface,W,H,true,[](auto*c){c->Clear(D2D1::ColorF(0x121D2F));ComPtr<ID2D1SolidColorBrush>b;c->CreateSolidColorBrush(D2D1::ColorF(0x203148),&b);for(int k=0;k<4;k++)c->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(1250-k*80,450-k*60),400+k*50,400+k*50),b.Get(),1.2f);})))return 5;
    app.device->CreateVisual(&v->desktop);v->desktop->SetContent(v->wallpaperSurface.Get());v->root->AddVisual(v->desktop.Get(),FALSE,v->windows.Get());
    for(int n=0;n<6;n++){RECT r{};App::Bounds(sources[n],r);Item i;i.source=sources[n];i.minimized=IsIconic(sources[n]);i.original={double(r.left-area.left),double(r.top-area.top),double(r.right-r.left),double(r.bottom-r.top)};if(!app.CreateItem(*v,i))return 6;v->items.push_back(std::move(i));}
    auto layout=App::Arrange(*v,false);if(layout.size()!=6)return 7;for(int n=0;n<6;n++){v->items[n].target=layout[n];if(!app.CreateTitle(*v,v->items[n]))return 8;}
    app.views.push_back(std::move(v));app.FollowNativeZOrder();app.StaticPose(0);app.device->Commit();app.device->WaitForCommitCompletion();
    auto show=[&]{for(auto&view:app.views)SetWindowPos(view->hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);app.phase=Phase::Opening;app.Animate(1);};
    const bool minimize=wcscmp(argv[3],L"minimize")==0;const int frames=minimize?240:225;
    if(minimize){show();while(app.phase!=Phase::Settled){Pump(app);Sleep(2);}}
    try{Recorder recorder(argv[1],argv[2]);double started=app.Now();for(int f=0;f<frames&&!stop;f++){
        if(GetAsyncKeyState(VK_ESCAPE)&0x8000){stop=true;result=12;break;}
        tick=f/double(FPS);if(!minimize){if(f==30)show();if(f==78)app.Select(sources[0]);if(f==120)app.Select(sources[1]);if(f==174)app.Close(sources[1]);}
        else{if(f==15)app.Select(sources[2]);if(f==55){for(auto&view:app.views)for(auto&i:view->items)if(i.source==sources[2])App::RequestMinimize(i);}
            if(f==58){Pump(app);app.ReconcileWindowStates();}if(f==125)app.Select(sources[2]);if(f==188)app.Close(sources[2]);}
        if(!IsIconic(sources[4])){InvalidateRect(sources[4],nullptr,FALSE);UpdateWindow(sources[4]);}
        Pump(app);if(!recorder.Frame(area)){fprintf(stderr,"Capture interrupted: recording area was covered by another process, or encoder failed.\n");result=9;break;}
        const double due=started+(f+1)/double(FPS);while(app.Now()<due){Pump(app);Sleep(1);}
    }}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());result=10;}
    app.End(false,false);for(HWND w:sources)DestroyWindow(w);DestroyWindow(cover);if(IsWindow(previousFocus))SetForegroundWindow(previousFocus);app.Cleanup();CloseHandle(g_stopEvent);g_stopEvent=nullptr;App::instance=nullptr;CoUninitialize();GdiplusShutdown(token);
    printf("capture_result=%d fictional_sources=6 production_renderer=1\n",result);return result;
}
