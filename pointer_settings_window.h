#pragma once
// Included inside SettingsUi after the shared window/control helpers.
static constexpr wchar_t pointerClass[]=L"LibinputPointerSettings";
struct PointerDialog {
    HWND window=nullptr;
    PointerSettings value;
    bool accepted=false;
    void refresh() {
        bool enabled=SendMessageW(GetDlgItem(window,400),BM_GETCHECK,0,0)==BST_CHECKED;
        bool mouse=enabled && SendMessageW(GetDlgItem(window,403),BM_GETCHECK,0,0)==BST_CHECKED;
        for(int id:{401,402,403,410,411,412}) EnableWindow(GetDlgItem(window,id),enabled);
        for(int id:{404,405,406,413,414,415}) EnableWindow(GetDlgItem(window,id),mouse);
        for(int id:{401,404}) {
            auto speed=static_cast<int>(SendMessageW(GetDlgItem(window,id),TBM_GETPOS,0,0));
            wchar_t label[24]; swprintf_s(label,L"%+.2f",speed/100.0); SetWindowTextW(GetDlgItem(window,id+1),label);
        }
    }
    bool commit(bool report=true) {
        auto dpiText=text(GetDlgItem(window,406)); wchar_t* end=nullptr; unsigned long dpi=wcstoul(dpiText.c_str(),&end,10);
        if(end==dpiText.c_str() || *end || dpi<100 || dpi>64000) {
            if(report) MessageBoxW(window,L"Mouse DPI must be a whole number from 100 to 64000.",L"Invalid value",MB_OK|MB_ICONINFORMATION);
            SetFocus(GetDlgItem(window,406)); return false;
        }
        value.enabled=SendMessageW(GetDlgItem(window,400),BM_GETCHECK,0,0)==BST_CHECKED;
        value.mouseAdaptive=SendMessageW(GetDlgItem(window,403),BM_GETCHECK,0,0)==BST_CHECKED;
        value.touchpadSpeed=static_cast<int>(SendMessageW(GetDlgItem(window,401),TBM_GETPOS,0,0))/100.0;
        value.mouseSpeed=static_cast<int>(SendMessageW(GetDlgItem(window,404),TBM_GETPOS,0,0))/100.0;
        value.mouseDpi=static_cast<unsigned>(dpi); return true;
    }
    HWND create(HWND owner) {
        RECT bounds{}; GetWindowRect(owner,&bounds);
        return CreateWindowExW(WS_EX_DLGMODALFRAME,pointerClass,L"Pointer acceleration",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
            bounds.left+px(65),bounds.top+px(35),px(640),px(525),owner,nullptr,GetModuleHandleW(nullptr),this);
    }
    bool show(HWND owner) {
        HWND dialog=create(owner); if(!dialog) return false;
        EnableWindow(owner,FALSE); ShowWindow(dialog,SW_SHOW); SetFocus(GetDlgItem(dialog,400));
        MSG message{}; while(IsWindow(dialog) && GetMessageW(&message,nullptr,0,0)>0) if(!IsDialogMessageW(dialog,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        EnableWindow(owner,TRUE); SetForegroundWindow(owner); return accepted;
    }
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
        auto* self=reinterpret_cast<PointerDialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE) { self=static_cast<PointerDialog*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); self->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self)); }
        if(!self) return DefWindowProcW(window,message,wp,lp);
        if(message==WM_CREATE) {
            control(window,L"BUTTON",L"Enable adaptive pointer acceleration",BS_AUTOCHECKBOX|WS_TABSTOP,400);
            control(window,L"STATIC",L"Touchpad speed",0,410);
            control(window,TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,401); control(window,L"STATIC",L"",SS_RIGHT,402);
            control(window,L"STATIC",L"Slower",0,411); control(window,L"STATIC",L"Faster",SS_RIGHT,412);
            control(window,L"BUTTON",L"Also accelerate the external mouse",BS_AUTOCHECKBOX|WS_TABSTOP,403);
            control(window,L"STATIC",L"Mouse speed",0,413);
            control(window,TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,404); control(window,L"STATIC",L"",SS_RIGHT,405);
            control(window,L"STATIC",L"Mouse DPI",0,414); control(window,L"EDIT",std::to_wstring(self->value.mouseDpi).c_str(),ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,406);
            control(window,L"STATIC",L"Match the DPI selected on your mouse.",0,415);
            control(window,L"STATIC",L"Uses libinput's adaptive profile. Leave the mouse option off to keep Windows mouse acceleration. Applies after you click Apply in Settings.",0,416);
            control(window,L"STATIC",L"Ctrl+Alt+F11 restores native pointer movement.",0,417);
            control(window,L"BUTTON",L"Use settings",BS_DEFPUSHBUTTON|WS_TABSTOP,IDOK); control(window,L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,IDCANCEL);
            SendMessageW(GetDlgItem(window,400),BM_SETCHECK,self->value.enabled?BST_CHECKED:BST_UNCHECKED,0);
            SendMessageW(GetDlgItem(window,403),BM_SETCHECK,self->value.mouseAdaptive?BST_CHECKED:BST_UNCHECKED,0);
            for(int id:{401,404}) { SendMessageW(GetDlgItem(window,id),TBM_SETRANGE,TRUE,MAKELPARAM(-100,100)); SendMessageW(GetDlgItem(window,id),TBM_SETTICFREQ,25,0); }
            SendMessageW(GetDlgItem(window,401),TBM_SETPOS,TRUE,static_cast<LPARAM>(std::round(self->value.touchpadSpeed*100)));
            SendMessageW(GetDlgItem(window,404),TBM_SETPOS,TRUE,static_cast<LPARAM>(std::round(self->value.mouseSpeed*100)));
            place(window,400,24,24,570,30); place(window,410,24,76,380,24); place(window,402,510,76,85,24);
            place(window,401,24,106,574,36); place(window,411,24,145,100,24); place(window,412,495,145,100,24);
            place(window,403,24,192,570,30); place(window,413,24,236,380,24); place(window,405,510,236,85,24);
            place(window,404,24,265,574,36); place(window,414,24,316,100,26); place(window,406,126,310,92,30); place(window,415,230,316,370,26);
            place(window,416,24,359,574,48); place(window,417,24,411,574,24);
            place(window,IDOK,350,446,130,33); place(window,IDCANCEL,490,446,105,33);
            self->refresh(); return 0;
        }
        if(message==WM_HSCROLL) { self->refresh(); return 0; }
        if(message==WM_CTLCOLORSTATIC) { SetBkColor(reinterpret_cast<HDC>(wp),RGB(248,250,252)); return reinterpret_cast<LRESULT>(background); }
        if(message==WM_COMMAND) {
            int id=LOWORD(wp);
            if(id==400 || id==403) self->refresh();
            else if(id==IDOK && self->commit()) { self->accepted=true; DestroyWindow(window); }
            else if(id==IDCANCEL) DestroyWindow(window);
            return 0;
        }
        if(message==WM_CLOSE) { DestroyWindow(window); return 0; }
        return DefWindowProcW(window,message,wp,lp);
    }
};
