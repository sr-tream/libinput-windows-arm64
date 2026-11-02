#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace PointerBackend { static LRESULT CALLBACK hook(int,WPARAM,LPARAM); }
static POINT cursor{50,100};
static std::vector<INPUT> outputs;
static bool acknowledgeOutput=true,sendSucceeds=true;
static LRESULT WINAPI fakeNext(HHOOK,int,WPARAM,LPARAM) { return 99; }
static BOOL WINAPI fakeCursor(LPPOINT value) { *value=cursor; return TRUE; }
static int WINAPI fakeMetric(int value) {
    if(value==SM_XVIRTUALSCREEN) return -1920; if(value==SM_YVIRTUALSCREEN) return -100;
    if(value==SM_CXVIRTUALSCREEN) return 4800; if(value==SM_CYVIRTUALSCREEN) return 1800; return 0;
}
static UINT WINAPI fakeSend(UINT count,LPINPUT input,int) {
    if(!sendSucceeds) return 0;
    for(UINT i=0;i<count;++i) {
        outputs.push_back(input[i]);
        MSLLHOOKSTRUCT notification{}; notification.flags=LLMHF_INJECTED; notification.dwExtraInfo=input[i].mi.dwExtraInfo;
        if(acknowledgeOutput) PointerBackend::hook(HC_ACTION,WM_MOUSEMOVE,reinterpret_cast<LPARAM>(&notification));
        if(input[i].mi.dwFlags&MOUSEEVENTF_ABSOLUTE) cursor={-1920+input[i].mi.dx*4800/65536,-100+input[i].mi.dy*1800/65536};
    }
    return count;
}
#define CallNextHookEx fakeNext
#define GetPhysicalCursorPos fakeCursor
#define GetSystemMetrics fakeMetric
#define SendInput fakeSend
#define wmain libinput_application_wmain
#include "touchpad.cpp"
#undef wmain
#undef SendInput
#undef GetSystemMetrics
#undef GetPhysicalCursorPos
#undef CallNextHookEx
static unsigned checks=0;
static void check(bool value,const char* name) { ++checks; if(!value) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); } }
int main() {
    using namespace PointerBackend;
    MSLLHOOKSTRUCT event{}; auto input=reinterpret_cast<LPARAM>(&event);
    check(hook(HC_ACTION,WM_MOUSEMOVE,input)==99,"disabled backend leaves native movement alone");
    gate=true; pulse=GetTickCount64();
    check(hook(HC_ACTION,WM_MOUSEMOVE,input)==1,"active backend suppresses unmarked hardware movement");
    check(hook(HC_ACTION,WM_LBUTTONDOWN,input)==99 && hook(HC_ACTION,WM_MOUSEWHEEL,input)==99,"buttons and wheels stay native");
    event.dwExtraInfo=0xff515780;
    check(hook(HC_ACTION,WM_MOUSEMOVE,input)==99,"touchscreen and pen promotion stays native");
    event.dwExtraInfo=0; event.flags=LLMHF_INJECTED;
    check(hook(HC_ACTION,WM_MOUSEMOVE,input)==99,"other applications' injection is not reprocessed");
    event.dwExtraInfo=outputTag;
    check(hook(HC_ACTION,WM_MOUSEMOVE,input)==99 && acknowledged==1,"own injection passes and acknowledges the hook");
    event.flags=0; event.dwExtraInfo=0; pulse=GetTickCount64()-150;
    check(hook(HC_ACTION,WM_MOUSEMOVE,input)==99 && !gate,"stale input worker fails open to native movement");
    gate=true; pulse=GetTickCount64(); issued=acknowledged=0;
    emitAbsolute({3.6,-2.4});
    check(cursor.x==54 && cursor.y==98,"absolute pixel-center mapping handles negative virtual desktop origins");
    check(outputs.back().mi.dwFlags&MOUSEEVENTF_VIRTUALDESK,"absolute output spans the virtual desktop");
    check(outputs.back().mi.dwExtraInfo==outputTag && issued==acknowledged,"all output is tagged and acknowledged");
    fractionX=fractionY=0; cursor={50,100};
    for(unsigned i=0;i<10;++i) emitAbsolute({.1,0});
    check(cursor.x==51 && cursor.y==100,"subpixel motion is retained across reports");
    RAWINPUT raw{}; raw.header.hDevice=reinterpret_cast<HANDLE>(1); raw.data.mouse.lLastX=3; raw.data.mouse.lLastY=-2;
    options.mouseAdaptive=false;
    mouseInput(raw);
    check(outputs.back().mi.dx==3 && outputs.back().mi.dy==-2 && !(outputs.back().mi.dwFlags&MOUSEEVENTF_ABSOLUTE),"mouse fallback uses original relative counts with Windows processing");
    size_t previous=outputs.size(); raw.header.hDevice=nullptr; mouseInput(raw);
    check(outputs.size()==previous,"native touchpad mouse echo is not injected a second time");
    raw.header.hDevice=reinterpret_cast<HANDLE>(1); raw.data.mouse.ulExtraInformation=outputTag; mouseInput(raw);
    check(outputs.size()==previous,"own Raw Input echo cannot cause recursion");
    raw.data.mouse.ulExtraInformation=0; raw.data.mouse.usFlags=MOUSE_MOVE_ABSOLUTE; mouseInput(raw);
    check(!gate,"unsupported absolute mouse fails open");
    gate=true; issued=acknowledged+1; pendingSince=monotonicUsec()-10000;
    INPUT pending{}; pending.mi.dwFlags=MOUSEEVENTF_MOVE;
    auto oldestPending=pendingSince; acknowledgeOutput=false;
    for(unsigned i=0;i<10;++i) sendMotion(pending);
    check(pendingSince==oldestPending,"continuous output cannot postpone the missing-acknowledgement deadline");
    pendingSince=monotonicUsec()-30000;
    check(!sendMotion(pending) && !gate,"lost hook acknowledgement stops injection");
    gate=true; issued=acknowledged=0; sendSucceeds=false;
    check(!sendMotion(pending) && !gate,"failed SendInput restores native motion immediately");
    std::printf("PASS: %u pointer output checks; all cursor and injection APIs simulated\n",checks);
}
