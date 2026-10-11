#include "settings-window.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <oleacc.h>
#include <algorithm>
#include <cmath>
#include <memory>

namespace native {
using namespace Gdiplus;
namespace {
enum {Startup=1010,Enabled,Memory,Duration,Radius,Exit,Retry,GitHub,Notices};
constexpr UINT kRefresh=WM_APP+104;
Color C(COLORREF c,BYTE alpha=255){return Color(alpha,GetRValue(c),GetGValue(c),GetBValue(c));}
std::unique_ptr<GraphicsPath> Rounded(float x,float y,float width,float height,float radius){
    auto path=std::make_unique<GraphicsPath>();float diameter=std::min({radius*2,width,height});
    path->AddArc(x,y,diameter,diameter,180,90);path->AddArc(x+width-diameter,y,diameter,diameter,270,90);
    path->AddArc(x+width-diameter,y+height-diameter,diameter,diameter,0,90);path->AddArc(x,y+height-diameter,diameter,diameter,90,90);path->CloseFigure();return path;
}
void FillRounded(Graphics&g,COLORREF color,float x,float y,float w,float h,float radius){auto path=Rounded(x,y,w,h,radius);SolidBrush brush(C(color));g.FillPath(&brush,path.get());}
void Text(Graphics&g,const wchar_t*text,COLORREF color,float size,RectF rect,bool bold=false,StringAlignment align=StringAlignmentNear){
    Font font(L"Microsoft YaHei UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(C(color));StringFormat format;
    format.SetAlignment(align);format.SetLineAlignment(StringAlignmentNear);format.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(text,-1,&font,rect,&format,&brush);
}
void AppMark(Graphics&g,float x,float y,float size,COLORREF accent){
    FillRounded(g,accent,x,y,size,size,size*.25f);
    Pen pen(Color(245,255,255,255),size*.042f);SolidBrush glass(Color(42,255,255,255));
    auto back=Rounded(x+size*.19f,y+size*.23f,size*.47f,size*.38f,size*.06f);g.FillPath(&glass,back.get());g.DrawPath(&pen,back.get());
    auto front=Rounded(x+size*.36f,y+size*.40f,size*.44f,size*.35f,size*.06f);SolidBrush fill(Color(255,255,255,255));g.FillPath(&fill,front.get());
}
void AccessibleName(HWND hwnd,const wchar_t*name){
    IAccPropServices*service=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_AccPropServices,nullptr,CLSCTX_INPROC_SERVER,IID_IAccPropServices,reinterpret_cast<void**>(&service)))){
        service->SetHwndPropStr(hwnd,OBJID_CLIENT,CHILDID_SELF,PROPID_ACC_NAME,name);service->Release();
    }
}
}
bool SettingsWindow::Create(HINSTANCE handle,const Preferences&values,WindowCallbacks actions){
    instance=handle;preferences=values;callbacks=std::move(actions);
    WNDCLASSEX wc{sizeof(wc)};wc.hInstance=instance;wc.lpfnWndProc=Procedure;wc.lpszClassName=kManagerClass;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIcon(instance,MAKEINTRESOURCE(101));wc.hIconSm=wc.hIcon;
    if(!RegisterClassEx(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    hwnd=CreateWindowEx(WS_EX_APPWINDOW,kManagerClass,L"Window Scatter 设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
                        CW_USEDEFAULT,CW_USEDEFAULT,780,620,nullptr,nullptr,instance,this);
    if(!hwnd)return false;
    taskbarMessage=RegisterWindowMessage(L"TaskbarCreated");
    scale=GetDpiForWindow(hwnd)/96.f;
    RECT bounds{0,0,Px(width),Px(height)};AdjustWindowRectExForDpi(&bounds,GetWindowLong(hwnd,GWL_STYLE),FALSE,GetWindowLong(hwnd,GWL_EXSTYLE),GetDpiForWindow(hwnd));
    POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfo(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY),&monitor);
    int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
    SetWindowPos(hwnd,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-w)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-h)/2,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    DWORD corner=2;DwmSetWindowAttribute(hwnd,33,&corner,sizeof(corner));
    startup=Button(L"开机自动启动",Startup,BS_AUTOCHECKBOX);enabled=Button(L"接管系统任务视图",Enabled,BS_AUTOCHECKBOX);
    SetWindowSubclass(startup,ToggleProcedure,1,reinterpret_cast<DWORD_PTR>(this));SetWindowSubclass(enabled,ToggleProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    memory=CreateWindowEx(0,WC_COMBOBOX,L"内存模式",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,
                          0,0,100,200,hwnd,reinterpret_cast<HMENU>(Memory),instance,nullptr);
    for(auto name:{L"自动（推荐）",L"常驻",L"省内存"})SendMessage(memory,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    SetWindowSubclass(memory,ComboProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    COMBOBOXINFO combo{sizeof(combo)};
    if(GetComboBoxInfo(memory,&combo)&&combo.hwndList)SetWindowSubclass(combo.hwndList,MenuProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    duration=CreateWindowEx(0,TRACKBAR_CLASS,L"动画时长（毫秒）",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,0,0,100,30,hwnd,reinterpret_cast<HMENU>(Duration),instance,nullptr);
    radius=CreateWindowEx(0,TRACKBAR_CLASS,L"圆角大小",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,0,0,100,30,hwnd,reinterpret_cast<HMENU>(Radius),instance,nullptr);
    SendMessage(duration,TBM_SETRANGE,TRUE,MAKELPARAM(0,900));SendMessage(duration,TBM_SETPAGESIZE,0,50);SendMessage(duration,TBM_SETLINESIZE,0,10);
    SendMessage(radius,TBM_SETRANGE,TRUE,MAKELPARAM(0,32));SendMessage(radius,TBM_SETPAGESIZE,0,4);
    exitButton=Button(L"退出程序",Exit);retry=Button(L"重新连接",Retry);github=Button(L"GitHub 仓库",GitHub);notices=Button(L"开源许可",Notices);
    AccessibleName(memory,L"内存模式");AccessibleName(duration,L"动画时长，单位毫秒");AccessibleName(radius,L"圆角大小，单位 DIP");
    loading=true;SendMessage(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    SendMessage(enabled,BM_SETCHECK,preferences.enabled?BST_CHECKED:BST_UNCHECKED,0);SendMessage(memory,CB_SETCURSEL,preferences.memory,0);
    SendMessage(duration,TBM_SETPOS,TRUE,preferences.duration);SendMessage(radius,TBM_SETPOS,TRUE,preferences.radius);loading=false;
    Theme();Layout();return true;
}
HWND SettingsWindow::Button(const wchar_t*label,int id,DWORD style){
    return CreateWindowEx(0,WC_BUTTON,label,WS_CHILD|WS_TABSTOP|style,0,0,10,10,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);
}
void SettingsWindow::Theme(){
    DWORD light=1,size=sizeof(light);RegGetValue(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);dark=light==0;
    HIGHCONTRAST contrast{sizeof(contrast)};SystemParametersInfo(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);highContrast=contrast.dwFlags&HCF_HIGHCONTRASTON;
    if(highContrast){colors={GetSysColor(COLOR_WINDOW),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_WINDOWTEXT)};}
    else if(dark)colors={RGB(30,31,33),RGB(239,240,244),RGB(157,160,168),RGB(53,55,59),RGB(86,157,255),RGB(48,65,91),RGB(255,105,105)};
    else colors={RGB(250,250,251),RGB(39,41,45),RGB(122,125,132),RGB(231,232,235),RGB(0,113,227),RGB(229,238,249),RGB(201,49,55)};
    if(backgroundBrush)DeleteObject(backgroundBrush);
    backgroundBrush=CreateSolidBrush(colors.background);
    BOOL immersive=dark&&!highContrast;DwmSetWindowAttribute(hwnd,20,&immersive,sizeof(immersive));
    for(HWND control:{memory,duration,radius})if(control)SetWindowTheme(control,dark?L"DarkMode_Explorer":L"Explorer",nullptr);
    if(hwnd)RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
void SettingsWindow::Layout(){
    if(!hwnd||arranging)return;arranging=true;
    scale=GetDpiForWindow(hwnd)/96.f;RECT client{};GetClientRect(hwnd,&client);width=int(std::lround(client.right/scale));height=int(std::lround(client.bottom/scale));
    int current;{std::lock_guard lock(statusMutex);current=status;}
    int targetHeight=current==3?600:524;
    if(height!=targetHeight){
        RECT frame{0,0,Px(width),Px(targetHeight)};
        AdjustWindowRectExForDpi(&frame,GetWindowLong(hwnd,GWL_STYLE),FALSE,GetWindowLong(hwnd,GWL_EXSTYLE),GetDpiForWindow(hwnd));
        RECT position{};GetWindowRect(hwnd,&position);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        int newHeight=frame.bottom-frame.top,top=std::max(monitor.rcWork.top,std::min(position.top,monitor.rcWork.bottom-newHeight));
        SetWindowPos(hwnd,nullptr,position.left,top,frame.right-frame.left,newHeight,SWP_NOZORDER|SWP_NOACTIVATE);
        height=targetHeight;
    }
    HFONT old=font;font=CreateFont(-Px(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    auto place=[&](HWND control,float x,float y,float w,float h,bool visible){if(!control)return;SetWindowPos(control,nullptr,Px(x),Px(y),Px(w),Px(h),SWP_NOZORDER|SWP_NOACTIVATE);SendMessage(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);ShowWindow(control,visible?SW_SHOWNA:SW_HIDE);};
    float left=32,right=width-32;
    place(startup,right-44,120,44,26,true);place(enabled,right-44,194,44,26,true);
    place(memory,right-160,268,160,180,true);SendMessage(memory,CB_SETITEMHEIGHT,WPARAM(-1),Px(24));SendMessage(memory,CB_SETITEMHEIGHT,0,Px(28));
    place(duration,right-282,338,218,30,true);place(radius,right-282,402,218,30,true);
    place(exitButton,right-84,height-45,84,30,true);
    place(github,left-8,height-45,100,30,true);place(notices,left+100,height-45,88,30,true);
    place(retry,right-90,490,90,30,current==3);
    if(license&&IsWindow(license))SendMessage(GetDlgItem(license,1),WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    if(old)DeleteObject(old);InvalidateRect(hwnd,nullptr,FALSE);arranging=false;
}
void SettingsWindow::Show(){
    SendMessage(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    // Explicitly opening settings must override STARTUPINFO's SW_HIDE (for
    // example after a hidden logon launch). The first ShowWindow call can ignore
    // its requested show mode, whereas SWP_SHOWWINDOW does not.
    if(IsIconic(hwnd))ShowWindow(hwnd,SW_RESTORE);
    SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);SetFocus(startup);
}
void SettingsWindow::Destroy(){if(license)DestroyWindow(license);license=nullptr;if(hwnd)DestroyWindow(hwnd);hwnd=nullptr;if(font)DeleteObject(font);font=nullptr;if(backgroundBrush)DeleteObject(backgroundBrush);backgroundBrush=nullptr;}
void SettingsWindow::SetStatus(int value,const std::wstring&text,const std::wstring&detail){
    {std::lock_guard lock(statusMutex);status=value;statusText=text;statusDetail=detail;}
    if(hwnd){SetProp(hwnd,L"WindowScatter.ConnectionState.v1",reinterpret_cast<HANDLE>(ULONG_PTR(value)));PostMessage(hwnd,kRefresh,0,0);}
}
void SettingsWindow::Save(){
    if(loading)return;
    if(callbacks.save&&!callbacks.save(preferences))MessageBox(hwnd,L"设置未能保存，请检查当前账户的写入权限。",L"Window Scatter",MB_ICONERROR|MB_OK);
    InvalidateRect(hwnd,nullptr,FALSE);
}
void SettingsWindow::Paint(HDC dc){
    RECT client{};GetClientRect(hwnd,&client);
    HDC buffer=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,client.right,client.bottom);HGDIOBJ old=SelectObject(buffer,bitmap);
    Graphics g(buffer);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
    SolidBrush base(C(colors.background));g.FillRectangle(&base,0.f,0.f,float(width),float(height));
    float left=32,right=width-32,content=right-left;
    Pen divider(C(colors.line));
    AppMark(g,left,29,28,colors.accent);
    Text(g,L"Window Scatter",colors.text,20,{left+40,26,content-140,31},true);
    Text(g,kVersion,colors.secondary,11,{right-70,37,70,20},false,StringAlignmentFar);
    Text(g,L"设置自动保存",colors.secondary,12,{left,71,content,23});

    Text(g,L"开机自动启动",colors.text,14,{left,112,content-80,25});
    Text(g,L"登录 Windows 后在后台运行",colors.secondary,12,{left,139,content-80,22});
    g.DrawLine(&divider,left,170.f,right,170.f);
    Text(g,L"接管系统任务视图",colors.text,14,{left,186,content-80,25});
    Text(g,L"使用 Win+Tab 和三指手势",colors.secondary,12,{left,213,content-80,22});
    g.DrawLine(&divider,left,244.f,right,244.f);
    Text(g,L"内存模式",colors.text,14,{left,260,content-185,25});
    const wchar_t*details[]={L"内存紧张时释放图形资源",L"保留图形资源，优先打开速度",L"每次退出总览后释放图形资源"};
    Text(g,details[std::clamp(preferences.memory,0,2)],colors.secondary,12,{left,290,content,22});
    g.DrawLine(&divider,left,325.f,right,325.f);
    Text(g,L"动画时长",colors.text,14,{left,342,200,25});
    Text(g,L"设为 0 可关闭动画",colors.secondary,12,{left,370,content,22});
    Text(g,(std::to_wstring(preferences.duration)+L" ms").c_str(),colors.secondary,12,{right-60,344,60,23},false,StringAlignmentFar);
    Text(g,L"窗口圆角",colors.text,14,{left,406,200,25});
    Text(g,L"选中框跟随系统强调色",colors.secondary,12,{left,434,content,22});
    Text(g,(std::to_wstring(preferences.radius)+L" DIP").c_str(),colors.secondary,12,{right-60,408,60,23},false,StringAlignmentFar);
    int current;std::wstring label,detail;{std::lock_guard lock(statusMutex);current=status;label=statusText;detail=statusDetail;}
    if(current==3){
        g.DrawLine(&divider,left,465.f,right,465.f);
        Text(g,label.c_str(),colors.danger,12,{left,477,content-110,23});
        Text(g,detail.c_str(),colors.secondary,11,{left,503,content-110,36});
    }
    g.Flush();BitBlt(dc,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
}
void SettingsWindow::DrawButton(const DRAWITEMSTRUCT&item){
    int id=GetDlgCtrlID(item.hwndItem);RECT bounds=item.rcItem;float w=(bounds.right-bounds.left)/scale,h=(bounds.bottom-bounds.top)/scale;
    Graphics g(item.hDC);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
    bool pressed=item.itemState&ODS_SELECTED;
    SolidBrush base(C(colors.background));g.FillRectangle(&base,0.f,0.f,w,h);
    if(pressed)FillRounded(g,colors.selection,0,0,w,h,5);
    COLORREF foreground=highContrast&&pressed?GetSysColor(COLOR_HIGHLIGHTTEXT):(id==Exit?colors.secondary:colors.accent);
    wchar_t label[100]{};GetWindowText(item.hwndItem,label,ARRAYSIZE(label));
    Text(g,label,foreground,12,{8,6,w-16,h-6},false,StringAlignmentCenter);
    g.Flush();if((item.itemState&ODS_FOCUS)&&!(SendMessage(item.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){RECT focus=bounds;InflateRect(&focus,-3,-3);DrawFocusRect(item.hDC,&focus);}
}
void SettingsWindow::DrawCombo(const DRAWITEMSTRUCT&item){
    bool selected=(item.itemState&ODS_SELECTED)&&!(item.itemState&ODS_COMBOBOXEDIT);COLORREF bg=selected?colors.selection:colors.background;
    HBRUSH brush=CreateSolidBrush(bg);FillRect(item.hDC,&item.rcItem,brush);DeleteObject(brush);SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,selected?(highContrast?GetSysColor(COLOR_HIGHLIGHTTEXT):colors.accent):colors.text);
    HGDIOBJ old=SelectObject(item.hDC,font);wchar_t text[100]{};
    if(item.itemID!=UINT(-1))SendMessage(item.hwndItem,CB_GETLBTEXT,item.itemID,reinterpret_cast<LPARAM>(text));
    RECT box=item.rcItem;box.left+=Px(9);DrawText(item.hDC,text,-1,&box,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(item.hDC,old);
    if(item.itemState&ODS_FOCUS)DrawFocusRect(item.hDC,&item.rcItem);
}
LRESULT CALLBACK SettingsWindow::ComboProcedure(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto*self=reinterpret_cast<SettingsWindow*>(data);
    if(message==WM_PAINT||message==WM_PRINTCLIENT||message==WM_PRINT){
        PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(control,&paint):reinterpret_cast<HDC>(wp);
        RECT r{};GetClientRect(control,&r);float w=r.right/self->scale,h=r.bottom/self->scale;
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(self->scale,self->scale);
        SolidBrush base(C(self->colors.background));g.FillRectangle(&base,0.f,0.f,w,h);
        bool focused=GetFocus()==control&&!(SendMessage(control,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS);
        auto outline=Rounded(.5f,.5f,w-1,h-1,7);Pen border(C(focused?self->colors.accent:self->colors.line),1.f);g.DrawPath(&border,outline.get());
        Pen chevron(C(self->colors.secondary),1.2f);chevron.SetStartCap(LineCapRound);chevron.SetEndCap(LineCapRound);
        g.DrawLine(&chevron,w-19,h/2-1.5f,w-15,h/2+2.5f);g.DrawLine(&chevron,w-15,h/2+2.5f,w-11,h/2-1.5f);g.Flush();
        wchar_t label[100]{};LRESULT selected=SendMessage(control,CB_GETCURSEL,0,0);
        if(selected!=CB_ERR)SendMessage(control,CB_GETLBTEXT,selected,reinterpret_cast<LPARAM>(label));
        auto old=SelectObject(dc,self->font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,self->colors.text);
        r.left+=self->Px(11);r.right-=self->Px(30);DrawText(dc,label,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(dc,old);
        if(message==WM_PAINT)EndPaint(control,&paint);return 0;
    }
    if(message==WM_ERASEBKGND)return 1;
    LRESULT result=DefSubclassProc(control,message,wp,lp);
    if(message==CB_SETCURSEL||message==CB_SHOWDROPDOWN||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_KEYUP||message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_UPDATEUISTATE)InvalidateRect(control,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(control,ComboProcedure,id);
    return result;
}
LRESULT CALLBACK SettingsWindow::MenuProcedure(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto*self=reinterpret_cast<SettingsWindow*>(data);
    LRESULT result=DefSubclassProc(control,message,wp,lp);
    if(message==WM_WINDOWPOSCHANGED){
        RECT r{};GetWindowRect(control,&r);int w=r.right-r.left,h=r.bottom-r.top;
        ULONG_PTR geometry=ULONG_PTR(MAKELONG(w,h))|(ULONG_PTR(GetDpiForWindow(control))<<32);
        if(w>0&&h>0&&ULONG_PTR(GetProp(control,L"WindowScatter.MenuGeometry.v1"))!=geometry){
            SetProp(control,L"WindowScatter.MenuGeometry.v1",reinterpret_cast<HANDLE>(geometry));
            HRGN region=CreateRoundRectRgn(0,0,w+1,h+1,self->Px(16),self->Px(16));
            if(!SetWindowRgn(control,region,TRUE))DeleteObject(region);
        }
    }
    if(message==WM_PAINT||message==WM_NCPAINT){
        RECT r{};GetWindowRect(control,&r);HDC dc=GetWindowDC(control);
        if(dc){Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(self->scale,self->scale);
            auto path=Rounded(.5f,.5f,(r.right-r.left)/self->scale-1,(r.bottom-r.top)/self->scale-1,8);
            Pen pen(C(self->colors.line));g.DrawPath(&pen,path.get());g.Flush();ReleaseDC(control,dc);}
    }
    if(message==WM_NCDESTROY){RemoveProp(control,L"WindowScatter.MenuGeometry.v1");RemoveWindowSubclass(control,MenuProcedure,id);}
    return result;
}
LRESULT CALLBACK SettingsWindow::ToggleProcedure(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto*self=reinterpret_cast<SettingsWindow*>(data);
    if(message==WM_PAINT||message==WM_PRINTCLIENT||message==WM_PRINT){
        PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(control,&paint):reinterpret_cast<HDC>(wp);RECT r;GetClientRect(control,&r);
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(self->scale,self->scale);SolidBrush base(C(self->colors.background));g.FillRectangle(&base,0.f,0.f,r.right/self->scale,r.bottom/self->scale);
        bool checked=SendMessage(control,BM_GETCHECK,0,0)==BST_CHECKED;
        COLORREF track=checked?self->colors.accent:(self->highContrast?GetSysColor(COLOR_WINDOWTEXT):(self->dark?RGB(99,101,111):RGB(202,203,210)));
        FillRounded(g,track,1,2,42,23,11.5f);
        SolidBrush knob(C(self->highContrast?GetSysColor(checked?COLOR_HIGHLIGHTTEXT:COLOR_WINDOW):RGB(255,255,255)));g.FillEllipse(&knob,checked?22.f:3.f,4.f,19.f,19.f);g.Flush();
        if(GetFocus()==control&&!(SendMessage(control,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){InflateRect(&r,-1,-1);DrawFocusRect(dc,&r);}
        if(message==WM_PAINT)EndPaint(control,&paint);return 0;
    }
    if(message==WM_ERASEBKGND)return 1;
    LRESULT result=DefSubclassProc(control,message,wp,lp);
    if(message==BM_SETCHECK||message==WM_LBUTTONUP||message==WM_KEYUP||message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_UPDATEUISTATE)InvalidateRect(control,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(control,ToggleProcedure,id);return result;
}
void SettingsWindow::ShowLicenses(){
    if(license&&IsWindow(license)){SetForegroundWindow(license);return;}
    std::vector<BYTE>bytes;if(!ReadResource(instance,202,bytes))return;
    int count=MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<const char*>(bytes.data()),int(bytes.size()),nullptr,0);
    std::wstring text(count,L'\0');MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<const char*>(bytes.data()),int(bytes.size()),text.data(),count);
    WNDCLASSEX wc{sizeof(wc)};wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"WindowScatter.Licenses.v1";wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wc.lpfnWndProc=[](HWND w,UINT m,WPARAM p,LPARAM l)->LRESULT{if(m==WM_SIZE){if(HWND edit=GetDlgItem(w,1))MoveWindow(edit,12,12,LOWORD(l)-24,HIWORD(l)-24,TRUE);return 0;}return DefWindowProc(w,m,p,l);};RegisterClassEx(&wc);
    RECT r;GetWindowRect(hwnd,&r);license=CreateWindowEx(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"开源许可",WS_OVERLAPPEDWINDOW,r.left+Px(30),r.top+Px(50),Px(670),Px(440),hwnd,nullptr,instance,nullptr);
    HWND edit=CreateWindowEx(0,WC_EDIT,text.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,12,12,Px(620),Px(360),license,reinterpret_cast<HMENU>(1),instance,nullptr);
    SendMessage(edit,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);SendMessage(license,WM_SIZE,0,MAKELPARAM(Px(654),Px(400)));ShowWindow(license,SW_SHOW);SetForegroundWindow(license);
}
LRESULT SettingsWindow::Message(UINT message,WPARAM wp,LPARAM lp){
    if(taskbarMessage&&message==taskbarMessage){if(callbacks.reconnect)callbacks.reconnect();return 0;}
    switch(message){
    case kOpenSettings:Show();return 1;
    case kExitApplication:if(callbacks.exit)callbacks.exit();return 1;
    case kStatusChanged:if(callbacks.bridgeStatus)callbacks.bridgeStatus(wp,lp);return 0;
    case kRefresh:Layout();return 0;
    case WM_CLOSE:ShowWindow(hwnd,SW_HIDE);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);Paint(dc);EndPaint(hwnd,&paint);return 0;}
    case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);Layout();return 0;}
    case WM_SIZE:if(wp!=SIZE_MINIMIZED&&startup)Layout();return 0;
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:Theme();return 0;
    case WM_DRAWITEM:{const auto&item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item.CtlType==ODT_COMBOBOX)DrawCombo(item);else DrawButton(item);return TRUE;}
    case WM_MEASUREITEM:{auto*item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);item->itemHeight=Px(28);return TRUE;}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:case WM_CTLCOLORLISTBOX:{HDC dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,colors.text);SetBkColor(dc,colors.background);return reinterpret_cast<LRESULT>(backgroundBrush);}
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(lp)==duration)preferences.duration=int(SendMessage(duration,TBM_GETPOS,0,0));
        if(reinterpret_cast<HWND>(lp)==radius)preferences.radius=int(SendMessage(radius,TBM_GETPOS,0,0));
        Save();return 0;
    case WM_COMMAND:{
        int id=LOWORD(wp),notification=HIWORD(wp);
        if(id==Startup&&notification==BN_CLICKED){bool checked=SendMessage(startup,BM_GETCHECK,0,0)==BST_CHECKED;if(!SetStartup(checked)){SendMessage(startup,BM_SETCHECK,!checked?BST_CHECKED:BST_UNCHECKED,0);MessageBox(hwnd,L"无法更改开机启动设置。",L"Window Scatter",MB_ICONERROR|MB_OK);}return 0;}
        if(id==Enabled&&notification==BN_CLICKED){preferences.enabled=SendMessage(enabled,BM_GETCHECK,0,0)==BST_CHECKED;Save();return 0;}
        if(id==Memory&&notification==CBN_SELCHANGE){preferences.memory=int(SendMessage(memory,CB_GETCURSEL,0,0));Save();return 0;}
        if(id==Exit&&notification==BN_CLICKED){if(callbacks.exit)callbacks.exit();return 0;}
        if(id==Retry&&notification==BN_CLICKED){if(callbacks.reconnect)callbacks.reconnect();return 0;}
        if(id==GitHub&&notification==BN_CLICKED){ShellExecute(hwnd,L"open",L"https://github.com/SinCircle/windhawk-window-scatter",nullptr,nullptr,SW_SHOWNORMAL);return 0;}
        if(id==Notices&&notification==BN_CLICKED){ShowLicenses();return 0;}
        if(id==IDCANCEL){ShowWindow(hwnd,SW_HIDE);return 0;}break;
    }
    }
    return DefWindowProc(hwnd,message,wp,lp);
}
LRESULT CALLBACK SettingsWindow::Procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    auto*self=reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtr(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    return self?self->Message(message,wp,lp):DefWindowProc(hwnd,message,wp,lp);
}
}
