#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
static std::vector<POINTER_TYPE_INFO> delivered;
static unsigned destroyed=0; static DWORD nextError=0;
static HSYNTHETICPOINTERDEVICE WINAPI fakeCreate(POINTER_INPUT_TYPE type,ULONG count,POINTER_FEEDBACK_MODE) { return type==PT_TOUCH && count==1?reinterpret_cast<HSYNTHETICPOINTERDEVICE>(1):nullptr; }
static void WINAPI fakeDestroy(HSYNTHETICPOINTERDEVICE) { ++destroyed; }
static BOOL WINAPI fakeInject(HSYNTHETICPOINTERDEVICE,const POINTER_TYPE_INFO* frames,UINT32 count) {
    if(nextError) { SetLastError(nextError); nextError=0; return FALSE; }
    for(UINT32 i=0;i<count;++i) delivered.push_back(frames[i]); return TRUE;
}
static BOOL WINAPI fakeCursor(LPPOINT point) { *point={100,100}; return TRUE; }
static BOOL WINAPI fakeRect(HWND,LPRECT rect) { *rect={0,0,500,500}; return TRUE; }
static BOOL WINAPI fakeToScreen(HWND,LPPOINT) { return TRUE; }
#define CreateSyntheticPointerDevice fakeCreate
#define DestroySyntheticPointerDevice fakeDestroy
#define InjectSyntheticPointerInput fakeInject
#define GetPhysicalCursorPos fakeCursor
#define GetClientRect fakeRect
#define ClientToScreen fakeToScreen
#include "touchscreen_output.h"
#undef CreateSyntheticPointerDevice
#undef DestroySyntheticPointerDevice
#undef InjectSyntheticPointerInput
#undef GetPhysicalCursorPos
#undef GetClientRect
#undef ClientToScreen
#include "touchscreen_handoff.h"
#define wmain libinput_application_wmain
#include "touchpad.cpp"
#undef wmain
static unsigned checks=0;
static void check(bool value,const char* name) { ++checks; if(!value) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); } }
int main() {
    using Route=HorizontalGestureHandoff::Route;
    HWND window=reinterpret_cast<HWND>(1),other=reinterpret_cast<HWND>(2);
    std::vector<Contact> origin{{1,40,30},{2,60,30}};
    for(double sign:{-1.0,1.0}) {
        TouchscreenGestureHandoff router; router.feed(origin,false,true);
        check(router.feed({{1,40,30.2},{2,60,30}},false,true).route==Route::Custom,"unknown vertical start remains our scroll");
        check(router.feed({{1,40+sign*.2,30.2},{2,60+sign*.2,30}},false,true).route==Route::Custom,"tiny horizontal movement cannot generate a touchscreen tap");
        check(router.feed({{1,40+sign*.9,30.2},{2,60+sign*.9,30}},false,true).route==Route::Touchscreen,"unknown short start can become touchscreen drag");
        check(router.feed({{1,40,30}},false,true).route==Route::Touchscreen,"touchscreen ownership persists until full release");
        check(router.feed({},false,true).lifted,"touchscreen release resets policy");
    }
    TouchscreenOutput output;
    TouchscreenGestureHandoff reversed;
    reversed.feed(origin,false,true);
    reversed.feed({{1,39.5,30},{2,59.5,30}},false,true);
    check(reversed.feed({{1,40.4,30},{2,60.4,30}},false,true).route==Route::Touchscreen,"short initial reversal can still select screen drag");
    check(reversed.lateralDisplacement().x>.8,"screen drag starts from aligned segment, rather than small net displacement");
    check(output.begin(window,window,origin,.9,.1),"single screen contact starts");
    check(output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_DOWN,0xff5157d7,nullptr),"observed null-device touchscreen mouse promotion is an echo");
    check(!output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_DOWN,0xff5157d7,other),"hardware device remains a real click even if extra information resembles touch");
    check(!output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_DOWN,0,nullptr),"untagged null-device PTP tap remains a real click");
    check(!output.isMouseEcho(RI_MOUSE_RIGHT_BUTTON_DOWN,0xff5157d7,nullptr),"right click is never ignored");
    check(!output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_RIGHT_BUTTON_DOWN,0xff5157d7,nullptr),"combined real button presses are never ignored");
    check(!output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_DOWN,0xff515757,nullptr),"pen signature is not a touch echo");
    check(!output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_UP,0xff5157d7,nullptr),"button release is not an echo-down notification");
    check(delivered.size()==1 && (delivered.back().touchInfo.pointerInfo.pointerFlags&POINTER_FLAG_DOWN),"only down is injected on first frame, avoiding timestamp collisions");
    check(output.feed({{1,40.2,30},{2,60.2,30}},false,window,window),"later HID frame updates contact");
    check(delivered.back().touchInfo.pointerInfo.ptPixelLocation.x==111 && delivered.back().touchInfo.pointerInfo.ptPixelLocation.y==101,"centroid travel becomes screen movement including initial drift");
    check(output.feed({},false,window,window) && !output.active(),"lifting fingers ends screen contact");
    check(!output.isMouseEcho(RI_MOUSE_LEFT_BUTTON_DOWN,0xff5157d7,nullptr),"touch-like click without active synthetic contact is not ignored");
    check(delivered.back().touchInfo.pointerInfo.pointerFlags==POINTER_FLAG_UP,"completed drag ends normally");
    check(delivered.back().touchInfo.pointerInfo.ptPixelLocation.x==111,"UP keeps last successful coordinates");
    delivered.clear(); check(output.begin(window,window,origin,.9,0),"next contact begins");
    check(output.feed({},false,window,window),"immediate release succeeds");
    check(delivered.back().touchInfo.pointerInfo.pointerFlags&POINTER_FLAG_CANCELED,"immediate release is canceled instead of clicking");
    for(unsigned condition=0;condition<3;++condition) {
        output.begin(window,window,origin,.9,0);
        auto contacts=origin; if(condition==2) contacts.push_back({3,70,30});
        check(output.feed(contacts,condition==0,condition==1?other:window,window),"click, foreground, or extra finger cancels");
        check(!output.active() && (delivered.back().touchInfo.pointerInfo.pointerFlags&POINTER_FLAG_CANCELED),"canceled contact cannot migrate to another application");
    }
    output.begin(window,window,origin,.9,0);
    check(output.feed(origin,false,window,other) && output.active(),"hover popup cannot release captured screen contact");
    check(output.feed(origin,false,window,other) && output.active(),"subsequent movement remains captured by original destination");
    output.foregroundChanged(other,GetTickCount()-100);
    check(output.active(),"queued foreground event predating DOWN cannot cancel newer contact");
    output.foregroundChanged(other,GetTickCount());
    check(!output.active(),"new foreground event still cancels contact");
    auto ended=output.takeEnd();
    check(ended.pending && ended.canceled && ended.moves>0,"contact lifecycle records cancellation and delivered movement");
    check(!output.takeEnd().pending,"lifecycle event is consumed once");
    output.begin(window,window,origin,.9,0); nextError=ERROR_NOT_READY;
    check(output.feed(origin,false,window,window) && output.active(),"timestamp collision never waits inside input handler");
    check(output.feed(origin,false,window,window),"latest position retries on next HID frame");
    check(delivered.back().touchInfo.pointerInfo.pointerFlags&POINTER_FLAG_UPDATE,"retry delivers update");
    nextError=ERROR_NOT_READY; check(!output.finish() && !output.active() && destroyed==1,"failed UP destroys injection device to release contact");
    check(output.begin(window,window,origin,.9,0),"device can be recreated after canceled failure");
    check(output.feed({{1,1000,1000},{2,1020,1000}},false,window,window),"large travel is accepted");
    check(delivered.back().touchInfo.pointerInfo.ptPixelLocation.x==499 && delivered.back().touchInfo.pointerInfo.ptPixelLocation.y==499,"contact remains within original client bounds");
    output.foregroundChanged(other); check(!output.active(),"foreground event cancels even between HID frames");
    check(output.begin(window,window,origin,.1,0),"small initial displacement fixture begins");
    check(output.feed(origin,false,window,window),"small screen update succeeds");
    check(output.finish() && (delivered.back().touchInfo.pointerInfo.pointerFlags&POINTER_FLAG_CANCELED),"sub-drag movement cannot turn into a tap");
    output.configure({20,true,true});
    check(output.begin(window,window,origin,.9,.2),"configured screen contact begins");
    check(output.feed({{1,40.2,30.3},{2,60.2,30.3}},false,window,window),"configured screen contact updates");
    check(delivered.back().touchInfo.pointerInfo.ptPixelLocation.x==78 && delivered.back().touchInfo.pointerInfo.ptPixelLocation.y==100,"gain, reversal and horizontal lock apply to screen coordinates");
    output.finish(); output.close(); check(destroyed==2,"cleanup destroys persistent pointer device");
    check(touchscreenOutput.begin(window,window,origin,.9,0),"real observer fixture starts a simulated screen contact");
    observeMouseButtons(RI_MOUSE_LEFT_BUTTON_DOWN,0xff5157d7,nullptr);
    check(touchscreenOutput.active() && touchMouseEchoes==1,"actual Raw Input observer preserves contact on recorded touch echo");
    observeMouseButtons(RI_MOUSE_LEFT_BUTTON_DOWN,0,nullptr);
    check(!touchscreenOutput.active(),"actual observer still cancels on untagged null-device tap");
    touchscreenOutput.begin(window,window,origin,.9,0);
    observeMouseButtons(RI_MOUSE_LEFT_BUTTON_DOWN,0xff5157d7,other);
    check(!touchscreenOutput.active(),"actual observer still cancels on hardware mouse click");
    touchscreenOutput.begin(window,window,origin,.9,0);
    observeMouseButtons(RI_MOUSE_RIGHT_BUTTON_DOWN,0xff5157d7,nullptr);
    check(!touchscreenOutput.active(),"actual observer still cancels on right button");
    touchscreenOutput.close();
    std::printf("PASS: %u touchscreen checks; all injection and geometry APIs simulated\n",checks);
}
