// Exercise the actual Raw Input button and foreground callback paths without
// registering devices, injecting input or changing touchpad settings.
#define wmain libinput_application_wmain
#include "touchpad.cpp"
#undef wmain
#include <cstdlib>

static unsigned checks=0;
static void check(bool condition,const char* name) {
    ++checks;
    if(!condition) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); }
}
static void prepareFling() {
    runMode=true; cancelMomentum(true); processorNeedsReset=false;
    for(unsigned i=0;i<30;++i) momentum.updateContacts(2,false,{0,1},100000+i*10000,700000+i*10000);
    auto lifted=momentum.updateContacts(0,false,{},400000,1000000);
    check(lifted.started && momentum.active(),"fixture starts an active fling");
    scrollContext={reinterpret_cast<HWND>(1),reinterpret_cast<HWND>(1)};
}
int main() {
    for(USHORT button: {RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_5_DOWN}) {
        prepareFling(); observeMouseButtons(button);
        check(!momentum.active() && !scrollContext.valid(),"each raw mouse button stops inertia and clears its destination");
        check(momentum.tick(1010000).y==0,"no inertial delta follows a click");
        auto lift=momentum.updateContacts(0,false,{},410000,1010000);
        check(!lift.started,"click discards release history");
    }
    prepareFling();
    observeMouseButtons(RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_LEFT_BUTTON_UP);
    check(!momentum.active(),"a short click with down and up in one report stops inertia");
    for(USHORT flags: {0,RI_MOUSE_LEFT_BUTTON_UP,RI_MOUSE_RIGHT_BUTTON_UP,RI_MOUSE_MIDDLE_BUTTON_UP,RI_MOUSE_BUTTON_4_UP,RI_MOUSE_BUTTON_5_UP,RI_MOUSE_WHEEL,RI_MOUSE_HWHEEL}) {
        prepareFling(); observeMouseButtons(flags);
        check(momentum.active(),"raw movement, release and wheel reports preserve inertia");
    }
    prepareFling();
    scrollForegroundEvent(nullptr,EVENT_SYSTEM_FOREGROUND,reinterpret_cast<HWND>(1),0,0,0,0);
    check(momentum.active(),"same foreground window preserves inertia");
    scrollForegroundEvent(nullptr,EVENT_SYSTEM_FOREGROUND,reinterpret_cast<HWND>(2),0,0,0,0);
    check(!momentum.active() && !scrollContext.valid(),"switching windows clears inertia and its destination");
    scrollForegroundEvent(nullptr,EVENT_SYSTEM_FOREGROUND,reinterpret_cast<HWND>(1),0,0,0,0);
    check(!momentum.active(),"switching back cannot revive an old fling");
    prepareFling(); scrollContextBoundAt=100;
    scrollForegroundEvent(nullptr,EVENT_SYSTEM_FOREGROUND,reinterpret_cast<HWND>(2),0,0,0,99);
    check(momentum.active(),"a delayed event from before this gesture does not cancel it");
    scrollForegroundEvent(nullptr,EVENT_SYSTEM_FOREGROUND,reinterpret_cast<HWND>(2),0,0,0,101);
    check(!momentum.active(),"a later focus event stops the gesture");
    momentum.reset();
    for(unsigned i=0;i<30;++i) momentum.updateContacts(2,false,{0,1},100000+i*10000,700000+i*10000);
    scrollContext={reinterpret_cast<HWND>(1),reinterpret_cast<HWND>(1)}; scrollContextBoundAt=0;
    scrollForegroundEvent(nullptr,EVENT_SYSTEM_FOREGROUND,reinterpret_cast<HWND>(2),0,0,0,1);
    auto movedRelease=momentum.updateContacts(0,false,{},400000,1000000);
    check(!movedRelease.started,"focus change before finger release cannot start old inertia in the new window");
    prepareFling();
    emitScroll({0,1},true);
    check(!momentum.active() && !scrollContext.valid(),"a changed wheel destination stops before any injection");
    cancelMomentum(true);
    std::printf("PASS: %u interaction checks; no input injected or settings changed\n",checks);
}
