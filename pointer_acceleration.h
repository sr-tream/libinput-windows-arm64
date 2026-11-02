#pragma once
#include "libinput_scroll.h"
#include <array>
#include <cassert>

// Compatibility types for unchanged libinput pointer filter functions. They
// are isolated from the smaller structs used by the existing scroll port.
namespace LibinputPointer {
using std::min;
constexpr double DEFAULT_THRESHOLD=0.0004,MINIMUM_THRESHOLD=0.0002;
constexpr double DEFAULT_ACCELERATION=2.0,DEFAULT_INCLINE=1.1;
constexpr unsigned DEFAULT_MOUSE_DPI=1000;
constexpr usec_t MOTION_TIMEOUT=1000000;
struct motion_filter { double speed_adjustment=0; };
using accel_profile_func_t=double(*)(motion_filter*,void*,double,usec_t);
struct pointer_tracker { device_float_coords delta; usec_t time=0; uint32_t dir=0; };
struct pointer_delta_smoothener { usec_t threshold=0,value=0; };
struct pointer_trackers { pointer_tracker* trackers=nullptr; size_t ntrackers=0; unsigned cur_tracker=0; pointer_delta_smoothener* smoothener=nullptr; };
struct touchpad_accelerator { motion_filter base; double threshold=130; int dpi=1000; double speed_factor=1; };
struct pointer_accelerator { motion_filter base; double threshold=DEFAULT_THRESHOLD,accel=DEFAULT_ACCELERATION,incline=DEFAULT_INCLINE; int dpi=1000; };
struct pointer_accelerator_low_dpi { motion_filter base; double threshold=DEFAULT_THRESHOLD,accel=DEFAULT_ACCELERATION,incline=DEFAULT_INCLINE; int dpi=1000; };
inline double v_us2ms(double value) { return value*1000; }
inline double v_us2s(double value) { return value*1000000; }
inline double v_usec_from_millis(double value) { return value/1000; }
inline uint32_t device_float_get_direction(device_float_coords value) { return xy_get_direction(value.x,value.y); }
pointer_tracker* trackers_by_offset(pointer_trackers*,unsigned);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "third_party/libinput/pointer-trackers.inc"
#include "third_party/libinput/pointer-touchpad.inc"
#include "third_party/libinput/pointer-mouse.inc"
#include "third_party/libinput/pointer-low-dpi.inc"
#pragma clang diagnostic pop
}

enum class PointerDevice { Touchpad,Mouse };
class AdaptivePointer {
    PointerDevice device;
    unsigned dpi;
    std::array<LibinputPointer::pointer_tracker,16> history{};
    LibinputPointer::pointer_trackers trackers;
    LibinputPointer::touchpad_accelerator touchpad;
    LibinputPointer::pointer_accelerator mouse;
    LibinputPointer::pointer_accelerator_low_dpi low;
    double lastVelocity=0;
    usec_t lastTime=0;
public:
    explicit AdaptivePointer(PointerDevice kind,unsigned resolution=1000):device(kind),dpi(std::clamp(resolution,100u,64000u)),trackers{history.data(),history.size(),0,nullptr} { low.dpi=mouse.dpi=touchpad.dpi=static_cast<int>(dpi); setSpeed(0); }
    AdaptivePointer(const AdaptivePointer&)=delete;
    AdaptivePointer& operator=(const AdaptivePointer&)=delete;
    void setSpeed(double speed) {
        speed=std::clamp(std::isfinite(speed)?speed:0.0,-1.0,1.0);
        LibinputPointer::touchpad_accelerator_set_speed(&touchpad.base,speed);
        LibinputPointer::accelerator_set_speed(&mouse.base,speed);
        low.base=mouse.base; low.threshold=mouse.threshold; low.accel=mouse.accel; low.incline=mouse.incline;
    }
    void restart(usec_t time) { LibinputPointer::trackers_reset(&trackers,time); lastTime=time; }
    void reset(usec_t time=0) { history={}; trackers.cur_tracker=0; lastVelocity=0; restart(time); }
    normalized_coords feed(device_float_coords delta,usec_t time) {
        if(!std::isfinite(delta.x) || !std::isfinite(delta.y)) { reset(time); return {}; }
        if(time<lastTime) { reset(time); return {}; }
        lastTime=time;
        // Touchpad coordinates are normalized from physical mm by the caller;
        // mouse counts are normalized before tracking, as in filter-mouse.c.
        auto normalized=normalize_for_dpi(&delta,static_cast<int>(dpi));
        const bool lowDpi=device==PointerDevice::Mouse && dpi<1000;
        if(lowDpi) normalized={delta.x,delta.y};
        device_float_coords tracked=device==PointerDevice::Mouse && !lowDpi?device_float_coords{normalized.x,normalized.y}:delta;
        LibinputPointer::trackers_feed(&trackers,&tracked,time);
        double velocity=LibinputPointer::trackers_velocity(&trackers,time);
        auto* filter=device==PointerDevice::Touchpad?&touchpad.base:lowDpi?&low.base:&mouse.base;
        auto profile=device==PointerDevice::Touchpad?LibinputPointer::touchpad_accel_profile_linear:lowDpi?LibinputPointer::pointer_accel_profile_linear_low_dpi:LibinputPointer::pointer_accel_profile_linear;
        double factor=LibinputPointer::calculate_acceleration_simpsons(filter,profile,nullptr,velocity,lastVelocity,time);
        lastVelocity=velocity;
        return {normalized.x*factor,normalized.y*factor};
    }
};

class TouchpadPointer {
    AdaptivePointer filter{PointerDevice::Touchpad};
    Contact previous{};
    bool anchored=false;
    usec_t lastTime=0;
public:
    void setSpeed(double value) { filter.setSpeed(value); reset(); }
    void reset() { anchored=false; lastTime=0; filter.reset(); }
    normalized_coords feed(const std::vector<Contact>& contacts,usec_t time) {
        if(contacts.size()!=1) { anchored=false; return {}; }
        const auto current=contacts.front();
        if(!anchored || current.id!=previous.id || time<=lastTime || time-lastTime>100000) {
            anchored=true; previous=current; lastTime=time; filter.restart(time); return {};
        }
        device_float_coords delta{(current.x-previous.x)*1000/25.4,(current.y-previous.y)*1000/25.4};
        previous=current; lastTime=time;
        if(std::hypot(delta.x,delta.y)>20*1000/25.4) { reset(); return {}; }
        return filter.feed(delta,time);
    }
};
