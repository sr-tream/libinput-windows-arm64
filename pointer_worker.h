#pragma once
#include "pointer_acceleration.h"
#include <memory>

namespace PointerBackend {
static constexpr ULONG_PTR outputTag=0x4c495054;
static constexpr wchar_t stopName[]=L"Local\\LibinputASCE1200PointerStop";
static PointerSettings options;
static TouchpadPointer touchpad;
static std::map<HANDLE,std::unique_ptr<AdaptivePointer>> mice;
static std::atomic<bool> gate{false};
static std::atomic<ULONGLONG> pulse{0};
static std::atomic<uint64_t> issued{0},acknowledged{0},blocked{0};
static std::thread hookThread;
static DWORD hookThreadId=0;
static HANDLE hookReady=nullptr;
static bool hookInstalled=false;
static double fractionX=0,fractionY=0;
static ULONG previousPointerScan=0;
static uint64_t pointerTime=0,lastHidWall=0,pendingSince=0;
static bool stopRequested=false;
static unsigned emitted=0,mouseForwarded=0;

static LRESULT CALLBACK hook(int code,WPARAM message,LPARAM data) {
    if(code==HC_ACTION) {
        const auto* input=reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        if((input->flags&LLMHF_INJECTED) && input->dwExtraInfo==outputTag) ++acknowledged;
        // Only motion is replaced. Native buttons, wheels, promoted touch/pen,
        // automation and our own injected events stay on their existing path.
        if(message==WM_MOUSEMOVE && !(input->flags&LLMHF_INJECTED) && input->dwExtraInfo==0 && gate.load(std::memory_order_relaxed)) {
            if(GetTickCount64()-pulse.load(std::memory_order_relaxed)<=100) { ++blocked; return 1; }
            gate.store(false,std::memory_order_relaxed);
        }
    }
    return CallNextHookEx(nullptr,code,message,data);
}
static void fail(const char* reason) { gate.store(false); log(reason); PostQuitMessage(42); }
static bool sendMotion(INPUT& event) {
    if(!gate.load()) return false;
    const auto now=monotonicUsec();
    if(issued.load()>acknowledged.load()) {
        if(now-pendingSince>20000) { fail("ERROR: pointer hook acknowledgement lost; returning to native motion"); return false; }
    } else pendingSince=now;
    event.type=INPUT_MOUSE; event.mi.dwExtraInfo=outputTag;
    ++issued;
    if(SendInput(1,&event,sizeof(event))!=1) { fail("ERROR: pointer injection failed; returning to native motion"); return false; }
    ++emitted; return true;
}
static void emitAbsolute(normalized_coords delta) {
    if(!std::isfinite(delta.x) || !std::isfinite(delta.y)) { fail("ERROR: nonfinite pointer output"); return; }
    fractionX+=delta.x; fractionY+=delta.y;
    LONG dx=static_cast<LONG>(std::round(fractionX)),dy=static_cast<LONG>(std::round(fractionY));
    fractionX-=dx; fractionY-=dy;
    if(!dx && !dy) return;
    POINT current{};
    if(!GetPhysicalCursorPos(&current)) { fail("ERROR: pointer position unavailable"); return; }
    LONG left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN),width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(width<=0 || height<=0) { fail("ERROR: invalid desktop bounds"); return; }
    LONG x=std::clamp(current.x+dx,left,left+width-1),y=std::clamp(current.y+dy,top,top+height-1);
    if(x!=current.x+dx) fractionX=0; if(y!=current.y+dy) fractionY=0;
    INPUT input{}; input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|MOUSEEVENTF_MOVE_NOCOALESCE;
    input.mi.dx=static_cast<LONG>(((2ll*(x-left)+1)*65536)/(2ll*width));
    input.mi.dy=static_cast<LONG>(((2ll*(y-top)+1)*65536)/(2ll*height));
    sendMotion(input);
}
static void mouseInput(const RAWINPUT& raw) {
    const auto& mouse=raw.data.mouse;
    if(mouse.ulExtraInformation || !raw.header.hDevice || (!mouse.lLastX && !mouse.lLastY)) return;
    if(mouse.usFlags&MOUSE_MOVE_ABSOLUTE) { fail("ERROR: absolute mouse device is unsupported by pointer replacement"); return; }
    if(options.mouseAdaptive) {
        auto& filter=mice[raw.header.hDevice];
        if(!filter) { filter=std::make_unique<AdaptivePointer>(PointerDevice::Mouse,options.mouseDpi); filter->setSpeed(options.mouseSpeed); }
        emitAbsolute(filter->feed({double(mouse.lLastX),double(mouse.lLastY)},monotonicUsec()));
    } else {
        INPUT input{}; input.mi.dx=mouse.lLastX; input.mi.dy=mouse.lLastY; input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_MOVE_NOCOALESCE;
        sendMotion(input); ++mouseForwarded;
    }
}
static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_INPUT) {
        UINT bytes=0;
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,nullptr,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1)) return DefWindowProcW(window,message,wp,lp);
        alignas(RAWINPUT) BYTE local[256]; std::vector<BYTE> overflow;
        if(bytes>sizeof(local)) overflow.resize(bytes);
        BYTE* storage=overflow.empty()?local:overflow.data();
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,storage,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1)) return DefWindowProcW(window,message,wp,lp);
        auto* input=reinterpret_cast<RAWINPUT*>(storage);
        if(input->header.dwType==RIM_TYPEMOUSE) mouseInput(*input);
        else if(input->header.dwType==RIM_TYPEHID && input->header.hDevice==rawDevice) {
            size_t offset=offsetof(RAWINPUT,data.hid.bRawData),required=static_cast<size_t>(input->data.hid.dwCount)*input->data.hid.dwSizeHid;
            if(bytes>=offset && required<=bytes-offset) for(DWORD i=0;i<input->data.hid.dwCount;++i) parse(input->data.hid.bRawData+i*input->data.hid.dwSizeHid,input->data.hid.dwSizeHid,input->header.hDevice);
        }
    }
    if(message==WM_HOTKEY || message==WM_CLOSE || (message==WM_POWERBROADCAST && wp==PBT_APMSUSPEND) || message==WM_INPUT_DEVICE_CHANGE) { gate.store(false); stopRequested=true; PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,wp,lp);
}
static int run(PointerSettings selected,unsigned seconds,DWORD parent=0) {
    options=selected; pointerWorkerMode=true; touchpad.setSpeed(options.touchpadSpeed);
    auto logPath=appDirectory()+L"\\pointer.log"; logFile=_wfsopen(logPath.c_str(),L"w",_SH_DENYNO);
    HANDLE singleton=CreateMutexW(nullptr,TRUE,L"Local\\LibinputASCE1200PointerWorker");
    if(!singleton || GetLastError()==ERROR_ALREADY_EXISTS) return 5;
    inspect();
    TOUCHPAD_PARAMETERS_V1 settings{};
    if(!device.pp || ambiguous || !rawDevice || !getSettings(settings) || !settings.touchpadActive || xUnit<=0 || yUnit<=0 || device.attrs.VendorID!=0x04f3 || device.attrs.ProductID!=0x32e2 || device.caps.InputReportByteLength!=42) { log("ERROR: pointer device validation failed"); return 7; }
    HANDLE stop=createUserStopEvent(stopName);
    HANDLE owner=parent?OpenProcess(SYNCHRONIZE,FALSE,parent):nullptr;
    if(!stop || (parent && !owner)) return 6;
    WNDCLASSW wc{}; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"LibinputAdaptivePointerWorker"; wc.lpfnWndProc=procedure; RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"libinput adaptive pointer",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
    RAWINPUTDEVICE devices[]{{1,2,RIDEV_INPUTSINK,window},{0x0d,5,RIDEV_INPUTSINK,window}};
    if(!window || !RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE))) return 3;
    MSG message{};
    RegisterHotKey(window,81,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F11);
    RegisterHotKey(window,82,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F12);
    hookReady=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    hookThread=std::thread([] {
        hookThreadId=GetCurrentThreadId(); MSG event{}; PeekMessageW(&event,nullptr,0,0,PM_NOREMOVE);
        HHOOK installed=SetWindowsHookExW(WH_MOUSE_LL,hook,GetModuleHandleW(nullptr),0); hookInstalled=installed!=nullptr; SetEvent(hookReady);
        if(installed) { while(GetMessageW(&event,nullptr,0,0)>0) { TranslateMessage(&event); DispatchMessageW(&event); } UnhookWindowsHookEx(installed); }
    });
    WaitForSingleObject(hookReady,3000); CloseHandle(hookReady);
    if(!hookInstalled) { if(hookThread.joinable()) hookThread.join(); return 43; }
    pulse.store(GetTickCount64()); gate.store(true);
    char line[300]; std::snprintf(line,sizeof(line),"POINTER_ACTIVE touchpad_speed=%.3f mouse_adaptive=%d mouse_speed=%.3f mouse_dpi=%u seconds=%u elevated=%d",options.touchpadSpeed,options.mouseAdaptive,options.mouseSpeed,options.mouseDpi,seconds,processIsElevated()); log(line);
    ULONGLONG end=seconds?GetTickCount64()+seconds*1000:0; int result=0;
    HANDLE handles[]{stop,owner}; DWORD count=owner?2:1;
    for(bool quit=false;!quit;) {
        pulse.store(GetTickCount64());
        DWORD ready=MsgWaitForMultipleObjectsEx(count,handles,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(ready>=WAIT_OBJECT_0 && ready<WAIT_OBJECT_0+count) break;
        if(ready==WAIT_FAILED) { result=44; break; }
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if(message.message==WM_QUIT) { result=static_cast<int>(message.wParam); quit=true; break; }
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if(!gate.load()) { result=42; break; }
        if(end && GetTickCount64()>=end) break;
    }
    gate.store(false); PostThreadMessageW(hookThreadId,WM_QUIT,0,0); if(hookThread.joinable()) hookThread.join();
    UnregisterHotKey(window,81); UnregisterHotKey(window,82); DestroyWindow(window);
    if(owner) CloseHandle(owner); CloseHandle(stop); ReleaseMutex(singleton); CloseHandle(singleton);
    std::snprintf(line,sizeof(line),"POINTER_END blocked=%llu output=%u mouse_forwarded=%u acknowledgements=%llu result=%d stop_requested=%d",static_cast<unsigned long long>(blocked.load()),emitted,mouseForwarded,static_cast<unsigned long long>(acknowledged.load()),result,stopRequested); log(line);
    if(logFile) { std::fclose(logFile); logFile=nullptr; } return result;
}
}
static void feedPointerContacts(const std::vector<Contact>& contacts,ULONG scan,bool) {
    const auto now=GetTickCount64();
    if(!PointerBackend::lastHidWall || now-PointerBackend::lastHidWall>100) { PointerBackend::pointerTime=monotonicUsec(); PointerBackend::touchpad.reset(); }
    else { ULONG delta=(scan-PointerBackend::previousPointerScan)&0xffff; if(!delta) return; PointerBackend::pointerTime+=static_cast<uint64_t>(delta)*100; }
    PointerBackend::lastHidWall=now; PointerBackend::previousPointerScan=scan;
    PointerBackend::emitAbsolute(PointerBackend::touchpad.feed(contacts,PointerBackend::pointerTime));
}

static HANDLE pointerChild=nullptr,pointerChildStop=nullptr;
static PointerSettings pointerRequested,pointerApplied;
static bool pointerFailed=false,pointerExpectedStop=false;
static void updatePointerWorker() {
    const auto desired=profileRuntime.pointerOptions();
    if(!(desired==pointerRequested)) { pointerRequested=desired; pointerFailed=false; }
    if(pointerChild && WaitForSingleObject(pointerChild,0)==WAIT_OBJECT_0) {
        DWORD result=0; GetExitCodeProcess(pointerChild,&result); CloseHandle(pointerChild); pointerChild=nullptr;
        if(!pointerExpectedStop) { pointerFailed=true; char line[180]; std::snprintf(line,sizeof(line),"POINTER_WORKER stopped result=%lu; native motion restored; toggle pointer settings to retry",result); log(line); }
        pointerExpectedStop=false;
    }
    if(pointerChild) {
        if(!desired.enabled || !(desired==pointerApplied)) { pointerExpectedStop=true; SetEvent(pointerChildStop); }
        return;
    }
    if(!desired.enabled || pointerFailed) return;
    if(!pointerChildStop) pointerChildStop=createUserStopEvent(PointerBackend::stopName);
    if(!pointerChildStop) { pointerFailed=true; log("ERROR: pointer stop event unavailable; native motion retained"); return; }
    ResetEvent(pointerChildStop);
    auto executable=appDirectory()+L"\\libinput-scroll.exe";
    auto command=L"\""+executable+L"\" --pointer-worker "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) { pointerFailed=true; log("ERROR: pointer worker could not start; native motion retained"); return; }
    CloseHandle(process.hThread); pointerChild=process.hProcess; pointerApplied=desired; log("POINTER_WORKER started; motion-only interception runs in a separate process");
}
static void stopPointerWorker() {
    if(pointerChild) {
        if(pointerChildStop) SetEvent(pointerChildStop);
        if(WaitForSingleObject(pointerChild,1500)==WAIT_TIMEOUT) { log("POINTER_WORKER stop timeout; terminating input helper to restore native movement"); TerminateProcess(pointerChild,45); WaitForSingleObject(pointerChild,500); }
        CloseHandle(pointerChild); pointerChild=nullptr;
    }
    if(pointerChildStop) { CloseHandle(pointerChildStop); pointerChildStop=nullptr; }
}
