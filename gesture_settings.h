#pragma once
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <cwctype>
#include "windows_access.h"
#include "touchscreen_handoff.h"
#include "touchscreen_output.h"

enum class GestureMode { None,WindowsV1,Touchscreen };
inline const wchar_t* gestureModeKey(GestureMode mode) { return mode==GestureMode::WindowsV1?L"v1":mode==GestureMode::Touchscreen?L"touchscreen":L"none"; }
inline const wchar_t* gestureModeLabel(GestureMode mode) { return mode==GestureMode::WindowsV1?L"Windows driver":mode==GestureMode::Touchscreen?L"Touchscreen":L"No gestures"; }
inline GestureMode parseGestureMode(const std::wstring& text) { return _wcsicmp(text.c_str(),L"v1")==0?GestureMode::WindowsV1:_wcsicmp(text.c_str(),L"touchscreen")==0?GestureMode::Touchscreen:GestureMode::None; }
struct GestureProfile { GestureMode mode=GestureMode::None; TouchscreenOptions touch; };
struct ApplicationRule { std::wstring image; GestureProfile profile; };
struct GestureSettings { GestureProfile defaults{GestureMode::WindowsV1,{}}; std::vector<ApplicationRule> applications; };
inline std::wstring executableBase(const std::wstring& path) { auto slash=path.find_last_of(L"\\/"); return path.substr(slash==std::wstring::npos?0:slash+1); }
inline bool sameImage(const std::wstring& a,const std::wstring& b) { return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL; }
inline std::wstring imageForWindow(HWND window) {
    DWORD pid=0; GetWindowThreadProcessId(window,&pid); if(!pid) return {};
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid); if(!process) return {};
    wchar_t path[32768]; DWORD length=32768;
    bool ok=QueryFullProcessImageNameW(process,0,path,&length)!=FALSE; CloseHandle(process);
    return ok?std::wstring(path,length):std::wstring{};
}
inline GestureProfile profileForImage(const GestureSettings& settings,const std::wstring& image) {
    if(image.empty()) return settings.defaults;
    // Exact paths take precedence over portable basename rules.
    for(const auto& app:settings.applications) if(app.image.find_first_of(L"\\/")!=std::wstring::npos && sameImage(app.image,image)) return app.profile;
    auto base=executableBase(image);
    for(const auto& app:settings.applications) if(app.image.find_first_of(L"\\/")==std::wstring::npos && sameImage(app.image,base)) return app.profile;
    return settings.defaults;
}
inline std::wstring iniText(const std::wstring& path,const wchar_t* section,const wchar_t* key,const wchar_t* fallback) {
    wchar_t value[32768]; GetPrivateProfileStringW(section,key,fallback,value,32768,path.c_str()); return value;
}
inline GestureProfile readGestureProfile(const std::wstring& path,const wchar_t* section,GestureMode fallback=GestureMode::None) {
    GestureProfile profile; profile.mode=parseGestureMode(iniText(path,section,L"mode",gestureModeKey(fallback)));
    double gain=_wtof(iniText(path,section,L"touch_pixels_per_mm",L"10").c_str());
    profile.touch.pixelsPerMm=std::clamp(std::isfinite(gain)?gain:10.0,1.0,40.0);
    profile.touch.invertHorizontal=GetPrivateProfileIntW(section,L"touch_invert_x",0,path.c_str())!=0;
    profile.touch.lockVertical=GetPrivateProfileIntW(section,L"touch_lock_y",0,path.c_str())!=0;
    return profile;
}
inline GestureSettings loadGestureSettings(const std::wstring& path) {
    GestureSettings settings; settings.defaults=readGestureProfile(path,L"default",GestureMode::WindowsV1);
    unsigned count=std::min(GetPrivateProfileIntW(L"applications",L"count",0,path.c_str()),128u);
    for(unsigned i=0;i<count;++i) {
        auto section=L"app."+std::to_wstring(i); auto image=iniText(path,section.c_str(),L"image",L"");
        if(!image.empty()) settings.applications.push_back({image,readGestureProfile(path,section.c_str())});
    }
    return settings;
}
inline std::wstring settingsChangedEventName(const std::wstring& path) {
    uint64_t hash=1469598103934665603ull; for(wchar_t c:path) { hash^=static_cast<uint64_t>(towlower(c)); hash*=1099511628211ull; }
    return L"Local\\LibinputGestureSettings-"+std::to_wstring(hash);
}
inline void notifyGestureSettingsChanged(const std::wstring& path) {
    HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,settingsChangedEventName(path).c_str()); if(event) { SetEvent(event); CloseHandle(event); }
}
inline bool saveGestureSettings(const std::wstring& path,const GestureSettings& settings,DWORD* failure=nullptr) {
    std::wstring text=L"; Gesture profiles. Scroll speed and inertia remain in settings.ini.\r\n";
    auto append=[&](const std::wstring& section,const GestureProfile& profile) {
        wchar_t gain[50]; swprintf_s(gain,L"%.3f",profile.touch.pixelsPerMm);
        text+=L"["+section+L"]\r\nmode="+gestureModeKey(profile.mode)+L"\r\ntouch_pixels_per_mm="+gain+
            L"\r\ntouch_invert_x="+(profile.touch.invertHorizontal?L"1":L"0")+L"\r\ntouch_lock_y="+(profile.touch.lockVertical?L"1":L"0")+L"\r\n";
    };
    append(L"default",settings.defaults);
    text+=L"\r\n[applications]\r\ncount="+std::to_wstring(settings.applications.size())+L"\r\n";
    for(size_t i=0;i<settings.applications.size();++i) {
        const auto& app=settings.applications[i]; auto section=L"app."+std::to_wstring(i);
        append(section,app.profile); text+=L"image="+app.image+L"\r\n\r\n";
    }
    text.insert(text.begin(),wchar_t(0xfeff));
    auto temporary=path+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(file==INVALID_HANDLE_VALUE) { if(failure) *failure=GetLastError(); return false; }
    DWORD written=0,bytes=static_cast<DWORD>(text.size()*sizeof(wchar_t));
    bool ok=WriteFile(file,text.data(),bytes,&written,nullptr)!=FALSE && written==bytes && FlushFileBuffers(file);
    DWORD error=ok?0:GetLastError(); CloseHandle(file);
    if(ok && !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { ok=false; error=GetLastError(); }
    if(!ok) DeleteFileW(temporary.c_str());
    else { WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str()); notifyGestureSettingsChanged(path); }
    if(failure) *failure=error; return ok;
}

class GestureProfileRuntime {
    struct Resolved { HWND target=nullptr; DWORD pid=0; std::wstring image; std::shared_ptr<const GestureSettings> config; GestureProfile profile; };
    std::atomic<std::shared_ptr<const GestureSettings>> current;
    std::atomic<std::shared_ptr<const Resolved>> resolved;
    std::thread worker;
    HANDLE quit=nullptr,changed=nullptr;
    std::wstring path;
    static uint64_t modified(const std::wstring& file) {
        WIN32_FILE_ATTRIBUTE_DATA data{}; if(!GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&data)) return 0;
        return uint64_t(data.ftLastWriteTime.dwHighDateTime)<<32|data.ftLastWriteTime.dwLowDateTime;
    }
    void run() {
        uint64_t stamp=0; HWND previous=nullptr; DWORD previousPid=0; std::wstring image;
        for(;;) {
            HANDLE events[]{quit,changed}; auto ready=WaitForMultipleObjects(2,events,FALSE,100);
            if(ready==WAIT_OBJECT_0 || ready==WAIT_FAILED) break;
            if(ready==WAIT_OBJECT_0+1) ResetEvent(changed);
            auto next=modified(path);
            if(next!=stamp) { current.store(std::make_shared<const GestureSettings>(loadGestureSettings(path))); stamp=next; }
            POINT point{}; GetPhysicalCursorPos(&point); HWND target=GetForegroundWindow();
            DWORD routing=MOUSEWHEEL_ROUTING_MOUSE_POS; SystemParametersInfoW(SPI_GETMOUSEWHEELROUTING,0,&routing,0);
            if(routing!=MOUSEWHEEL_ROUTING_FOCUS) { HWND hit=WindowFromPhysicalPoint(point); target=hit?GetAncestor(hit,GA_ROOT):nullptr; }
            DWORD pid=0; GetWindowThreadProcessId(target,&pid);
            if(target!=previous || pid!=previousPid) { image=imageForWindow(target); previous=target; previousPid=pid; }
            auto config=current.load(); auto old=resolved.load();
            if(!old || target!=old->target || pid!=old->pid || config!=old->config) {
                auto value=std::make_shared<Resolved>(); value->target=target; value->pid=pid; value->image=image; value->config=config; value->profile=profileForImage(*config,image);
                resolved.store(std::shared_ptr<const Resolved>(value));
            }
        }
    }
public:
    ~GestureProfileRuntime() { stop(); }
    bool start(const std::wstring& file) {
        path=file; current.store(std::make_shared<const GestureSettings>(loadGestureSettings(path)));
        quit=CreateEventW(nullptr,TRUE,FALSE,nullptr); changed=createUserStopEvent(settingsChangedEventName(path).c_str());
        if(!quit || !changed) { stop(); return false; }
        try { worker=std::thread([this] { run(); }); } catch(...) { stop(); return false; }
        return true;
    }
    void notifyContext() { if(changed) SetEvent(changed); }
    void stop() {
        if(quit) SetEvent(quit); if(worker.joinable()) worker.join();
        if(quit) { CloseHandle(quit); quit=nullptr; } if(changed) { CloseHandle(changed); changed=nullptr; }
    }
    GestureProfile forWindow(HWND target,std::wstring* name=nullptr) const {
        auto config=current.load(); auto snapshot=resolved.load();
        if(!config) return {};
        DWORD pid=0; GetWindowThreadProcessId(target,&pid);
        if(snapshot && snapshot->target==target && snapshot->pid==pid && snapshot->config==config) { if(name) *name=executableBase(snapshot->image); return snapshot->profile; }
        // A new window may precede the background cache. This bounded kernel
        // query occurs once at gesture start; no provider calls or file reads.
        auto image=imageForWindow(target); if(name) *name=executableBase(image); return profileForImage(*config,image);
    }
};

class ProfileGestureRouter {
    GestureProfile profile;
    HorizontalGestureHandoff native;
    TouchscreenGestureHandoff touch;
    bool active=false;
public:
    using Route=HorizontalGestureHandoff::Route; using Result=HorizontalGestureHandoff::Result;
    void begin(GestureProfile value) { profile=value; native.reset(); touch.reset(); active=false; }
    GestureProfile selected() const { return profile; }
    Route current() const { return profile.mode==GestureMode::WindowsV1?native.current():profile.mode==GestureMode::Touchscreen?touch.current():active?Route::Custom:Route::Idle; }
    normalized_coords lateralDisplacement() const { return touch.lateralDisplacement(); }
    void noteCustomOutput() { if(profile.mode==GestureMode::Touchscreen) touch.noteCustomOutput(); }
    Result feed(const std::vector<Contact>& contacts,bool clicked=false,bool foregroundTarget=true) {
        if(profile.mode==GestureMode::WindowsV1) return native.feed(contacts,clicked);
        if(profile.mode==GestureMode::Touchscreen) return touch.feed(contacts,clicked,foregroundTarget);
        if(contacts.empty()) { bool lifted=active; active=false; return {Route::Idle,false,lifted}; }
        active=true; return {Route::Custom};
    }
};
