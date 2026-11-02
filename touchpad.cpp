// Native ARM64 Raw Input adapter for the ASUS ASCE1200 Precision Touchpad.
// Port of the libinput scroll components, using the existing HID transport.
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct Field { USHORT index, page, usage, collection; LONG logicalMin, logicalMax, physicalMin, physicalMax; ULONG units, exponent; UCHAR report; bool button; };
struct Device {
    std::wstring path;
    PHIDP_PREPARSED_DATA pp = nullptr;
    HIDP_CAPS caps{};
    HIDD_ATTRIBUTES attrs{};
    std::vector<Field> fields;
    ~Device() { if (pp) HidD_FreePreparsedData(pp); }
};
static Device device;
static FILE* logFile;
static unsigned reports, parsed, nullHandles, otherHandles, badReports;
static HANDLE rawDevice;
static HWND window;
static unsigned duration = 30;

static void log(const char* text) { std::puts(text); if (logFile) { std::fprintf(logFile,"%s\n",text); std::fflush(logFile); } }
#include "windows_scroll.h"
#include "settings_window.h"
static void openSettingsWindow() {
    wchar_t exe[32768]; if(!GetModuleFileNameW(nullptr,exe,32768)) return;
    std::wstring command=L"\""+std::wstring(exe)+L"\" --settings";
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_SHOWNORMAL; PROCESS_INFORMATION child{};
    if(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)) { CloseHandle(child.hThread); CloseHandle(child.hProcess); }
}
static void inspect() {
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO info = SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) { log("SetupDiGetClassDevs failed"); return; }
    for (DWORD i=0;;++i) {
        SP_DEVICE_INTERFACE_DATA it{}; it.cbSize=sizeof(it);
        if (!SetupDiEnumDeviceInterfaces(info,nullptr,&guid,i,&it)) break;
        DWORD bytes=0; SetupDiGetDeviceInterfaceDetailW(info,&it,nullptr,0,&bytes,nullptr);
        std::vector<BYTE> buf(bytes);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize=sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(info,&it,detail,bytes,nullptr,nullptr)) continue;
        std::wstring path=detail->DevicePath, upper=path;
        std::transform(upper.begin(),upper.end(),upper.begin(),towupper);
        HANDLE h=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if (h==INVALID_HANDLE_VALUE) continue;
        PHIDP_PREPARSED_DATA pp=nullptr;
        HIDP_CAPS caps{};
        if (!HidD_GetPreparsedData(h,&pp) || HidP_GetCaps(pp,&caps)!=HIDP_STATUS_SUCCESS || caps.UsagePage!=0x0D || caps.Usage!=0x05) {
            if(pp) HidD_FreePreparsedData(pp); CloseHandle(h); continue;
        }
        if (upper.find(L"ASCE1200") == std::wstring::npos || device.pp) {
            ambiguous=true; log("ERROR: another touchpad present; global panning replacement is ambiguous");
            HidD_FreePreparsedData(pp); CloseHandle(h); continue;
        }
        device.pp=pp; device.path=path; device.caps=caps;
        device.attrs.Size=sizeof(device.attrs); HidD_GetAttributes(h,&device.attrs);
        char line[512];
        std::snprintf(line,sizeof(line),"DEVICE vid=%04x pid=%04x input_bytes=%u value_caps=%u button_caps=%u",device.attrs.VendorID,device.attrs.ProductID,caps.InputReportByteLength,caps.NumberInputValueCaps,caps.NumberInputButtonCaps); log(line);
        USHORT n=caps.NumberInputValueCaps;
        std::vector<HIDP_VALUE_CAPS> values(n);
        if (HidP_GetValueCaps(HidP_Input,values.data(),&n,pp)==HIDP_STATUS_SUCCESS) {
            for (const auto& v:values) {
                USHORT first=v.IsRange?v.Range.UsageMin:v.NotRange.Usage;
                USHORT last=v.IsRange?v.Range.UsageMax:first;
                USHORT idx=v.IsRange?v.Range.DataIndexMin:v.NotRange.DataIndex;
                for(unsigned u=first;u<=last;++u) device.fields.push_back({static_cast<USHORT>(idx+u-first),v.UsagePage,static_cast<USHORT>(u),v.LinkCollection,v.LogicalMin,v.LogicalMax,v.PhysicalMin,v.PhysicalMax,v.Units,v.UnitsExp,v.ReportID,false});
            }
        }
        n=caps.NumberInputButtonCaps;
        std::vector<HIDP_BUTTON_CAPS> buttons(n);
        if(n && HidP_GetButtonCaps(HidP_Input,buttons.data(),&n,pp)==HIDP_STATUS_SUCCESS) {
            for(const auto& v:buttons) {
                USHORT first=v.IsRange?v.Range.UsageMin:v.NotRange.Usage;
                USHORT last=v.IsRange?v.Range.UsageMax:first;
                USHORT idx=v.IsRange?v.Range.DataIndexMin:v.NotRange.DataIndex;
                for(unsigned u=first;u<=last;++u) device.fields.push_back({static_cast<USHORT>(idx+u-first),v.UsagePage,static_cast<USHORT>(u),v.LinkCollection,0,1,0,0,0,0,v.ReportID,true});
            }
        }
        for(const auto& f:device.fields) {
            if(f.collection==1 && f.page==1 && f.usage==0x30) xUnit=axisUnit(f);
            if(f.collection==1 && f.page==1 && f.usage==0x31) yUnit=axisUnit(f);
            std::snprintf(line,sizeof(line),"FIELD report=%u index=%u collection=%u page=%04x usage=%04x button=%d logical=%ld..%ld physical=%ld..%ld units=%lx exponent=%lx",f.report,f.index,f.collection,f.page,f.usage,f.button,f.logicalMin,f.logicalMax,f.physicalMin,f.physicalMax,f.units,f.exponent); log(line);
        }
        CloseHandle(h);
    }
    SetupDiDestroyDeviceInfoList(info);
    UINT count=0; GetRawInputDeviceList(nullptr,&count,sizeof(RAWINPUTDEVICELIST));
    std::vector<RAWINPUTDEVICELIST> list(count);
    if(GetRawInputDeviceList(list.data(),&count,sizeof(RAWINPUTDEVICELIST))!=UINT(-1)) {
        for(const auto& item:list) {
            RID_DEVICE_INFO ri{}; ri.cbSize=sizeof(ri); UINT size=sizeof(ri);
            if(item.dwType!=RIM_TYPEHID || GetRawInputDeviceInfoW(item.hDevice,RIDI_DEVICEINFO,&ri,&size)==UINT(-1)) continue;
            if(ri.hid.usUsagePage==0x0D && ri.hid.usUsage==0x05) {
                UINT chars=0; GetRawInputDeviceInfoW(item.hDevice,RIDI_DEVICENAME,nullptr,&chars);
                std::wstring name(chars,L'\0');
                GetRawInputDeviceInfoW(item.hDevice,RIDI_DEVICENAME,name.data(),&chars);
                std::transform(name.begin(),name.end(),name.begin(),towupper);
                if(name.find(L"ASCE1200")!=std::wstring::npos) rawDevice=item.hDevice;
            }
        }
    }
    TOUCHPAD_PARAMETERS_V1 settings{}; settings.versionNumber=TOUCHPAD_PARAMETERS_VERSION_1;
    char line[256];
    if(SystemParametersInfoW(SPI_GETTOUCHPADPARAMETERS,sizeof(settings),&settings,0)) {
        std::snprintf(line,sizeof(line),"SETTINGS present=%d active=%d contacts=%u pan=%d zoom=%d reversed=%d",settings.touchpadPresent,settings.touchpadActive,settings.maxSupportedContacts,settings.panEnabled,settings.zoomEnabled,settings.scrollDirectionReversed); log(line);
    } else { std::snprintf(line,sizeof(line),"SETTINGS query failed error=%lu",GetLastError()); log(line); }
}

static void parse(BYTE* report, ULONG length, HANDLE source) {
    if(!device.pp || length!=device.caps.InputReportByteLength) { ++badReports; return; }
    ULONG n=HidP_MaxDataListLength(HidP_Input,device.pp);
    std::vector<HIDP_DATA> data(n);
    NTSTATUS status=HidP_GetData(HidP_Input,data.data(),&n,device.pp,reinterpret_cast<PCHAR>(report),length);
    if(status!=HIDP_STATUS_SUCCESS) { ++badReports; return; }
    ++parsed;
    std::map<USHORT,ULONG> vals;
    for(ULONG i=0;i<n;++i) {
        bool button=false;
        for(const auto& f:device.fields) if(f.index==data[i].DataIndex && f.report==report[0]) { button=f.button; break; }
        vals[data[i].DataIndex]=button?static_cast<ULONG>(data[i].On!=FALSE):data[i].RawValue;
    }
    frame(vals,report[0]);
    if(runMode || pointerWorkerMode) return;
    char line[2048];
    int pos=std::snprintf(line,sizeof(line),"REPORT t=%llu source=%p id=%u",GetTickCount64(),source,report[0]);
    for(const auto& f:device.fields) {
        if(f.report!=report[0]) continue;
        if(f.page==0x0D || f.page==0x01) {
            ULONG v=vals.count(f.index)?vals[f.index]:0;
            pos+=std::snprintf(line+pos,sizeof(line)-pos," c%u:%02x/%02x=%lu",f.collection,f.page,f.usage,v);
            if(pos>1800) break;
        }
    }
    if(logFile) { std::fprintf(logFile,"%s\n",line); if(parsed%20==0) std::fflush(logFile); }
}
static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_INPUT) {
        RawInputLatencyScope timing;
        UINT bytes=0;
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,nullptr,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1)) return DefWindowProcW(h,msg,wp,lp);
        alignas(RAWINPUT) BYTE local[256];
        std::vector<BYTE> overflow;
        if(bytes>sizeof(local)) overflow.resize(bytes);
        BYTE* buf=overflow.empty()?local:overflow.data();
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,buf,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1)) return DefWindowProcW(h,msg,wp,lp);
        auto* raw=reinterpret_cast<RAWINPUT*>(buf);
        if(raw->header.dwType==RIM_TYPEMOUSE && runMode) {
            observeMouseButtons(raw->data.mouse.usButtonFlags,raw->data.mouse.ulExtraInformation,raw->header.hDevice);
        } else if(raw->header.dwType==RIM_TYPEHID) {
            ++reports;
            if(!raw->header.hDevice) ++nullHandles;
            else if(raw->header.hDevice!=rawDevice) { ++otherHandles; return DefWindowProcW(h,msg,wp,lp); }
            size_t off=offsetof(RAWINPUT,data.hid.bRawData);
            size_t required=static_cast<size_t>(raw->data.hid.dwCount)*raw->data.hid.dwSizeHid;
            if(bytes>=off && required<=bytes-off) {
                for(DWORD i=0;i<raw->data.hid.dwCount;++i) parse(raw->data.hid.bRawData+i*raw->data.hid.dwSizeHid,raw->data.hid.dwSizeHid,raw->header.hDevice);
            } else ++badReports;
        }
        return DefWindowProcW(h,msg,wp,lp);
    }
    if(runtimeMessage(h,msg,wp,lp)) return 0;
    return DefWindowProcW(h,msg,wp,lp);
}
#include "pointer_worker.h"
int wmain(int argc,wchar_t** argv) {
    // Resolve cursor positions and application windows in one physical DPI space.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(argc>=2 && std::wcscmp(argv[1],L"--pointer-preview")==0) {
        auto options=loadGestureSettings(appDirectory()+L"\\gestures.ini").pointer;
        options.enabled=true;
        for(int i=3;i<argc;++i) { if(std::wcscmp(argv[i],L"--mouse-dpi")==0 && i+1<argc) { options.mouseAdaptive=true; options.mouseDpi=std::clamp(static_cast<unsigned>(_wtoi(argv[++i])),100u,64000u); } else if(std::wcscmp(argv[i],L"--speed")==0 && i+1<argc) options.touchpadSpeed=std::clamp(_wtof(argv[++i]),-1.0,1.0); }
        return PointerBackend::run(options,argc>=3?std::clamp(static_cast<unsigned>(_wtoi(argv[2])),15u,600u):120);
    }
    if(argc>=3 && std::wcscmp(argv[1],L"--pointer-worker")==0) return PointerBackend::run(loadGestureSettings(appDirectory()+L"\\gestures.ini").pointer,0,static_cast<DWORD>(_wtol(argv[2])));
    if(argc>=2 && std::wcscmp(argv[1],L"--stop-pointer")==0) { HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,PointerBackend::stopName); if(event) { SetEvent(event); CloseHandle(event); } return 0; }
    if(argc>=2 && std::wcscmp(argv[1],L"--check-icon")==0) {
        for(int size: {16,20,24,32,48,64,128,256}) {
            HICON icon=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101),IMAGE_ICON,size,size,0));
            if(!icon) return 1;
            DestroyIcon(icon);
        }
        std::puts("ICON verified: 16 20 24 32 48 64 128 256"); return 0;
    }
    if(argc>=5 && std::wcscmp(argv[1],L"--watchdog")==0) return watchdog(static_cast<DWORD>(_wtol(argv[2])),_wtoi(argv[3])!=0,argv[4]);
    if(argc>=4 && std::wcscmp(argv[1],L"--settings-self-test")==0) return SettingsUi::run(argv[2],true,argv[3]);
    if(argc==1 || (argc>=2 && std::wcscmp(argv[1],L"--settings")==0)) return SettingsUi::run(appDirectory()+L"\\gestures.ini");
    if(argc>=2 && std::wcscmp(argv[1],L"--supervise")==0) return supervise();
    if(argc>=2 && std::wcscmp(argv[1],L"--status")==0) {
        TOUCHPAD_PARAMETERS_V1 s;
        if(!getSettings(s)) return 1;
        std::printf("pan=%d zoom=%d active=%d reversed=%d\n",s.panEnabled!=0,s.zoomEnabled!=0,s.touchpadActive!=0,s.scrollDirectionReversed!=0); return 0;
    }
    if(argc>=2 && std::wcscmp(argv[1],L"--test-recovery")==0) {
        TOUCHPAD_PARAMETERS_V1 s;
        if(!getSettings(s)) return 1;
        originalPan=s.panEnabled!=0;
        if(!startWatchdog()) return 2;
        if(!setPan(false)) { setPan(originalPan); SetEvent(watchdogDone); return 3; }
        // Deliberately skip normal cleanup: the external watchdog must restore.
        ExitProcess(77);
    }
    if(argc>=2 && std::wcscmp(argv[1],L"--stop")==0) {
        HANDLE pointerStop=OpenEventW(EVENT_MODIFY_STATE,FALSE,PointerBackend::stopName); if(pointerStop) { SetEvent(pointerStop); CloseHandle(pointerStop); }
        HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,stopName);
        if(!e) return GetLastError()==ERROR_FILE_NOT_FOUND?0:6;
        bool stopped=SetEvent(e)!=FALSE; CloseHandle(e); return stopped?0:6;
    }
    loadConfiguration();
    auto defaultLog=appDirectory()+L"\\probe.log";
    const wchar_t* out=defaultLog.c_str();
    for(int i=1;i<argc;++i) {
        if(std::wcscmp(argv[i],L"--seconds")==0 && i+1<argc) duration=std::clamp(static_cast<unsigned>(_wtoi(argv[++i])),1u,300u);
        else if(std::wcscmp(argv[i],L"--log")==0 && i+1<argc) out=argv[++i];
        else if(std::wcscmp(argv[i],L"--run")==0) { runMode=true; duration=0; }
        else if(std::wcscmp(argv[i],L"--scale")==0 && i+1<argc) wheelScale=std::clamp(_wtof(argv[++i]),0.1,10.0);
        else if(std::wcscmp(argv[i],L"--native-direction")==0) invertDirection=false;
        else if(std::wcscmp(argv[i],L"--no-inertia")==0) momentumEnabled=false;
        else if(std::wcscmp(argv[i],L"--friction")==0 && i+1<argc) momentum.setFriction(_wtof(argv[++i]));
        else if(std::wcscmp(argv[i],L"--profile")==0) collectLatency=true;
        else if(std::wcscmp(argv[i],L"--app-profiles")==0) appProfiles=true;
        else if(std::wcscmp(argv[i],L"--test-seconds")==0 && i+1<argc) duration=std::clamp(static_cast<unsigned>(_wtoi(argv[++i])),30u,1800u);
    }
    logFile=_wfsopen(out,L"w",_SH_DENYNO);
    log(runMode?"ASCE1200 libinput scroll component port - native Windows ARM64":"ASCE1200 Raw Input probe - no settings changed, no input injected");
    if(runMode) {
        char config[400]; std::snprintf(config,sizeof(config),"CONFIG scale=%.3f reverse_native=%d inertia=%d friction=%.5f pid=%lu momentum_cancel=click,two_finger_touch,window_change profile=%d elevated=%d app_profiles=%d dpi_awareness=%d",wheelScale,invertDirection,momentumEnabled,momentum.getFriction(),GetCurrentProcessId(),collectLatency,processIsElevated(),appProfiles,GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())); log(config);
    }
    if(runMode) {
        instanceMutex=CreateMutexW(nullptr,TRUE,L"Local\\LibinputASCE1200ScrollInstance");
        if(!instanceMutex || GetLastError()==ERROR_ALREADY_EXISTS) { log("ERROR: another instance is running"); return 5; }
        stopEvent=createUserStopEvent(stopName); if(!stopEvent) return 6;
    }
    inspect();
    if(!device.pp || ambiguous) { log("ERROR: target touchpad missing or ambiguous"); if(logFile) std::fclose(logFile); return 2; }
    if(runMode) {
        TOUCHPAD_PARAMETERS_V1 settings;
        if(!getSettings(settings) || !settings.touchpadActive || xUnit<=0 || yUnit<=0 || device.attrs.VendorID!=0x04F3 || device.attrs.ProductID!=0x32E2 || device.caps.InputReportByteLength!=42) { log("ERROR: unvalidated device or unsupported settings API"); return 7; }
        originalPan=settings.panEnabled!=0; reversed=settings.scrollDirectionReversed!=0;
    }
    WNDCLASSW wc{}; wc.lpfnWndProc=wndproc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"LibinputArm64TouchpadProbe";
    wc.hIcon=static_cast<HICON>(LoadImageW(wc.hInstance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));
    RegisterClassW(&wc);
    window=CreateWindowExW(0,wc.lpszClassName,L"libinput scrolling",0,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    RAWINPUTDEVICE rid[]={{0x0D,0x05,RIDEV_INPUTSINK|RIDEV_DEVNOTIFY,window},{0x01,0x02,RIDEV_INPUTSINK,window}};
    if(!window || !RegisterRawInputDevices(rid,runMode?2:1,sizeof(RAWINPUTDEVICE))) { log("ERROR: raw input registration failed"); return 3; }
    if(duration) deadline=GetTickCount64()+duration*1000;
    if(runMode) {
        inertiaTimer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if(!inertiaTimer) { log("ERROR: high resolution timer unavailable"); return 16; }
        if(!RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F12)) { log("ERROR: emergency hotkey unavailable"); return 13; }
        if(!startInteractionMonitoring()) { log("ERROR: focus monitoring unavailable; native scrolling retained"); return 17; }
        tray.cbSize=sizeof(tray); tray.hWnd=window; tray.uID=1; tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
        tray.uCallbackMessage=WM_APP+1;
        tray.hIcon=static_cast<HICON>(LoadImageW(wc.hInstance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
        if(!tray.hIcon) tray.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcscpy_s(tray.szTip,L"libinput — double-click: settings; Ctrl+Alt+F12: stop"); Shell_NotifyIconW(NIM_ADD,&tray);
    }
    if(!SetTimer(window,1,100,nullptr)) { log("ERROR: recovery timer unavailable"); return 14; }
    if(runMode && appProfiles) {
        if(!profileRuntime.start(appDirectory()+L"\\gestures.ini")) { log("ERROR: application profile watcher could not start"); PostQuitMessage(21); }
        else if(!startWatchdog()) { log("ERROR: recovery watchdog could not start"); PostQuitMessage(10); }
        else {
            panChanged=true;
            if(!setPan(false)) { log("ERROR: could not initialize application gesture gate"); PostQuitMessage(11); }
            else { handoffPanEnabled=false; live=true; log("ACTIVE: per-application gesture profiles; settings reload between gestures; no UI Automation queries"); }
        }
    }
    log("READY: move one finger, then scroll with two fingers, then lift all fingers");
    MSG msg{}; int rc;
    if(runMode) {
        bool quit=false; rc=1; HANDLE handles[]={inertiaTimer,stopEvent};
        while(!quit) {
            DWORD ready=MsgWaitForMultipleObjectsEx(2,handles,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(ready==WAIT_OBJECT_0+1) PostQuitMessage(0);
            if(ready==WAIT_FAILED) { rc=-1; break; }
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                if(msg.message==WM_QUIT) { quit=true; break; }
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            // Deliver click, focus and new-touch notifications before a due
            // inertia tick when both become ready at the same time.
            if(!quit && ready==WAIT_OBJECT_0) momentumTick();
        }
        stopPointerWorker(); stopInteractionMonitoring(); touchscreenOutput.close(); logTouchEnd(); cancelMomentum(true); CloseHandle(inertiaTimer); inertiaTimer=nullptr;
    } else while((rc=GetMessageW(&msg,nullptr,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    logLatency(true);
    if(runMode && duration) {
        char ending[160]; std::snprintf(ending,sizeof(ending),"TEST_END deadline_reached=%d stop_requested=%d",deadline && GetTickCount64()>=deadline,stopEvent && WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0); log(ending);
    }
    if(panChanged) {
        if(setPan(originalPan)) {
            log("RESTORED: original Windows panning");
            if(watchdogDone) SetEvent(watchdogDone);
            if(watchdogProcess) WaitForSingleObject(watchdogProcess,2000);
        } else log("ERROR: restore failed; watchdog will retry");
    }
    profileRuntime.stop();
    if(runMode) { Shell_NotifyIconW(NIM_DELETE,&tray); UnregisterHotKey(window,1); }
    char line[600]; std::snprintf(line,sizeof(line),"SUMMARY reports=%u parsed=%u null_handles=%u other_devices=%u invalid_reports=%u frames=%u two_finger=%u rejected=%u wheel_events=%u momentum_events=%u click_stops=%u context_stops=%u native_gestures=%u custom_gestures=%u touch_mouse_echoes=%u",reports,parsed,nullHandles,otherHandles,badReports,frames,twoFingerFrames,rejected,wheelEvents,momentumEvents,clickStops,contextStops,nativeGestures,customGestures,touchMouseEchoes); log(line);
    DestroyWindow(window);
    if(logFile) std::fclose(logFile);
    if(stopEvent) CloseHandle(stopEvent);
    if(instanceMutex) { ReleaseMutex(instanceMutex); CloseHandle(instanceMutex); }
    return rc<0?4:static_cast<int>(msg.wParam);
}
