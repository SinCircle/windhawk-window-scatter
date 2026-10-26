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
enum {Startup=1010,Enabled,Memory,Duration,Exit=1015,Retry,GitHub,Notices,
      About=1020,Connection,HelpEnabled=1030,HelpStartup,HelpMemory,HelpDuration,PopupClose};
constexpr UINT kRefresh=WM_APP+104;
Color C(COLORREF c,BYTE alpha=255){return Color(alpha,GetRValue(c),GetGValue(c),GetBValue(c));}
std::unique_ptr<GraphicsPath> Rounded(float x,float y,float width,float height,float radius){
    auto path=std::make_unique<GraphicsPath>();float diameter=std::min({radius*2,width,height});
    path->AddArc(x,y,diameter,diameter,180,90);path->AddArc(x+width-diameter,y,diameter,diameter,270,90);
    path->AddArc(x+width-diameter,y+height-diameter,diameter,diameter,0,90);path->AddArc(x,y+height-diameter,diameter,diameter,90,90);path->CloseFigure();return path;
}
void FillRounded(Graphics&g,COLORREF color,float x,float y,float w,float h,float radius){auto path=Rounded(x,y,w,h,radius);SolidBrush brush(C(color));g.FillPath(&brush,path.get());}
void Text(Graphics&g,const wchar_t*text,COLORREF color,float size,RectF rect,bool bold=false,StringAlignment align=StringAlignmentNear,StringAlignment vertical=StringAlignmentCenter){
    Font font(L"Microsoft YaHei UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(C(color));StringFormat format;
    format.SetAlignment(align);format.SetLineAlignment(vertical);format.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(text,-1,&font,rect,&format,&brush);
}
void AppMark(Graphics&g,float x,float y,float size,COLORREF accent){
    FillRounded(g,accent,x,y,size,size,size*.25f);
    float unit=size/118.f;
    Pen pen(Color(245,255,255,255),4*unit);SolidBrush glass(Color(40,255,255,255));
    auto back=Rounded(x+23*unit,y+29*unit,52*unit,40*unit,7*unit);g.FillPath(&glass,back.get());g.DrawPath(&pen,back.get());
    auto front=Rounded(x+45*unit,y+51*unit,52*unit,40*unit,7*unit);SolidBrush fill(Color(255,255,255,255));g.FillPath(&fill,front.get());
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
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hIcon=reinterpret_cast<HICON>(LoadImage(instance,MAKEINTRESOURCE(101),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));
    wc.hIconSm=reinterpret_cast<HICON>(LoadImage(instance,MAKEINTRESOURCE(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
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
    enabled=Button(L"启用总览",Enabled,BS_AUTOCHECKBOX);
    help[0]=Button(L"启用总览说明",HelpEnabled);
    startup=Button(L"开机启动",Startup,BS_AUTOCHECKBOX);
    help[1]=Button(L"开机启动说明",HelpStartup);
    memory=CreateWindowEx(0,WC_COMBOBOX,L"内存模式",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,
                          0,0,100,200,hwnd,reinterpret_cast<HMENU>(Memory),instance,nullptr);
    for(auto name:{L"自动",L"常驻",L"节省"})SendMessage(memory,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    help[2]=Button(L"内存模式说明",HelpMemory);
    duration=CreateWindowEx(0,TRACKBAR_CLASS,L"动画时长（毫秒）",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,
                            0,0,100,30,hwnd,reinterpret_cast<HMENU>(Duration),instance,nullptr);
    SendMessage(duration,TBM_SETRANGE,TRUE,MAKELPARAM(0,900));SendMessage(duration,TBM_SETPAGESIZE,0,50);SendMessage(duration,TBM_SETLINESIZE,0,10);
    help[3]=Button(L"动画时长说明",HelpDuration);
    connection=Button(L"查看连接状态",Connection);about=Button(L"关于",About);exitButton=Button(L"退出程序",Exit);
    SetWindowSubclass(startup,ToggleProcedure,1,reinterpret_cast<DWORD_PTR>(this));SetWindowSubclass(enabled,ToggleProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(memory,ComboProcedure,1,reinterpret_cast<DWORD_PTR>(this));SetWindowSubclass(duration,SliderProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    for(HWND control:{memory,duration})SetWindowSubclass(control,ActionProcedure,2,reinterpret_cast<DWORD_PTR>(this));
    COMBOBOXINFO combo{sizeof(combo)};
    if(GetComboBoxInfo(memory,&combo)&&combo.hwndList)SetWindowSubclass(combo.hwndList,MenuProcedure,1,reinterpret_cast<DWORD_PTR>(this));
    AccessibleName(memory,L"内存模式");AccessibleName(duration,L"动画时长，单位毫秒");
    loading=true;SendMessage(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    SendMessage(enabled,BM_SETCHECK,preferences.enabled?BST_CHECKED:BST_UNCHECKED,0);SendMessage(memory,CB_SETCURSEL,preferences.memory,0);
    SendMessage(duration,TBM_SETPOS,TRUE,preferences.duration);loading=false;
    SendMessage(hwnd,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
    Theme();Layout();return true;
}
HWND SettingsWindow::Button(const wchar_t*label,int id,DWORD style,HWND parent){
    HWND control=CreateWindowEx(0,WC_BUTTON,label,WS_CHILD|WS_TABSTOP|style,0,0,10,10,parent?parent:hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);
    if(control)SetWindowSubclass(control,ActionProcedure,2,reinterpret_cast<DWORD_PTR>(this));
    return control;
}
void SettingsWindow::Theme(){
    DWORD light=1,size=sizeof(light);RegGetValue(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);dark=light==0;
    HIGHCONTRAST contrast{sizeof(contrast)};SystemParametersInfo(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);highContrast=contrast.dwFlags&HCF_HIGHCONTRASTON;
    if(highContrast)colors={GetSysColor(COLOR_WINDOW),GetSysColor(COLOR_WINDOW),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_WINDOWTEXT)};
    else if(dark)colors={RGB(29,29,31),RGB(42,42,45),RGB(245,245,247),RGB(164,164,171),RGB(62,62,66),RGB(80,155,255),RGB(50,66,87),RGB(255,117,107)};
    else colors={RGB(245,245,247),RGB(255,255,255),RGB(29,29,31),RGB(112,112,120),RGB(230,230,234),RGB(0,113,227),RGB(230,240,252),RGB(196,58,51)};
    if(backgroundBrush)DeleteObject(backgroundBrush);if(cardBrush)DeleteObject(cardBrush);
    backgroundBrush=CreateSolidBrush(colors.background);cardBrush=CreateSolidBrush(colors.card);
    BOOL immersive=dark&&!highContrast;DwmSetWindowAttribute(hwnd,20,&immersive,sizeof(immersive));
    if(popup)DwmSetWindowAttribute(popup,20,&immersive,sizeof(immersive));
    for(HWND control:{memory,duration})if(control)SetWindowTheme(control,dark?L"DarkMode_Explorer":L"Explorer",nullptr);
    if(hwnd)RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    if(popup)RedrawWindow(popup,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
void SettingsWindow::Layout(){
    if(!hwnd||arranging)return;arranging=true;
    scale=GetDpiForWindow(hwnd)/96.f;RECT client{};GetClientRect(hwnd,&client);width=int(std::lround(client.right/scale));height=int(std::lround(client.bottom/scale));
    HFONT old=font;font=CreateFont(-Px(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    auto place=[&](HWND control,float x,float y,float w,float h){
        if(!control)return;SetWindowPos(control,nullptr,Px(x),Px(y),Px(w),Px(h),SWP_NOZORDER|SWP_NOACTIVATE);
        SendMessage(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);ShowWindow(control,SW_SHOWNA);
    };
    float right=width-44;
    place(enabled,right-44,99,44,26);place(startup,right-44,155,44,26);
    place(memory,right-156,226,156,180);SendMessage(memory,CB_SETITEMHEIGHT,WPARAM(-1),Px(28));SendMessage(memory,CB_SETITEMHEIGHT,0,Px(32));
    place(duration,right-250,291,184,30);
    const float centers[]={112,168,242,306};for(size_t i=0;i<help.size();++i)place(help[i],102,centers[i]-13,20,26);
    place(connection,width-136,28,112,28);place(about,28,height-54,60,30);place(exitButton,width-116,height-54,92,30);
    if(license&&IsWindow(license))SendMessage(GetDlgItem(license,1),WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    if(popup)UpdateHelp();if(old)DeleteObject(old);InvalidateRect(hwnd,nullptr,FALSE);arranging=false;
}
void SettingsWindow::Show(){
    SendMessage(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    // Explicitly opening settings must override STARTUPINFO's SW_HIDE (for
    // example after a hidden logon launch). The first ShowWindow call can ignore
    // its requested show mode, whereas SWP_SHOWWINDOW does not.
    if(IsIconic(hwnd))ShowWindow(hwnd,SW_RESTORE);
    SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);SetFocus(enabled);RefreshHelpHover();
}
void SettingsWindow::SetHoveredHelpRow(int row){
    if(hoveredHelpRow==row)return;
    int previous=hoveredHelpRow;hoveredHelpRow=row;
    for(int index:{previous,row})if(index>=0&&index<int(help.size())&&help[index])InvalidateRect(help[index],nullptr,FALSE);
}
void SettingsWindow::TrackHelpHover(HWND control,LPARAM position){
    if(control!=hwnd&&!IsChild(hwnd,control)){SetHoveredHelpRow(-1);return;}
    POINT point{static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};
    if(control!=hwnd)MapWindowPoints(control,hwnd,&point,1);
    float x=point.x/scale,y=point.y/scale;int row=-1;
    if(x>=24&&x<width-24){
        if(y>=84&&y<196)row=y<140?0:1;
        else if(y>=210&&y<338)row=y<274?2:3;
    }
    SetHoveredHelpRow(row);
}
void SettingsWindow::RefreshHelpHover(){
    POINT point{};GetCursorPos(&point);HWND under=WindowFromPoint(point);
    if(IsVisible()&&(under==hwnd||IsChild(hwnd,under))){ScreenToClient(hwnd,&point);TrackHelpHover(hwnd,MAKELPARAM(point.x,point.y));}
    else SetHoveredHelpRow(-1);
}
bool SettingsWindow::ProcessDialogMessage(MSG&message){
    if(message.message==WM_KEYDOWN&&(message.wParam==VK_TAB||message.wParam==VK_LEFT||message.wParam==VK_RIGHT||message.wParam==VK_UP||message.wParam==VK_DOWN||message.wParam==VK_SPACE))
        SendMessage(popup&&IsWindowVisible(popup)?popup:hwnd,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
    if(popup&&IsWindowVisible(popup)&&IsDialogMessage(popup,&message))return true;
    return IsVisible()&&IsDialogMessage(hwnd,&message);
}
void SettingsWindow::Destroy(){
    CloseHelp(false);if(popup)DestroyWindow(popup);popup=nullptr;
    if(license)DestroyWindow(license);license=nullptr;if(hwnd)DestroyWindow(hwnd);hwnd=nullptr;
    if(font)DeleteObject(font);font=nullptr;if(backgroundBrush)DeleteObject(backgroundBrush);backgroundBrush=nullptr;
    if(cardBrush)DeleteObject(cardBrush);cardBrush=nullptr;
}
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
    {
        Graphics g(buffer);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
        SolidBrush base(C(colors.background));g.FillRectangle(&base,0.f,0.f,float(width),float(height));
        AppMark(g,26,25,32,colors.accent);Text(g,L"Window Scatter",colors.text,20,{72,23,264,36},true);
        FillRounded(g,colors.card,24,84,float(width-48),112,14);FillRounded(g,colors.card,24,210,float(width-48),128,14);
        Pen divider(C(colors.line));g.DrawLine(&divider,44.f,140.f,float(width-44),140.f);g.DrawLine(&divider,44.f,274.f,float(width-44),274.f);
        if(highContrast){for(auto bounds:{RectF(24,84,float(width-48),112),RectF(24,210,float(width-48),128)}){auto path=Rounded(bounds.X,bounds.Y,bounds.Width,bounds.Height,14);g.DrawPath(&divider,path.get());}}
        Text(g,L"启用总览",colors.text,14,{44,96,84,32});Text(g,L"开机启动",colors.text,14,{44,152,84,32});
        Text(g,L"内存模式",colors.text,14,{44,226,84,32});Text(g,L"动画时长",colors.text,14,{44,290,84,32});
        Text(g,(std::to_wstring(preferences.duration)+L" ms").c_str(),colors.secondary,13,{float(width-110),290,66,32},false,StringAlignmentFar);
    }
    BitBlt(dc,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
}
void SettingsWindow::DrawButton(const DRAWITEMSTRUCT&item){
    int id=GetDlgCtrlID(item.hwndItem);RECT bounds=item.rcItem;float w=(bounds.right-bounds.left)/scale,h=(bounds.bottom-bounds.top)/scale;
    bool question=id>=HelpEnabled&&id<=HelpDuration,inPopup=GetParent(item.hwndItem)==popup;
    bool hovered=GetProp(item.hwndItem,L"WindowScatter.ButtonHover.v1")!=nullptr,pressed=item.itemState&ODS_SELECTED;
    FillRect(item.hDC,&bounds,question||inPopup?cardBrush:backgroundBrush);
    Graphics g(item.hDC);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
    SolidBrush base(C(question||inPopup?colors.card:colors.background));g.FillRectangle(&base,0.f,0.f,w,h);
    if(!question&&(hovered||pressed))FillRounded(g,colors.selection,1,1,w-2,h-2,7);
    bool keyboardFocus=(item.itemState&ODS_FOCUS)&&!(SendMessage(item.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS);
    COLORREF foreground=highContrast&&!question&&(hovered||pressed)?GetSysColor(COLOR_HIGHLIGHTTEXT):colors.secondary;
    if(question){
        if(hoveredHelpRow==id-HelpEnabled||keyboardFocus)
            Text(g,L"?",hovered?colors.accent:foreground,14,{0,0,w,h},false,StringAlignmentCenter);
    }else if(id==PopupClose){
        Pen line(C(foreground),1.4f);line.SetStartCap(LineCapRound);line.SetEndCap(LineCapRound);
        g.DrawLine(&line,w/2-3.5f,h/2-3.5f,w/2+3.5f,h/2+3.5f);g.DrawLine(&line,w/2+3.5f,h/2-3.5f,w/2-3.5f,h/2+3.5f);
    }else if(id==Connection){
        int current;{std::lock_guard lock(statusMutex);current=status;}
        COLORREF dot=highContrast?foreground:current==1?RGB(44,163,103):current==3?RGB(212,133,39):colors.accent;
        SolidBrush indicator(C(dot));g.FillEllipse(&indicator,10.f,h/2-3,6.f,6.f);
        Text(g,current==1?L"已连接":current==3?L"未连接":L"连接中",foreground,12,{24,0,56,h});
        if(hovered||keyboardFocus)Text(g,L"?",foreground,12,{66,0,14,h},false,StringAlignmentCenter);
    }else{
        wchar_t label[100]{};GetWindowText(item.hwndItem,label,ARRAYSIZE(label));
        Text(g,label,inPopup&&!highContrast?colors.accent:foreground,13,{8,0,w-16,h},false,StringAlignmentCenter);
    }
    g.Flush();if((item.itemState&ODS_FOCUS)&&!(SendMessage(item.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){RECT focus=bounds;InflateRect(&focus,-3,-3);DrawFocusRect(item.hDC,&focus);}
}
LRESULT CALLBACK SettingsWindow::ActionProcedure(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto*self=reinterpret_cast<SettingsWindow*>(data);
    if(message==WM_MOUSEMOVE)self->TrackHelpHover(control,lp);
    if(message==WM_MOUSEMOVE&&!GetProp(control,L"WindowScatter.ButtonHover.v1")){
        SetProp(control,L"WindowScatter.ButtonHover.v1",reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0};TrackMouseEvent(&track);InvalidateRect(control,nullptr,FALSE);
    }
    if(message==WM_MOUSELEAVE){RemoveProp(control,L"WindowScatter.ButtonHover.v1");InvalidateRect(control,nullptr,FALSE);self->RefreshHelpHover();}
    if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_UPDATEUISTATE)InvalidateRect(control,nullptr,FALSE);
    if(message==WM_NCDESTROY){RemoveProp(control,L"WindowScatter.ButtonHover.v1");RemoveWindowSubclass(control,ActionProcedure,id);}
    return DefSubclassProc(control,message,wp,lp);
}
void SettingsWindow::DrawCombo(const DRAWITEMSTRUCT&item){
    bool selected=(item.itemState&ODS_SELECTED)&&!(item.itemState&ODS_COMBOBOXEDIT);COLORREF bg=selected?colors.selection:colors.card;
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
        FillRect(dc,&r,self->cardBrush);
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(self->scale,self->scale);
        SolidBrush base(C(self->colors.card));g.FillRectangle(&base,0.f,0.f,w,h);
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
        FillRect(dc,&r,self->cardBrush);
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(self->scale,self->scale);SolidBrush base(C(self->colors.card));g.FillRectangle(&base,0.f,0.f,r.right/self->scale,r.bottom/self->scale);
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
LRESULT CALLBACK SettingsWindow::SliderProcedure(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto*self=reinterpret_cast<SettingsWindow*>(data);
    if(message==WM_PAINT||message==WM_PRINTCLIENT||message==WM_PRINT){
        PAINTSTRUCT paint{};HDC dc=message==WM_PAINT?BeginPaint(control,&paint):reinterpret_cast<HDC>(wp);
        RECT bounds{},channel{},thumb{};GetClientRect(control,&bounds);
        FillRect(dc,&bounds,self->cardBrush);
        SendMessage(control,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));SendMessage(control,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(self->scale,self->scale);
        SolidBrush base(C(self->colors.card));g.FillRectangle(&base,0.f,0.f,bounds.right/self->scale,bounds.bottom/self->scale);
        float left=channel.left/self->scale,right=channel.right/self->scale,x=(thumb.left+thumb.right)/(2*self->scale),y=(thumb.top+thumb.bottom)/(2*self->scale);
        if(right>left){
            FillRounded(g,self->highContrast?self->colors.secondary:self->colors.line,left,y-1.5f,right-left,3,1.5f);
            if(x>left)FillRounded(g,self->colors.accent,left,y-1.5f,std::min(x,right)-left,3,1.5f);
        }
        SolidBrush shadow(C(self->colors.text,25));g.FillEllipse(&shadow,x-8.5f,y-7.5f,17.f,17.f);
        SolidBrush knob(C(self->highContrast?self->colors.text:RGB(255,255,255)));g.FillEllipse(&knob,x-8,y-8,16.f,16.f);
        Pen edge(C(self->highContrast?self->colors.card:self->colors.line),.8f);g.DrawEllipse(&edge,x-8,y-8,16.f,16.f);g.Flush();
        if(GetFocus()==control&&!(SendMessage(control,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){InflateRect(&bounds,-1,-1);DrawFocusRect(dc,&bounds);}
        if(message==WM_PAINT)EndPaint(control,&paint);return 0;
    }
    if(message==WM_ERASEBKGND)return 1;
    LRESULT result=DefSubclassProc(control,message,wp,lp);
    if(message==TBM_SETPOS||message==WM_MOUSEMOVE||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_KEYDOWN||message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_UPDATEUISTATE)InvalidateRect(control,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(control,SliderProcedure,id);
    return result;
}
bool SettingsWindow::ConfirmStartupLocation(){
    auto location=CurrentStartupLocation();if(location==StartupLocation::Permanent)return true;
    std::wstring message=location==StartupLocation::Downloads
        ?L"程序还在下载目录，清理下载文件后，开机启动可能失效。"
        :L"程序还在临时目录，系统清理后，开机启动可能失效。";
    message+=L"\n\n建议先移到固定文件夹，再开启自启。";
    std::wstring path=ExecutablePath();
    const TASKDIALOG_BUTTON buttons[]={{IDOK,L"仍然开启"},{IDCANCEL,L"取消"}};
    TASKDIALOGCONFIG config{sizeof(config)};config.hwndParent=hwnd;
    config.dwFlags=TDF_POSITION_RELATIVE_TO_WINDOW|TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT;
    config.pszWindowTitle=L"Window Scatter";config.pszMainIcon=TD_WARNING_ICON;
    config.pszMainInstruction=L"先把程序移到固定位置？";config.pszContent=message.c_str();
    config.pszExpandedInformation=path.c_str();config.pszExpandedControlText=L"收起位置";config.pszCollapsedControlText=L"查看位置";
    config.pButtons=buttons;config.cButtons=ARRAYSIZE(buttons);config.nDefaultButton=IDCANCEL;
    int selected=IDCANCEL;
    if(FAILED(TaskDialogIndirect(&config,&selected,nullptr,nullptr)))
        selected=MessageBox(hwnd,(message+L"\n\n仍要开启吗？").c_str(),L"Window Scatter",MB_ICONWARNING|MB_OKCANCEL|MB_DEFBUTTON2);
    return selected==IDOK;
}
void SettingsWindow::RefreshStatus(){
    int current;{std::lock_guard lock(statusMutex);current=status;}
    SetWindowText(connection,current==1?L"已连接，查看详情":current==3?L"未连接，查看详情":L"连接中，查看详情");
    InvalidateRect(connection,nullptr,FALSE);
    if(popupTopic==Connection)UpdateHelp();
}
void SettingsWindow::ShowHelp(int topic,HWND anchor){
    if(popup&&IsWindowVisible(popup)&&popupTopic==topic){CloseHelp();return;}
    if(!popup){
        WNDCLASSEX wc{sizeof(wc)};wc.hInstance=instance;wc.lpfnWndProc=PopupProcedure;wc.lpszClassName=L"WindowScatter.Help.v1";
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.style=CS_DROPSHADOW;RegisterClassEx(&wc);
        popup=CreateWindowEx(WS_EX_TOOLWINDOW,wc.lpszClassName,L"",WS_POPUP|WS_CLIPCHILDREN,0,0,1,1,hwnd,nullptr,instance,this);
        if(!popup)return;
        DWORD corner=2;DwmSetWindowAttribute(popup,33,&corner,sizeof(corner));
        BOOL immersive=dark&&!highContrast;DwmSetWindowAttribute(popup,20,&immersive,sizeof(immersive));
        popupClose=Button(L"关闭说明",PopupClose,BS_OWNERDRAW,popup);
        popupBody=CreateWindowEx(0,WC_STATIC,L"",WS_CHILD|SS_LEFT|SS_NOPREFIX,0,0,1,1,popup,reinterpret_cast<HMENU>(1),instance,nullptr);
        retry=Button(L"重新连接",Retry,BS_OWNERDRAW,popup);
        github=Button(L"GitHub",GitHub,BS_OWNERDRAW,popup);notices=Button(L"开源许可",Notices,BS_OWNERDRAW,popup);
    }
    popupTopic=topic;popupAnchor=anchor;UpdateHelp();
    SendMessage(popup,WM_CHANGEUISTATE,MAKEWPARAM(SendMessage(hwnd,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS?UIS_SET:UIS_CLEAR,UISF_HIDEFOCUS),0);
    RECT target{},parent{};GetWindowRect(anchor,&target);GetWindowRect(hwnd,&parent);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    int w=Px(332),h=Px(popupHeight),x=std::min(target.left,parent.right-Px(24)-w),y=target.bottom+Px(8);
    x=std::clamp(x,int(monitor.rcWork.left+Px(8)),int(std::max(monitor.rcWork.left+Px(8),monitor.rcWork.right-w-Px(8))));
    if(y+h>monitor.rcWork.bottom-Px(8))y=target.top-h-Px(8);
    y=std::max(y,int(monitor.rcWork.top+Px(8)));
    SetWindowPos(popup,HWND_TOP,x,y,w,h,SWP_SHOWWINDOW);SetForegroundWindow(popup);SetFocus(popupClose);
}
void SettingsWindow::CloseHelp(bool restoreFocus){
    if(!popup||!popupTopic)return;
    popupTopic=0;ShowWindow(popup,SW_HIDE);
    if(restoreFocus&&popupAnchor&&IsWindowVisible(hwnd))SetFocus(popupAnchor);
}
void SettingsWindow::UpdateHelp(){
    if(!popup||!popupTopic)return;
    int current;std::wstring label,detail;
    {std::lock_guard lock(statusMutex);current=status;label=statusText;detail=statusDetail;}
    switch(popupTopic){
    case HelpEnabled:popupTitle=L"启用总览";popupText=L"用 Win+Tab 或三指上滑打开总览，点击预览切换窗口。\n\n关闭此选项后恢复系统任务视图。Shift+Win+Tab 始终打开系统任务视图。";break;
    case HelpStartup:popupTitle=L"开机启动";popupText=L"登录 Windows 后在后台运行，不弹出设置。\n\n请把程序保存在固定位置。移动程序后，在新位置重新开启此选项。";break;
    case HelpMemory:popupTitle=L"内存模式";popupText=L"自动：内存紧张时释放图形资源。\n\n常驻：保留资源，优先打开速度。\n\n节省：关闭总览后释放图形资源。";break;
    case HelpDuration:popupTitle=L"动画时长";popupText=L"调整窗口展开、重排和收回的时长，单位为毫秒。\n\n设为 0 可关闭动画。修改会即时生效。";break;
    case About:popupTitle=L"Window Scatter";popupText=std::wstring(L"版本 ")+kVersion+L"\n\n设置即时保存。关闭面板后继续在后台运行，再次打开程序可返回这里。";break;
    case Connection:popupTitle=label;popupText=detail.empty()?L"连接状态更新后会显示在这里。":detail;break;
    default:return;
    }
    SetWindowText(popup,popupTitle.c_str());SetWindowText(popupBody,popupText.c_str());
    HDC dc=GetDC(popup);auto previous=SelectObject(dc,font);RECT measured{0,0,Px(292),0};
    DrawText(dc,popupText.c_str(),-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(dc,previous);ReleaseDC(popup,dc);
    int bodyHeight=std::max(22,int(std::ceil(measured.bottom/scale))+4);
    bool actions=popupTopic==About||(popupTopic==Connection&&current==3);
    popupHeight=58+bodyHeight+20+(actions?42:0);
    auto place=[&](HWND control,float x,float y,float w,float h,bool visible){
        SetWindowPos(control,nullptr,Px(x),Px(y),Px(w),Px(h),SWP_NOZORDER|SWP_NOACTIVATE);
        SendMessage(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);ShowWindow(control,visible?SW_SHOWNA:SW_HIDE);
    };
    place(popupClose,292,14,26,26,true);place(popupBody,20,58,292,float(bodyHeight),true);
    place(retry,208,float(popupHeight-52),104,32,popupTopic==Connection&&current==3);
    place(github,12,float(popupHeight-52),84,32,popupTopic==About);place(notices,108,float(popupHeight-52),100,32,popupTopic==About);
    SetWindowPos(popup,nullptr,0,0,Px(332),Px(popupHeight),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    InvalidateRect(popup,nullptr,FALSE);
}
void SettingsWindow::PaintHelp(HDC dc){
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
    SolidBrush base(C(colors.card));g.FillRectangle(&base,0.f,0.f,332.f,float(popupHeight));
    auto outline=Rounded(.5f,.5f,331.f,float(popupHeight-1),12);Pen border(C(colors.line));g.DrawPath(&border,outline.get());
    Text(g,popupTitle.c_str(),colors.text,15,{20,16,264,26},true);
}
LRESULT CALLBACK SettingsWindow::PopupProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto*self=reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams);SetWindowLongPtr(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProc(window,message,wp,lp);
    switch(message){
    case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)self->CloseHelp(false);return 0;
    case WM_CLOSE:self->CloseHelp();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);self->PaintHelp(dc);EndPaint(window,&paint);return 0;}
    case WM_PRINTCLIENT:self->PaintHelp(reinterpret_cast<HDC>(wp));return 0;
    case WM_DRAWITEM:self->DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_CTLCOLORSTATIC:SetBkColor(reinterpret_cast<HDC>(wp),self->colors.card);SetTextColor(reinterpret_cast<HDC>(wp),self->colors.text);return reinterpret_cast<LRESULT>(self->cardBrush);
    case WM_COMMAND:{
        int id=LOWORD(wp);if(id==IDCANCEL||id==PopupClose){self->CloseHelp();return 0;}
        if(id==Retry||id==GitHub||id==Notices){self->CloseHelp();SendMessage(self->hwnd,WM_COMMAND,wp,lp);return 0;}
        break;
    }
    }
    return DefWindowProc(window,message,wp,lp);
}
void SettingsWindow::ShowLicenses(){
    if(license&&IsWindow(license)){ShowWindow(license,SW_SHOWNORMAL);SetForegroundWindow(license);return;}
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
    case kRefresh:RefreshStatus();return 0;
    case WM_CLOSE:CloseHelp(false);SetHoveredHelpRow(-1);ShowWindow(hwnd,SW_HIDE);return 0;
    case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)SetHoveredHelpRow(-1);break;
    case WM_MOUSEMOVE:{TrackHelpHover(hwnd,lp);TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,hwnd,0};TrackMouseEvent(&tracking);return 0;}
    case WM_MOUSELEAVE:RefreshHelpHover();return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);Paint(dc);EndPaint(hwnd,&paint);return 0;}
    case WM_PRINTCLIENT:Paint(reinterpret_cast<HDC>(wp));return 0;
    case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);Layout();return 0;}
    case WM_SIZE:if(wp!=SIZE_MINIMIZED&&startup)Layout();return 0;
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:Theme();return 0;
    case WM_DRAWITEM:{const auto&item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item.CtlType==ODT_COMBOBOX)DrawCombo(item);else DrawButton(item);return TRUE;}
    case WM_MEASUREITEM:{auto*item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);item->itemHeight=Px(28);return TRUE;}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:case WM_CTLCOLORLISTBOX:{HDC dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,colors.text);SetBkColor(dc,colors.card);return reinterpret_cast<LRESULT>(cardBrush);}
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(lp)==duration)preferences.duration=int(SendMessage(duration,TBM_GETPOS,0,0));
        Save();return 0;
    case WM_COMMAND:{
        int id=LOWORD(wp),notification=HIWORD(wp);
        if(id==Startup&&notification==BN_CLICKED){
            bool checked=SendMessage(startup,BM_GETCHECK,0,0)==BST_CHECKED;
            if(checked&&!ConfirmStartupLocation()){SendMessage(startup,BM_SETCHECK,BST_UNCHECKED,0);return 0;}
            if(!SetStartup(checked)){SendMessage(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);MessageBox(hwnd,L"无法更改开机启动设置。",L"Window Scatter",MB_ICONERROR|MB_OK);}
            return 0;
        }
        if(notification==BN_CLICKED&&((id>=HelpEnabled&&id<=HelpDuration)||id==About||id==Connection)){ShowHelp(id,reinterpret_cast<HWND>(lp));return 0;}
        if(id==Enabled&&notification==BN_CLICKED){preferences.enabled=SendMessage(enabled,BM_GETCHECK,0,0)==BST_CHECKED;Save();return 0;}
        if(id==Memory&&notification==CBN_SELCHANGE){preferences.memory=int(SendMessage(memory,CB_GETCURSEL,0,0));Save();return 0;}
        if(id==Exit&&notification==BN_CLICKED){if(callbacks.exit)callbacks.exit();return 0;}
        if(id==Retry&&notification==BN_CLICKED){if(callbacks.reconnect)callbacks.reconnect();return 0;}
        if(id==GitHub&&notification==BN_CLICKED){ShellExecute(hwnd,L"open",L"https://github.com/SinCircle/windhawk-window-scatter",nullptr,nullptr,SW_SHOWNORMAL);return 0;}
        if(id==Notices&&notification==BN_CLICKED){ShowLicenses();return 0;}
        if(id==IDCANCEL){CloseHelp(false);ShowWindow(hwnd,SW_HIDE);return 0;}break;
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
