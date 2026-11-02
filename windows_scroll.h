#pragma once
#include <shellapi.h>
#include <share.h>
#include "libinput_scroll.h"
#include "momentum.h"
#include "latency_stats.h"
#include "windows_access.h"
#include "gesture_handoff.h"
#include "touchscreen_output.h"
#include "gesture_settings.h"
static void openSettingsWindow();
static bool appProfiles=false,profileLatched=false;
static bool pointerWorkerMode=false;
static void feedPointerContacts(const std::vector<Contact>&,ULONG,bool);
static void updatePointerWorker();
static void stopPointerWorker();
static GestureProfileRuntime profileRuntime;
static ProfileGestureRouter profileRouter;
static bool runMode=false, ambiguous=false, panChanged=false, originalPan=true, reversed=false, live=false;
static HANDLE stopEvent=nullptr, instanceMutex=nullptr, watchdogProcess=nullptr, watchdogDone=nullptr;
static unsigned frames=0,twoFingerFrames=0,wheelEvents=0,rejected=0,momentumEvents=0;
static double wheelScale=3.5,xUnit=0,yUnit=0;
static bool invertDirection=true,momentumEnabled=true;
static bool collectLatency=false;
static bool handoffPanEnabled=false;
static TouchscreenOutput touchscreenOutput;
static unsigned touchMouseEchoes=0;
static void logTouchEnd() {
    const auto event=touchscreenOutput.takeEnd(); if(!event.pending) return;
    char line[300]; std::snprintf(line,sizeof(line),"TOUCH_END reason=%s canceled=%d frames=%u moves=%u elapsed_ms=%lu dx_px=%ld dy_px=%ld",event.reason,event.canceled,event.frames,event.moves,event.elapsedMs,event.dx,event.dy); log(line);
}
static unsigned nativeGestures=0,customGestures=0;
static WheelAccumulator wheel;
static MomentumScroll momentum;
static HANDLE inertiaTimer=nullptr;
static HWINEVENTHOOK focusHook=nullptr;
static DWORD wheelRouting=MOUSEWHEEL_ROUTING_MOUSE_POS;
static bool processorNeedsReset=false;
static unsigned clickStops=0,contextStops=0;
static constexpr ULONG_PTR directScrollTag=0x4c495343;
struct ScrollContext {
    HWND foreground=nullptr,target=nullptr;
    bool valid() const { return foreground && target; }
    bool matches(const ScrollContext& other) const {
        return valid() && other.valid() && foreground==other.foreground && target==other.target;
    }
};
static ScrollContext scrollContext;
static DWORD scrollContextBoundAt=0;
static ScrollProcessor processor;
static uint64_t scanEpoch=0,lastReportWall=0;
static ULONG previousScan=0;
static NOTIFYICONDATAW tray{};
static ULONGLONG deadline=0;
static const wchar_t* stopName=L"Local\\LibinputASCE1200ScrollStop";
static std::wstring appDirectory() {
    wchar_t exe[32768]; DWORD n=GetModuleFileNameW(nullptr,exe,32768);
    std::wstring path(exe,n); auto slash=path.find_last_of(L"\\/");
    return slash==std::wstring::npos?L".":path.substr(0,slash);
}
static void loadConfiguration() {
    auto path=appDirectory()+L"\\settings.ini"; wchar_t value[128];
    GetPrivateProfileStringW(L"scroll",L"scale",L"3.5",value,128,path.c_str()); wheelScale=std::clamp(_wtof(value),0.1,10.0);
    invertDirection=GetPrivateProfileIntW(L"scroll",L"reverse_native_direction",1,path.c_str())!=0;
    momentumEnabled=GetPrivateProfileIntW(L"scroll",L"inertia",1,path.c_str())!=0;
    GetPrivateProfileStringW(L"scroll",L"inertia_friction",L"0.002",value,128,path.c_str()); momentum.setFriction(_wtof(value));
}
static int supervise() {
    if(!processIsElevated()) {
        wchar_t exe[32768]; if(!GetModuleFileNameW(nullptr,exe,32768)) return 1;
        SHELLEXECUTEINFOW request{}; request.cbSize=sizeof(request);
        request.fMask=SEE_MASK_NOCLOSEPROCESS;
        request.lpVerb=L"runas"; request.lpFile=exe; request.lpParameters=L"--supervise";
        request.nShow=SW_HIDE;
        if(!ShellExecuteExW(&request)) return static_cast<int>(GetLastError());
        if(request.hProcess) CloseHandle(request.hProcess);
        return 0;
    }
    SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
    HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\LibinputASCE1200Supervisor");
    if(!mutex || GetLastError()==ERROR_ALREADY_EXISTS) return 0;
    HANDLE stop=createUserStopEvent(stopName); if(!stop) { CloseHandle(mutex); return 1; }
    auto root=appDirectory(),exe=root+L"\\libinput-scroll.exe";
    for(;;) {
        if(WaitForSingleObject(stop,0)==WAIT_OBJECT_0) break;
        std::wstring cmd=L"\""+exe+L"\" --run --app-profiles --log \""+root+L"\\session.log\"";
        STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
        if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&si,&pi)) break;
        CloseHandle(pi.hThread); HANDLE handles[]={stop,pi.hProcess};
        DWORD result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
        if(result==WAIT_OBJECT_0) { WaitForSingleObject(pi.hProcess,5000); CloseHandle(pi.hProcess); break; }
        DWORD code=1; GetExitCodeProcess(pi.hProcess,&code); CloseHandle(pi.hProcess);
        // Hot-unplug, suspend and temporary device absence may recover.
        // Other failures leave native panning restored and stop the supervisor.
        if(code!=0 && code!=2 && code!=7) break;
        if(WaitForSingleObject(stop,5000)==WAIT_OBJECT_0) break;
    }
    CloseHandle(stop); ReleaseMutex(mutex); CloseHandle(mutex); return 0;
}
static uint64_t monotonicUsec() {
    static LARGE_INTEGER frequency=[] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart/frequency.QuadPart)*1000000+static_cast<uint64_t>(now.QuadPart%frequency.QuadPart)*1000000/frequency.QuadPart;
}
static LatencyStats rawHandlerLatency,rawQueueLatency,sendInputLatency;
struct RawInputLatencyScope {
    uint64_t entered=0;
    RawInputLatencyScope() {
        if(runMode && collectLatency) {
            entered=monotonicUsec();
            DWORD age=GetTickCount()-static_cast<DWORD>(GetMessageTime());
            if(age<=1000) rawQueueLatency.add(static_cast<uint64_t>(age)*1000);
        }
    }
    ~RawInputLatencyScope() { if(entered) rawHandlerLatency.add(monotonicUsec()-entered); }
};
static void logLatency(bool force=false) {
    static uint64_t loggedSamples=0,lastLog=0;
    uint64_t now=GetTickCount64();
    if(!runMode || !collectLatency || rawHandlerLatency.count()==loggedSamples || (!force && (rawHandlerLatency.count()<100 || now-lastLog<5000))) return;
    loggedSamples=rawHandlerLatency.count(); lastLog=now;
    char line[768];
    std::snprintf(line,sizeof(line),"LATENCY raw_n=%llu handler_mean_us=%.2f handler_p95_le_us=%llu handler_max_us=%llu queue_mean_ms=%.3f queue_p95_le_ms=%.3f queue_max_ms=%.3f send_n=%llu send_mean_us=%.2f send_p95_le_us=%llu send_max_us=%llu",rawHandlerLatency.count(),rawHandlerLatency.mean(),rawHandlerLatency.percentileUpper(95),rawHandlerLatency.max(),rawQueueLatency.mean()/1000.0,rawQueueLatency.percentileUpper(95)/1000.0,rawQueueLatency.max()/1000.0,sendInputLatency.count(),sendInputLatency.mean(),sendInputLatency.percentileUpper(95),sendInputLatency.max());
    log(line);
}
static bool armMomentumTimer() {
    LARGE_INTEGER due; due.QuadPart=-80000; // One high-resolution tick in 8ms.
    return inertiaTimer && SetWaitableTimer(inertiaTimer,&due,0,nullptr,nullptr,FALSE);
}
static ScrollContext currentScrollContext() {
    HWND foreground=GetForegroundWindow(),target=foreground;
    if(wheelRouting!=MOUSEWHEEL_ROUTING_FOCUS) {
        POINT point{};
        if(!GetPhysicalCursorPos(&point)) return {};
        // Use one coordinate space even when Windows scales this non-UI process.
        HWND underPointer=WindowFromPhysicalPoint(point);
        target=underPointer?GetAncestor(underPointer,GA_ROOT):nullptr;
    }
    return {foreground,target};
}
static void cancelMomentum(bool resetHistory=false) {
    if(resetHistory) { momentum.reset(); processorNeedsReset=true; }
    else momentum.cancelActive();
    scrollContext={}; scrollContextBoundAt=0; wheel.reset();
    if(inertiaTimer) CancelWaitableTimer(inertiaTimer);
}
static void observeMouseButtons(USHORT flags,ULONG extra=0,HANDLE sourceDevice=nullptr) {
    constexpr USHORT down=RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_RIGHT_BUTTON_DOWN|RI_MOUSE_MIDDLE_BUTTON_DOWN|RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN;
    // Raw Input is an asynchronous notification. We never intercept or wait
    // on the system's mouse event delivery, including our own wheel events.
    if(flags&down) {
        if(touchscreenOutput.isMouseEcho(flags,extra,sourceDevice)) {
            if(++touchMouseEchoes<=3) {
                char line[140]; std::snprintf(line,sizeof(line),"TOUCH_MOUSE_ECHO ignored=1 flags=%u extra=%lu",flags,extra); log(line);
            }
            return;
        }
        if(touchscreenOutput.active()) {
            INPUT_MESSAGE_SOURCE source{}; GetCurrentInputMessageSource(&source);
            char line[220]; std::snprintf(line,sizeof(line),"TOUCH_BUTTON flags=%u extra=%lu device=%p source_type=%d source_origin=%d",flags,extra,sourceDevice,source.deviceType,source.originId); log(line);
        }
        touchscreenOutput.finish(true,"raw_mouse_button"); logTouchEnd();
        if(momentum.active()) ++clickStops;
        cancelMomentum(true);
    }
}
static void CALLBACK scrollForegroundEvent(HWINEVENTHOOK,DWORD,HWND foreground,LONG,LONG,DWORD,DWORD eventTime) {
    if(appProfiles) profileRuntime.notifyContext();
    touchscreenOutput.foregroundChanged(foreground,eventTime); logTouchEnd();
    // Observe each transition, including away-and-back between timer ticks.
    if(scrollContext.valid() && static_cast<LONG>(eventTime-scrollContextBoundAt)>=0 && foreground!=scrollContext.foreground) {
        if(momentum.active()) ++contextStops;
        cancelMomentum(true);
    }
}
static bool startInteractionMonitoring() {
    SystemParametersInfoW(SPI_GETMOUSEWHEELROUTING,0,&wheelRouting,0);
    focusHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,scrollForegroundEvent,0,0,WINEVENT_OUTOFCONTEXT);
    if(focusHook) {
        char line[180]; std::snprintf(line,sizeof(line),"INTERACTION raw_mouse_buttons=1 foreground_events=1 wheel_routing=%lu inertia_target_guard=1",wheelRouting); log(line);
        return true;
    }
    return false;
}
static void stopInteractionMonitoring() {
    if(focusHook) { UnhookWinEvent(focusHook); focusHook=nullptr; }
}
static void emitScroll(normalized_coords d,bool inertial) {
    if(inertial && !scrollContext.matches(currentScrollContext())) {
        if(momentum.active()) ++contextStops;
        cancelMomentum(true); return;
    }
    auto delta=wheel.feed(d,wheelScale,reversed!=invertDirection);
    LONG x=delta.x,y=delta.y;
    if(!x && !y) return;
    INPUT input[2]{}; UINT n=0;
    const ULONG_PTR tag=directScrollTag;
    if(x) { input[n].type=INPUT_MOUSE; input[n].mi.dwFlags=MOUSEEVENTF_HWHEEL; input[n].mi.mouseData=static_cast<DWORD>(x); input[n].mi.dwExtraInfo=tag; ++n; }
    if(y) { input[n].type=INPUT_MOUSE; input[n].mi.dwFlags=MOUSEEVENTF_WHEEL; input[n].mi.mouseData=static_cast<DWORD>(y); input[n].mi.dwExtraInfo=tag; ++n; }
    uint64_t sendStart=collectLatency?monotonicUsec():0;
    UINT sent=SendInput(n,input,sizeof(INPUT));
    if(collectLatency) sendInputLatency.add(monotonicUsec()-sendStart);
    if(sent!=n) { log("ERROR: SendInput failed; restoring native panning"); PostQuitMessage(12); return; }
    wheelEvents+=n; if(inertial) momentumEvents+=n;
    if(appProfiles && !inertial) profileRouter.noteCustomOutput();
    if(logFile && (wheelEvents<=40 || wheelEvents%1000<n)) {
        std::fprintf(logFile,"SCROLL x=%ld y=%ld total_events=%u momentum_events=%u frames=%u rejected=%u\n",x,y,wheelEvents,momentumEvents,frames,rejected); std::fflush(logFile);
    }
}
static void momentumTick() {
    if(!momentum.active()) return;
    auto d=momentum.tick(monotonicUsec()); emitScroll(d,true);
    if(momentum.active() && !armMomentumTimer()) { log("ERROR: momentum timer failed"); PostQuitMessage(16); }
}
static bool getSettings(TOUCHPAD_PARAMETERS_V1& s) {
    s={}; s.versionNumber=TOUCHPAD_PARAMETERS_VERSION_1;
    return SystemParametersInfoW(SPI_GETTOUCHPADPARAMETERS,sizeof(s),&s,0)!=FALSE;
}
static bool setPan(bool enabled) {
    TOUCHPAD_PARAMETERS_V1 s;
    if(!getSettings(s)) return false;
    s.panEnabled=enabled;
    // Live session only; do not persist changes to the user's profile.
    if(!SystemParametersInfoW(SPI_SETTOUCHPADPARAMETERS,sizeof(s),&s,0)) return false;
    if(!getSettings(s)) return false;
    return static_cast<bool>(s.panEnabled)==enabled;
}
static int watchdog(DWORD parent,bool pan,const wchar_t* eventName) {
    HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,eventName);
    std::wstring doneName=std::wstring(eventName)+L"-done";
    HANDLE done=OpenEventW(SYNCHRONIZE,FALSE,doneName.c_str());
    if(!process || !ready || !done) { if(process) CloseHandle(process); if(ready) CloseHandle(ready); if(done) CloseHandle(done); return 8; }
    SetEvent(ready); CloseHandle(ready);
    HANDLE handles[]={done,process};
    DWORD result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
    CloseHandle(process); CloseHandle(done);
    return result==WAIT_OBJECT_0?0:(setPan(pan)?0:9);
}
static bool startWatchdog() {
    wchar_t exe[32768]; if(!GetModuleFileNameW(nullptr,exe,32768)) return false;
    std::wstring eventName=L"Local\\LibinputASCE1200WatchdogReady-"+std::to_wstring(GetCurrentProcessId());
    HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,eventName.c_str());
    if(!ready) return false;
    std::wstring doneName=eventName+L"-done";
    watchdogDone=CreateEventW(nullptr,TRUE,FALSE,doneName.c_str());
    if(!watchdogDone) { CloseHandle(ready); return false; }
    std::wstring cmd=L"\""+std::wstring(exe)+L"\" --watchdog "+std::to_wstring(GetCurrentProcessId())+L" "+(originalPan?L"1":L"0")+L" "+eventName;
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    bool ok=CreateProcessW(exe,cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=FALSE;
    if(ok) {
        CloseHandle(pi.hThread); watchdogProcess=pi.hProcess;
        ok=WaitForSingleObject(ready,4000)==WAIT_OBJECT_0;
        DWORD code=0; ok=ok && GetExitCodeProcess(watchdogProcess,&code) && code==STILL_ACTIVE;
    }
    CloseHandle(ready); return ok;
}
static double axisUnit(const Field& f) {
    if(f.logicalMax<=f.logicalMin || f.physicalMax<=f.physicalMin) return 0;
    int exp=static_cast<int>(f.exponent&15); if(exp>=8) exp-=16;
    double unit=f.units==0x11?10.0:f.units==0x13?25.4:0.0;
    return (f.physicalMax-f.physicalMin)*unit*std::pow(10.0,exp)/(f.logicalMax-f.logicalMin);
}
static void frame(const std::map<USHORT,ULONG>& vals,UCHAR reportID) {
    struct Slot { bool tip=false,confidence=false; ULONG id=0; double x=0,y=0; unsigned present=0; };
    std::map<USHORT,Slot> slots;
    ULONG count=0,scan=0; bool clicked=false,hasCount=false,hasScan=false;
    for(const auto& f:device.fields) {
        if(f.report!=reportID) continue;
        auto it=vals.find(f.index); ULONG v=it==vals.end()?0:it->second;
        if(f.collection==0) {
            if(f.page==0x0D && f.usage==0x54 && it!=vals.end()) { count=v; hasCount=true; }
            if(f.page==0x0D && f.usage==0x56 && it!=vals.end()) { scan=v; hasScan=true; }
            if(f.page==9 && f.usage==1) clicked=v!=0;
        } else {
            auto& s=slots[f.collection];
            if(f.page==0x0D && f.usage==0x42) s.tip=v!=0;
            if(f.page==0x0D && f.usage==0x47) s.confidence=v!=0;
            if(f.page==0x0D && f.usage==0x51 && it!=vals.end()) { s.id=v; s.present|=1; }
            if(f.page==1 && f.usage==0x30 && it!=vals.end()) { s.x=v*xUnit; s.present|=2; }
            if(f.page==1 && f.usage==0x31 && it!=vals.end()) { s.y=v*yUnit; s.present|=4; }
        }
    }
    if(!hasCount || !hasScan || count>5 || slots.size()!=5 || xUnit<=0 || yUnit<=0) { ++rejected; processor.reset(); return; }
    std::vector<Contact> contacts;
    for(const auto& [collection,s]:slots) {
        if(s.tip && s.confidence) {
            if(s.present!=7 || s.x<0 || s.x>126.91 || s.y<0 || s.y>82.01) { ++rejected; processor.reset(); return; }
            contacts.push_back({s.id,s.x,s.y});
        }
    }
    if(contacts.size()>count) { ++rejected; processor.reset(); return; }
    ++frames; if(contacts.size()==2) ++twoFingerFrames;
    if(pointerWorkerMode) { feedPointerContacts(contacts,scan,clicked); return; }
    if(!runMode) return;
    if(!live) {
        // Do not change anything until live, complete two-finger frames arrive.
        if(twoFingerFrames<5) return;
        if(!startWatchdog()) { log("ERROR: recovery watchdog could not start"); PostQuitMessage(10); return; }
        panChanged=true;
        if(!setPan(false)) { log("ERROR: could not disable native panning"); PostQuitMessage(11); return; }
        live=true; processor.reset();
        log("ACTIVE: libinput scrolling; native cursor, zoom and 3/4-finger gestures retained; Ctrl+Alt+F12 restores panning");
    }
    uint64_t wall=GetTickCount64();
    if(!lastReportWall || wall-lastReportWall>100) { processor.reset(); scanEpoch=wall*1000; }
    else {
        ULONG diff=(scan-previousScan)&0xffff;
        if(!diff) return;
        scanEpoch+=static_cast<uint64_t>(diff)*100; // HID scan unit is 100 us.
    }
    lastReportWall=wall; previousScan=scan;
    const auto context=currentScrollContext();
    const bool touchOk=!appProfiles || touchscreenOutput.feed(contacts,clicked,context.foreground,context.target);
    logTouchEnd();
    if(!touchOk) {
        log("ERROR: touchscreen output failed; restoring native scrolling"); PostQuitMessage(20); return;
    }
    if(clicked || (scrollContext.valid() && !scrollContext.matches(context))) cancelMomentum(true);
    if(appProfiles) {
        if(!profileLatched && contacts.size()>=2) {
            auto selected=profileRuntime.forWindow(context.target); profileRouter.begin(selected); profileLatched=true;
            char line[160]; DWORD pid=0; GetWindowThreadProcessId(context.target,&pid);
            std::snprintf(line,sizeof(line),"APP_GESTURE mode=%ls pid=%lu",gestureModeKey(selected.mode),pid); log(line);
        }
        auto routing=profileLatched?profileRouter.feed(contacts,clicked,context.foreground==context.target):HorizontalGestureHandoff::Result{};
        const bool desiredPan=routing.route==HorizontalGestureHandoff::Route::Native;
        if(desiredPan!=handoffPanEnabled) { if(!setPan(desiredPan)) { log("ERROR: cannot switch application gesture gate"); PostQuitMessage(18); return; } handoffPanEnabled=desiredPan; }
        if(routing.lifted) profileLatched=false;
        if(routing.decided) {
            bool native=routing.route==HorizontalGestureHandoff::Route::Native,touch=routing.route==HorizontalGestureHandoff::Route::Touchscreen;
            if(native || touch) { cancelMomentum(true); processor.reset(); processorNeedsReset=false; ++nativeGestures; } else ++customGestures;
            if(touch) {
                auto displacement=profileRouter.lateralDisplacement(); touchscreenOutput.configure(profileRouter.selected().touch);
                if(!touchscreenOutput.begin(context.foreground,context.target,contacts,displacement.x,displacement.y)) { log("ERROR: cannot start configured touchscreen gesture"); PostQuitMessage(20); return; }
            }
            char line[180]; std::snprintf(line,sizeof(line),"APP_HANDOFF route=%s samples=%u",touch?"touchscreen":native?"windows-driver":"libinput",routing.samples); log(line);
        }
        if(routing.route==HorizontalGestureHandoff::Route::Native || routing.route==HorizontalGestureHandoff::Route::Touchscreen) return;
    }
    if(processorNeedsReset) { processor.reset(); processorNeedsReset=false; }
    auto d=processor.feed(contacts,scanEpoch,clicked);
    if(appProfiles && profileLatched && profileRouter.current()==HorizontalGestureHandoff::Route::Pending) d={};
    auto touch=momentum.updateContacts(static_cast<unsigned>(contacts.size()),clicked,d,scanEpoch,monotonicUsec());
    if(contacts.size()>=2) {
        if(inertiaTimer) CancelWaitableTimer(inertiaTimer);
        if(touch.newTwoFingerTouch) wheel.reset();
        if(contacts.size()==2 && !clicked) {
            if(touch.newTwoFingerTouch || !scrollContext.valid()) { scrollContext=context; scrollContextBoundAt=GetTickCount(); }
        } else scrollContext={};
    }
    if(touch.started && (!momentumEnabled || !scrollContext.matches(context))) cancelMomentum(true);
    if(touch.started && momentum.active()) {
        if(!armMomentumTimer()) { log("ERROR: momentum timer failed"); PostQuitMessage(16); }
    }
    if(contacts.size()==2 && !clicked) emitScroll(d,false);
    else if(!momentum.active()) wheel.reset();
}
static bool runtimeMessage(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    (void)wp;
    if(msg==WM_INPUT_DEVICE_CHANGE && runMode) {
        // Registration announces the already-enumerated target as an arrival.
        if(wp==GIDC_ARRIVAL && reinterpret_cast<HANDLE>(lp)==rawDevice) return true;
        log("Device change; stopping to revalidate descriptor"); PostQuitMessage(0); return true;
    }
    if(msg==WM_TIMER) {
        if(appProfiles) updatePointerWorker();
        logLatency();
        if(live && watchdogProcess && WaitForSingleObject(watchdogProcess,0)==WAIT_OBJECT_0) { log("ERROR: watchdog exited; restoring native scrolling"); PostQuitMessage(15); }
        if((deadline && GetTickCount64()>=deadline) || (stopEvent && WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0)) PostQuitMessage(0);
        return true;
    }
    if(msg==WM_SETTINGCHANGE && wp==SPI_SETMOUSEWHEELROUTING) {
        SystemParametersInfoW(SPI_GETMOUSEWHEELROUTING,0,&wheelRouting,0);
        cancelMomentum(true); return true;
    }
    if(msg==WM_HOTKEY || msg==WM_CLOSE) { if(stopEvent) SetEvent(stopEvent); PostQuitMessage(0); return true; }
    if(msg==WM_POWERBROADCAST && wp==PBT_APMSUSPEND) { PostQuitMessage(0); return true; }
    if(msg==WM_APP+1 && lp==WM_LBUTTONDBLCLK) { openSettingsWindow(); return true; }
    if(msg==WM_APP+1 && lp==WM_RBUTTONUP) {
        HMENU menu=CreatePopupMenu(); AppendMenuW(menu,MF_STRING,1,L"Settings…"); AppendMenuW(menu,MF_SEPARATOR,0,nullptr); AppendMenuW(menu,MF_STRING,2,L"Stop and restore Windows scrolling");
        POINT pt{}; GetCursorPos(&pt); SetForegroundWindow(h);
        int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,h,nullptr);
        DestroyMenu(menu); if(command==1) openSettingsWindow(); else if(command==2) { if(stopEvent) SetEvent(stopEvent); PostQuitMessage(0); } return true;
    }
    return false;
}
