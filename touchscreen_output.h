#pragma once
#include "libinput_scroll.h"
struct TouchscreenOptions { double pixelsPerMm=10; bool invertHorizontal=false,lockVertical=false; };

// One screen contact follows the two-finger centroid. No cursor repositioning,
// sleep, synchronous hook, or application-specific shortcut is used.
class TouchscreenOutput {
public:
    struct EndInfo {
        bool pending=false,canceled=false;
        const char* reason="none";
        unsigned frames=0,moves=0;
        DWORD elapsedMs=0;
        LONG dx=0,dy=0;
    };
private:
    HSYNTHETICPOINTERDEVICE device=nullptr;
    bool touching=false,moved=false;
    HWND foreground=nullptr,target=nullptr;
    POINT start{},last{};
    RECT bounds{};
    double originX=0,originY=0,initialX=0,initialY=0;
    DWORD lastError=0;
    DWORD boundAt=0;
    unsigned contactFrames=0,moveFrames=0;
    EndInfo ended{};
    TouchscreenOptions options;
    bool send(POINTER_FLAGS flags,POINT point) {
        POINTER_TYPE_INFO frame{}; frame.type=PT_TOUCH;
        auto& touch=frame.touchInfo; auto& pointer=touch.pointerInfo;
        pointer.pointerType=PT_TOUCH; pointer.pointerId=0;
        pointer.pointerFlags=flags; pointer.ptPixelLocation=point;
        touch.touchMask=TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;
        touch.rcContact={point.x-3,point.y-3,point.x+3,point.y+3};
        touch.orientation=90; touch.pressure=320;
        if(!InjectSyntheticPointerInput(device,&frame,1)) { lastError=GetLastError(); return false; }
        last=point; lastError=0; return true;
    }
public:
    bool active() const { return touching; }
    void configure(TouchscreenOptions value) { options=value; options.pixelsPerMm=std::clamp(std::isfinite(value.pixelsPerMm)?value.pixelsPerMm:10.0,1.0,40.0); }
    bool isMouseEcho(USHORT flags,ULONG extra,HANDLE sourceDevice) const {
        constexpr USHORT down=RI_MOUSE_LEFT_BUTTON_DOWN|RI_MOUSE_RIGHT_BUTTON_DOWN|RI_MOUSE_MIDDLE_BUTTON_DOWN|RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN;
        // Windows tags touch-promoted mouse input with MI_WP_SIGNATURE and
        // its touch bit. Ignore only the null-device left-button echo while
        // our screen contact exists. Hardware mice and untagged PTP taps stay clicks.
        return touching && !sourceDevice && (flags&down)==RI_MOUSE_LEFT_BUTTON_DOWN &&
            (extra&0xffffff80u)==0xff515780u;
    }
    EndInfo takeEnd() { auto result=ended; ended={}; return result; }
    DWORD error() const { return lastError; }
    bool initialize() {
        if(device) return true;
        device=CreateSyntheticPointerDevice(PT_TOUCH,1,POINTER_FEEDBACK_NONE);
        if(!device) lastError=GetLastError();
        return device!=nullptr;
    }
    bool finish(bool canceled=false,const char* reason="release") {
        if(!touching) return true;
        // UP uses the last successfully injected position, as required by the API.
        const bool cancel=canceled || !moved;
        bool ok=send(POINTER_FLAG_UP|(cancel?POINTER_FLAG_CANCELED:0),last);
        ended={true,cancel,reason,contactFrames,moveFrames,GetTickCount()-boundAt,last.x-start.x,last.y-start.y};
        touching=false;
        if(!ok && device) { DestroySyntheticPointerDevice(device); device=nullptr; }
        return ok;
    }
    void close() {
        finish(true,"shutdown");
        if(device) { DestroySyntheticPointerDevice(device); device=nullptr; }
    }
    void foregroundChanged(HWND window,DWORD eventTime=0) {
        // Out-of-context WinEvents may arrive after a newer gesture began.
        if(eventTime && static_cast<LONG>(eventTime-boundAt)<0) return;
        if(touching && window!=foreground) finish(true,"foreground_event");
    }
    bool begin(HWND owner,HWND destination,const std::vector<Contact>& contacts,double dx,double dy) {
        if(contacts.size()!=2 || !owner || !destination || !initialize()) return false;
        if(!GetPhysicalCursorPos(&start) || !GetClientRect(destination,&bounds)) return false;
        POINT corners[2]{{bounds.left,bounds.top},{bounds.right,bounds.bottom}};
        ClientToScreen(destination,&corners[0]); ClientToScreen(destination,&corners[1]);
        bounds={corners[0].x,corners[0].y,corners[1].x,corners[1].y};
        if(!PtInRect(&bounds,start)) return true; // Never touch a non-client caption/border.
        foreground=owner; target=destination; moved=false;
        boundAt=GetTickCount(); contactFrames=moveFrames=0;
        originX=(contacts[0].x+contacts[1].x)/2; originY=(contacts[0].y+contacts[1].y)/2;
        initialX=dx; initialY=dy;
        touching=send(POINTER_FLAG_DOWN|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT,start);
        return touching;
    }
    bool feed(const std::vector<Contact>& contacts,bool clicked,HWND owner,HWND destination) {
        (void)destination;
        if(!touching) return true;
        ++contactFrames;
        // A DOWN contact stays captured by the original touch destination.
        // A popup under the stationary mouse cursor must not release that
        // contact. Foreground changes and real clicks still cancel it.
        if(clicked) return finish(true,"physical_click");
        if(owner!=foreground) return finish(true,"foreground_poll");
        if(contacts.size()>2) return finish(true,"extra_fingers");
        if(contacts.size()!=2) return finish(false,contacts.empty()?"all_fingers_up":"one_finger");
        const double x=initialX+(contacts[0].x+contacts[1].x)/2-originX;
        const double y=initialY+(contacts[0].y+contacts[1].y)/2-originY;
        POINT point{std::clamp(start.x+static_cast<LONG>(std::lround(x*options.pixelsPerMm*(options.invertHorizontal?-1:1))),bounds.left,bounds.right-1),
                    std::clamp(start.y+static_cast<LONG>(std::lround(options.lockVertical?0:y*options.pixelsPerMm)),bounds.top,bounds.bottom-1)};
        if(point.x==last.x && point.y==last.y) return true;
        if(!send(POINTER_FLAG_UPDATE|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT,point))
            return lastError==ERROR_NOT_READY; // Retry latest position on the next HID frame, never wait here.
        moved=moved || std::hypot(static_cast<double>(point.x-start.x),static_cast<double>(point.y-start.y))>=8;
        ++moveFrames; return true;
    }
};
