#pragma once
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <gdiplus.h>
#include "gesture_settings.h"
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"uxtheme.lib")
#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"gdi32.lib")

namespace SettingsUi {
static constexpr wchar_t mainClass[]=L"LibinputScrollSettings",pickerClass[]=L"LibinputScrollAppPicker";
static constexpr int appsId=100,modeId=101,gainId=102,invertId=103,axisId=104,addId=105,removeId=106,pointerId=107;
static HFONT normalFont=nullptr,titleFont=nullptr,sectionFont=nullptr;
static HBRUSH background=nullptr;
static int scale=96;
static int px(int value) { return MulDiv(value,scale,96); }
static HWND control(HWND owner,const wchar_t* kind,const wchar_t* text,DWORD style,int id) {
    HWND value=CreateWindowExW(kind==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,kind,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,owner,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(value,WM_SETFONT,reinterpret_cast<WPARAM>(normalFont),TRUE); return value;
}
static std::wstring text(HWND control) { int count=GetWindowTextLengthW(control); std::wstring value(count+1,L'\0'); GetWindowTextW(control,value.data(),count+1); value.resize(count); return value; }
static void place(HWND owner,int id,int x,int y,int w,int h) { MoveWindow(GetDlgItem(owner,id),px(x),px(y),px(std::max(w,1)),px(std::max(h,1)),TRUE); }
static void fonts(UINT dpi) {
    scale=static_cast<int>(dpi);
    if(normalFont) DeleteObject(normalFont); if(titleFont) DeleteObject(titleFont); if(sectionFont) DeleteObject(sectionFont);
    normalFont=CreateFontW(-MulDiv(10,scale,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    titleFont=CreateFontW(-MulDiv(21,scale,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    sectionFont=CreateFontW(-MulDiv(12,scale,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
}
static void column(HWND list,int index,const wchar_t* title,int width) { LVCOLUMNW item{}; item.mask=LVCF_TEXT|LVCF_WIDTH; item.pszText=const_cast<wchar_t*>(title); item.cx=px(width); ListView_InsertColumn(list,index,&item); }
static void cell(HWND list,int row,int sub,const std::wstring& value) {
    LVITEMW item{}; item.iItem=row; item.iSubItem=sub; item.mask=LVIF_TEXT; item.pszText=const_cast<wchar_t*>(value.c_str());
    if(sub) ListView_SetItem(list,&item); else ListView_InsertItem(list,&item);
}
static bool capture(HWND window,const std::wstring& path) {
    using namespace Gdiplus;
    RECT rect{}; GetClientRect(window,&rect); HDC screen=GetDC(window),memory=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,rect.right,rect.bottom); HGDIOBJ previous=SelectObject(memory,bitmap);
    bool rendered=PrintWindow(window,memory,PW_CLIENTONLY)!=FALSE;
    CLSID encoder{}; UINT count=0,bytes=0; GetImageEncodersSize(&count,&bytes); std::vector<BYTE> storage(bytes);
    auto* encoders=reinterpret_cast<ImageCodecInfo*>(storage.data()); GetImageEncoders(count,bytes,encoders);
    for(UINT i=0;i<count;++i) if(wcscmp(encoders[i].MimeType,L"image/png")==0) encoder=encoders[i].Clsid;
    SelectObject(memory,previous); Bitmap image(bitmap,nullptr); bool saved=image.Save(path.c_str(),&encoder,nullptr)==Ok;
    DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window,screen); return rendered && saved;
}
struct Picker {
    HWND window=nullptr,list=nullptr; std::vector<std::wstring> images,shown; std::wstring selected;
    static BOOL CALLBACK enumerate(HWND window,LPARAM context) {
        if(!IsWindowVisible(window) || GetWindow(window,GW_OWNER)) return TRUE;
        auto& self=*reinterpret_cast<Picker*>(context); auto image=imageForWindow(window);
        DWORD pid=0; GetWindowThreadProcessId(window,&pid); if(pid==GetCurrentProcessId() || image.empty()) return TRUE;
        if(std::none_of(self.images.begin(),self.images.end(),[&](const auto& other) { return sameImage(other,image); })) self.images.push_back(image);
        return TRUE;
    }
    void filter() {
        auto needle=text(GetDlgItem(window,200)); std::transform(needle.begin(),needle.end(),needle.begin(),towlower);
        ListView_DeleteAllItems(list); shown.clear();
        for(const auto& image:images) {
            auto searchable=image; std::transform(searchable.begin(),searchable.end(),searchable.begin(),towlower);
            if(!needle.empty() && searchable.find(needle)==std::wstring::npos) continue;
            int row=static_cast<int>(shown.size()); shown.push_back(image); cell(list,row,0,executableBase(image)); cell(list,row,1,image);
        }
        if(!shown.empty()) ListView_SetItemState(list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    void choose() { int row=ListView_GetNextItem(list,-1,LVNI_SELECTED); if(row>=0 && row<static_cast<int>(shown.size())) { selected=shown[row]; DestroyWindow(window); } }
    void browse() {
        wchar_t file[32768]{}; OPENFILENAMEW request{}; request.lStructSize=sizeof(request); request.hwndOwner=window;
        request.lpstrTitle=L"Choose an application"; request.lpstrFilter=L"Applications (*.exe)\0*.exe\0\0"; request.lpstrFile=file; request.nMaxFile=32768;
        request.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_DONTADDTORECENT;
        if(GetOpenFileNameW(&request)) { selected=file; DestroyWindow(window); }
    }
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
        auto* self=reinterpret_cast<Picker*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE) { self=static_cast<Picker*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); self->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
        if(!self) return DefWindowProcW(window,message,wp,lp);
        if(message==WM_CREATE) {
            control(window,L"STATIC",L"Choose a running application or browse for its executable",0,210);
            control(window,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,200); SendMessageW(GetDlgItem(window,200),EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Search by name or path"));
            self->list=control(window,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER,201);
            ListView_SetExtendedListViewStyle(self->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER); SetWindowTheme(self->list,L"Explorer",nullptr);
            column(self->list,0,L"Application",180); column(self->list,1,L"File",470);
            control(window,L"BUTTON",L"Browse…",BS_PUSHBUTTON|WS_TABSTOP,202); control(window,L"BUTTON",L"Add",BS_DEFPUSHBUTTON|WS_TABSTOP,IDOK);
            control(window,L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,IDCANCEL);
            EnumWindows(enumerate,reinterpret_cast<LPARAM>(self)); std::sort(self->images.begin(),self->images.end()); self->filter(); return 0;
        }
        if(message==WM_SIZE) { int w=MulDiv(LOWORD(lp),96,scale),h=MulDiv(HIWORD(lp),96,scale); place(window,210,20,18,w-40,24); place(window,200,20,52,w-40,29); place(window,201,20,96,w-40,h-164); place(window,202,20,h-48,110,32); place(window,IDOK,w-236,h-48,100,32); place(window,IDCANCEL,w-120,h-48,100,32); return 0; }
        if(message==WM_COMMAND) { int id=LOWORD(wp); if(id==200 && HIWORD(wp)==EN_CHANGE) self->filter(); else if(id==IDOK) self->choose(); else if(id==202) self->browse(); else if(id==IDCANCEL) DestroyWindow(window); return 0; }
        if(message==WM_NOTIFY && reinterpret_cast<NMHDR*>(lp)->code==NM_DBLCLK) { self->choose(); return 0; }
        if(message==WM_CLOSE) { DestroyWindow(window); return 0; }
        if(message==WM_CTLCOLORSTATIC) { SetBkColor(reinterpret_cast<HDC>(wp),RGB(255,255,255)); return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH)); }
        return DefWindowProcW(window,message,wp,lp);
    }
    std::wstring show(HWND owner) {
        RECT bounds{}; GetWindowRect(owner,&bounds);
        HWND dialog=CreateWindowExW(WS_EX_DLGMODALFRAME,pickerClass,L"Add application",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,bounds.left+px(45),bounds.top+px(55),px(760),px(465),owner,nullptr,GetModuleHandleW(nullptr),this);
        EnableWindow(owner,FALSE); ShowWindow(dialog,SW_SHOW); SetFocus(GetDlgItem(dialog,200));
        MSG message{}; while(IsWindow(dialog) && GetMessageW(&message,nullptr,0,0)>0) if(!IsDialogMessageW(dialog,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        EnableWindow(owner,TRUE); SetForegroundWindow(owner); return selected;
    }
};
#include "pointer_settings_window.h"
struct Window {
    HWND window=nullptr,list=nullptr; GestureSettings data; std::wstring path,testImage;
    int selected=-1; bool loading=false,dirty=false,selfTest=false; int result=0;
    GestureProfile& profile(int row) { return row==0?data.defaults:data.applications[static_cast<size_t>(row-1)].profile; }
    void markDirty() { if(!loading) { dirty=true; SetWindowTextW(GetDlgItem(window,320),L"Unsaved changes"); EnableWindow(GetDlgItem(window,IDOK),TRUE); } }
    bool commit(bool report=true) {
        if(selected<0) return true;
        auto& value=profile(selected); auto mode=static_cast<GestureMode>(SendMessageW(GetDlgItem(window,modeId),CB_GETCURSEL,0,0));
        auto gainText=text(GetDlgItem(window,gainId)); std::replace(gainText.begin(),gainText.end(),L',',L'.'); wchar_t* end=nullptr; double gain=wcstod(gainText.c_str(),&end);
        if(mode==GestureMode::Touchscreen && (end==gainText.c_str() || *end || !std::isfinite(gain) || gain<1 || gain>40)) {
            if(report) MessageBoxW(window,L"Sensitivity must be a number from 1 to 40 pixels per millimeter.",L"Invalid value",MB_OK|MB_ICONINFORMATION);
            SetFocus(GetDlgItem(window,gainId)); return false;
        }
        value.mode=mode;
        if(std::isfinite(gain) && gain>=1 && gain<=40) value.touch.pixelsPerMm=gain;
        value.touch.invertHorizontal=SendMessageW(GetDlgItem(window,invertId),BM_GETCHECK,0,0)==BST_CHECKED;
        value.touch.lockVertical=SendMessageW(GetDlgItem(window,axisId),BM_GETCHECK,0,0)==BST_CHECKED;
        cell(list,selected,1,gestureModeLabel(value.mode)); return true;
    }
    void modeDescription() {
        auto mode=static_cast<GestureMode>(SendMessageW(GetDlgItem(window,modeId),CB_GETCURSEL,0,0)); bool touch=mode==GestureMode::Touchscreen;
        for(int id:{gainId,invertId,axisId,311,312}) EnableWindow(GetDlgItem(window,id),touch);
        const wchar_t* description=mode==GestureMode::WindowsV1?L"Passes horizontal gestures to Windows. Supports back and forward navigation in Firefox.":touch?L"A horizontal gesture becomes a screen contact until you lift your fingers. Useful for Telegram gestures. Enable horizontal axis lock to ignore vertical motion.":L"Uses libinput scrolling only. Horizontal scrolling and momentum remain enabled.";
        SetWindowTextW(GetDlgItem(window,310),description);
    }
    void load(int row) {
        selected=row; loading=true; const auto& value=profile(row);
        SetWindowTextW(GetDlgItem(window,304),row?executableBase(data.applications[row-1].image).c_str():L"All other applications");
        SetWindowTextW(GetDlgItem(window,305),row?data.applications[row-1].image.c_str():L"Applies when an application has no individual rule.");
        SendMessageW(GetDlgItem(window,modeId),CB_SETCURSEL,static_cast<WPARAM>(value.mode),0);
        wchar_t gain[40]; swprintf_s(gain,L"%.1f",value.touch.pixelsPerMm); SetWindowTextW(GetDlgItem(window,gainId),gain);
        SendMessageW(GetDlgItem(window,invertId),BM_SETCHECK,value.touch.invertHorizontal?BST_CHECKED:BST_UNCHECKED,0);
        SendMessageW(GetDlgItem(window,axisId),BM_SETCHECK,value.touch.lockVertical?BST_CHECKED:BST_UNCHECKED,0);
        EnableWindow(GetDlgItem(window,removeId),row>0); modeDescription(); loading=false;
    }
    void rows(int select) {
        loading=true; ListView_DeleteAllItems(list); cell(list,0,0,L"Default"); cell(list,0,1,gestureModeLabel(data.defaults.mode));
        for(size_t i=0;i<data.applications.size();++i) { cell(list,static_cast<int>(i+1),0,executableBase(data.applications[i].image)); cell(list,static_cast<int>(i+1),1,gestureModeLabel(data.applications[i].profile.mode)); }
        ListView_SetItemState(list,select,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED); loading=false; load(select);
    }
    void add(std::wstring image) {
        if(image.empty()) return;
        for(size_t i=0;i<data.applications.size();++i) if(sameImage(data.applications[i].image,image) ||
            (data.applications[i].image.find_first_of(L"\\/")==std::wstring::npos && sameImage(data.applications[i].image,executableBase(image)))) { rows(static_cast<int>(i+1)); return; }
        if(data.applications.size()>=128) { MessageBoxW(window,L"You can add up to 128 applications.",L"Applications",MB_OK); return; }
        data.applications.push_back({image,data.defaults}); rows(static_cast<int>(data.applications.size())); markDirty();
    }
    bool save(bool report=true) {
        if(!commit(report)) return false; DWORD failure=0;
        if(!saveGestureSettings(path,data,&failure)) { if(report) { auto error=L"Could not save settings. Error code: "+std::to_wstring(failure); MessageBoxW(window,error.c_str(),L"Settings",MB_OK|MB_ICONERROR); } return false; }
        dirty=false; SetWindowTextW(GetDlgItem(window,320),L"Saved. Pointer settings apply now; gesture rules apply between gestures."); EnableWindow(GetDlgItem(window,IDOK),FALSE); return true;
    }
    void layout() {
        RECT bounds{}; GetClientRect(window,&bounds); int w=MulDiv(bounds.right,96,scale),h=MulDiv(bounds.bottom,96,scale),right=w-394;
        place(window,300,24,18,w-280,42); place(window,pointerId,w-240,25,216,35); place(window,301,24,66,w-48,28); place(window,302,24,108,300,22);
        place(window,appsId,24,136,310,h-264); place(window,addId,24,h-112,170,33); place(window,removeId,204,h-112,130,33);
        place(window,304,370,105,right,30); place(window,305,370,140,right,35); place(window,306,370,183,right,23);
        place(window,modeId,370,210,right,200); place(window,310,370,251,right,52); place(window,307,370,320,right,164);
        place(window,311,390,351,240,24); place(window,gainId,w-168,345,72,29); place(window,312,w-90,351,66,24);
        place(window,invertId,390,390,right-36,26); place(window,axisId,390,431,right-36,26);
        place(window,320,24,h-58,w-290,39); place(window,IDOK,w-244,h-61,105,35); place(window,IDCANCEL,w-124,h-61,100,35);
        ListView_SetColumnWidth(list,0,px(170)); ListView_SetColumnWidth(list,1,px(132));
    }
    void runSelfTest() {
        add(L"C:\\Test Apps Ω\\Telegram.exe");
        SendMessageW(GetDlgItem(window,modeId),CB_SETCURSEL,static_cast<WPARAM>(GestureMode::Touchscreen),0); modeDescription();
        SetWindowTextW(GetDlgItem(window,gainId),L"14,5"); SendMessageW(GetDlgItem(window,axisId),BM_SETCHECK,BST_CHECKED,0);
        bool ok=save(false); auto read=loadGestureSettings(path); auto chosen=profileForImage(read,L"C:\\Test Apps Ω\\Telegram.exe");
        ok=ok && chosen.mode==GestureMode::Touchscreen && chosen.touch.pixelsPerMm==14.5 && chosen.touch.lockVertical;
        ok=ok && IsWindowEnabled(GetDlgItem(window,invertId)) && IsWindowEnabled(GetDlgItem(window,removeId));
        SetWindowTextW(GetDlgItem(window,gainId),L"0"); ok=!commit(false) && ok; SetWindowTextW(GetDlgItem(window,gainId),L"14,5");
        ok=save(false) && ok;
        PointerDialog pointer; pointer.value=data.pointer; HWND pointerWindow=pointer.create(window);
        if(pointerWindow) {
            ShowWindow(pointerWindow,SW_SHOWNOACTIVATE);
            SendMessageW(GetDlgItem(pointerWindow,400),BM_SETCHECK,BST_CHECKED,0);
            SendMessageW(GetDlgItem(pointerWindow,403),BM_SETCHECK,BST_CHECKED,0);
            SendMessageW(GetDlgItem(pointerWindow,401),TBM_SETPOS,TRUE,35);
            SendMessageW(GetDlgItem(pointerWindow,404),TBM_SETPOS,TRUE,-25);
            SetWindowTextW(GetDlgItem(pointerWindow,406),L"800"); pointer.refresh();
            ok=pointer.commit(false) && ok; data.pointer=pointer.value;
            ok=data.pointer.enabled && data.pointer.mouseAdaptive && data.pointer.touchpadSpeed==.35 && data.pointer.mouseSpeed==-.25 && data.pointer.mouseDpi==800 && ok;
            UpdateWindow(pointerWindow); ok=capture(pointerWindow,testImage+L".pointer.png") && ok;
            SetWindowTextW(GetDlgItem(pointerWindow,406),L"0"); ok=!pointer.commit(false) && ok;
            DestroyWindow(pointerWindow); ok=save(false) && loadGestureSettings(path).pointer==data.pointer && ok;
        } else ok=false;
        UpdateWindow(window); ok=capture(window,testImage) && ok;
        Picker picker;
        HWND pick=CreateWindowExW(WS_EX_DLGMODALFRAME,pickerClass,L"Add application",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,px(80),px(80),px(760),px(465),window,nullptr,GetModuleHandleW(nullptr),&picker);
        if(pick) {
            ShowWindow(pick,SW_SHOWNOACTIVATE); UpdateWindow(pick);
            ok=capture(pick,testImage+L".picker.png") && ok;
            if(!picker.images.empty()) { SetWindowTextW(GetDlgItem(pick,200),executableBase(picker.images[0]).c_str()); ok=!picker.shown.empty() && ok; picker.choose(); ok=!picker.selected.empty() && ok; }
            else ok=false;
            if(IsWindow(pick)) DestroyWindow(pick);
        } else ok=false;
        result=ok?0:1; dirty=false; DestroyWindow(window);
    }
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
        auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE) { self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); self->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
        if(!self) return DefWindowProcW(window,message,wp,lp);
        if(message==WM_CREATE) {
            control(window,L"STATIC",L"Touchpad gestures",0,300); SendMessageW(GetDlgItem(window,300),WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
            control(window,L"STATIC",L"Configure gesture handling for each application.",0,301);
            control(window,L"BUTTON",L"Pointer acceleration…",BS_PUSHBUTTON|WS_TABSTOP,pointerId);
            control(window,L"STATIC",L"Applications",0,302);
            self->list=control(window,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER,appsId);
            ListView_SetExtendedListViewStyle(self->list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER); SetWindowTheme(self->list,L"Explorer",nullptr);
            column(self->list,0,L"Application",170); column(self->list,1,L"Gestures",132);
            control(window,L"BUTTON",L"Add application…",BS_PUSHBUTTON|WS_TABSTOP,addId); control(window,L"BUTTON",L"Remove",BS_PUSHBUTTON|WS_TABSTOP,removeId);
            control(window,L"STATIC",L"",SS_ENDELLIPSIS,304); SendMessageW(GetDlgItem(window,304),WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE);
            control(window,L"STATIC",L"",SS_ENDELLIPSIS,305); control(window,L"STATIC",L"Horizontal gestures",0,306);
            HWND modes=control(window,WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,modeId);
            for(auto mode:{GestureMode::None,GestureMode::WindowsV1,GestureMode::Touchscreen}) SendMessageW(modes,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(gestureModeLabel(mode)));
            control(window,L"STATIC",L"",0,310); control(window,L"BUTTON",L"Touchscreen emulation",BS_GROUPBOX,307);
            control(window,L"STATIC",L"Sensitivity",0,311); control(window,L"EDIT",L"10",ES_AUTOHSCROLL|WS_TABSTOP,gainId); control(window,L"STATIC",L"px/mm",0,312);
            control(window,L"BUTTON",L"Invert horizontal direction",BS_AUTOCHECKBOX|WS_TABSTOP,invertId);
            control(window,L"BUTTON",L"Lock horizontal axis",BS_AUTOCHECKBOX|WS_TABSTOP,axisId);
            control(window,L"STATIC",L"Changes take effect between gestures.",0,320);
            control(window,L"BUTTON",L"Apply",BS_DEFPUSHBUTTON|WS_TABSTOP,IDOK); control(window,L"BUTTON",L"Close",BS_PUSHBUTTON|WS_TABSTOP,IDCANCEL);
            self->rows(0); EnableWindow(GetDlgItem(window,IDOK),FALSE); return 0;
        }
        if(message==WM_SIZE) { self->layout(); return 0; }
        if(message==WM_GETMINMAXINFO) { auto* limits=reinterpret_cast<MINMAXINFO*>(lp); limits->ptMinTrackSize={px(930),px(610)}; return 0; }
        if(message==WM_DPICHANGED) { fonts(HIWORD(wp)); auto* suggested=reinterpret_cast<RECT*>(lp); SetWindowPos(window,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE); EnumChildWindows(window,[](HWND child,LPARAM)->BOOL { SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(normalFont),TRUE); return TRUE; },0); SendMessageW(GetDlgItem(window,300),WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE); SendMessageW(GetDlgItem(window,304),WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE); self->layout(); return 0; }
        if(message==WM_CTLCOLORSTATIC) { SetBkColor(reinterpret_cast<HDC>(wp),RGB(248,250,252)); SetTextColor(reinterpret_cast<HDC>(wp),RGB(35,47,62)); return reinterpret_cast<LRESULT>(background); }
        if(message==WM_COMMAND && !self->loading) {
            int id=LOWORD(wp),notification=HIWORD(wp);
            if(id==IDOK) self->save(); else if(id==IDCANCEL) SendMessageW(window,WM_CLOSE,0,0);
            else if(id==addId && self->commit()) { Picker picker; self->add(picker.show(window)); }
            else if(id==pointerId && self->commit()) { PointerDialog dialog; dialog.value=self->data.pointer; if(dialog.show(window)) { self->data.pointer=dialog.value; self->markDirty(); } }
            else if(id==removeId && self->selected>0) { int old=self->selected; self->data.applications.erase(self->data.applications.begin()+old-1); self->rows(std::min(old,static_cast<int>(self->data.applications.size()))); self->markDirty(); }
            else if(id==modeId && notification==CBN_SELCHANGE) { self->modeDescription(); self->markDirty(); }
            else if((id==gainId && notification==EN_CHANGE) || id==invertId || id==axisId) self->markDirty(); return 0;
        }
        if(message==WM_NOTIFY && !self->loading) {
            auto* notice=reinterpret_cast<NMLISTVIEW*>(lp);
            if(notice->hdr.idFrom==appsId && notice->hdr.code==LVN_ITEMCHANGED && (notice->uNewState&LVIS_SELECTED) && notice->iItem!=self->selected) {
                if(self->commit()) self->load(notice->iItem); else { self->loading=true; ListView_SetItemState(self->list,notice->iItem,0,LVIS_SELECTED); ListView_SetItemState(self->list,self->selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED); self->loading=false; }
            }
        }
        if(message==WM_CLOSE) {
            if(self->dirty) { int answer=MessageBoxW(window,L"Save changes to settings?",L"Touchpad gestures",MB_YESNOCANCEL|MB_ICONQUESTION); if(answer==IDCANCEL || (answer==IDYES && !self->save())) return 0; }
            DestroyWindow(window); return 0;
        }
        if(message==WM_APP+33) { self->runSelfTest(); return 0; }
        if(message==WM_DESTROY) { PostQuitMessage(self->result); return 0; }
        return DefWindowProcW(window,message,wp,lp);
    }
};
inline int run(const std::wstring& path,bool selfTest=false,const std::wstring& image={}) {
    HANDLE singleton=nullptr;
    if(!selfTest) {
        // The tray may launch an elevated settings process; an ordinary launch
        // can still find and activate its window without opening its mutex.
        HWND existing=FindWindowW(mainClass,nullptr);
        if(existing) { ShowWindowAsync(existing,SW_RESTORE); SetForegroundWindow(existing); return 0; }
        singleton=CreateMutexW(nullptr,TRUE,L"Local\\LibinputASCE1200SettingsWindow");
        if(singleton && GetLastError()==ERROR_ALREADY_EXISTS) { existing=FindWindowW(mainClass,nullptr); if(existing) { ShowWindowAsync(existing,SW_RESTORE); SetForegroundWindow(existing); } CloseHandle(singleton); return 0; }
    }
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&common);
    Gdiplus::GdiplusStartupInput startup; ULONG_PTR graphics=0; Gdiplus::GdiplusStartup(&graphics,&startup,nullptr);
    fonts(GetDpiForSystem()); background=CreateSolidBrush(RGB(248,250,252));
    WNDCLASSW wc{}; wc.lpfnWndProc=Window::procedure; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=mainClass; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=background;
    wc.hIcon=static_cast<HICON>(LoadImageW(wc.hInstance,MAKEINTRESOURCEW(101),IMAGE_ICON,32,32,LR_SHARED)); RegisterClassW(&wc);
    wc.lpfnWndProc=Picker::procedure; wc.lpszClassName=pickerClass; wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1); RegisterClassW(&wc);
    wc.lpfnWndProc=PointerDialog::procedure; wc.lpszClassName=pointerClass; wc.hbrBackground=background; RegisterClassW(&wc);
    Window state; state.path=path; state.data=loadGestureSettings(path); state.selfTest=selfTest; state.testImage=image;
    HWND window=CreateWindowExW(0,mainClass,L"libinput — Settings",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,px(960),px(640),nullptr,nullptr,GetModuleHandleW(nullptr),&state);
    if(!window) return 1; ShowWindow(window,selfTest?SW_SHOWNOACTIVATE:SW_SHOW); UpdateWindow(window);
    if(selfTest) PostMessageW(window,WM_APP+33,0,0);
    MSG message{}; while(GetMessageW(&message,nullptr,0,0)>0) if(!IsDialogMessageW(window,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    Gdiplus::GdiplusShutdown(graphics); DeleteObject(normalFont); DeleteObject(titleFont); DeleteObject(sectionFont); normalFont=titleFont=sectionFont=nullptr; DeleteObject(background);
    if(singleton) { ReleaseMutex(singleton); CloseHandle(singleton); } return static_cast<int>(message.wParam);
}
}
