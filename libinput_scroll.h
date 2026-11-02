#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

// Narrow compatibility layer for unchanged libinput scroll functions.
// Raw device coordinates are converted to mm by the Windows adapter.
using usec_t = uint64_t;
inline usec_t usec_from_millis(uint64_t n) { return n*1000; }
inline usec_t usec_from_uint64_t(uint64_t n) { return n; }
inline uint64_t usec_as_uint64_t(usec_t n) { return n; }
inline bool usec_is_zero(usec_t n) { return n==0; }
inline usec_t usec_delta(usec_t a,usec_t b) { return a-b; }
inline usec_t usec_add(usec_t a,usec_t b) { return a+b; }
inline usec_t usec_sub(usec_t a,usec_t b) { return a-b; }
inline int usec_cmp(usec_t a,usec_t b) { return (a>b)-(a<b); }
inline bool usec_gt(usec_t a,usec_t b) { return a>b; }
struct device_float_coords { double x=0,y=0; };
struct phys_coords { double x=0,y=0; };
struct normalized_coords { double x=0,y=0; };
struct tp_dispatch {
    struct {
        struct { bool h=false,v=false; } active;
        struct { usec_t h=0,v=0; } duration;
        phys_coords vector;
        usec_t time_prev=0;
    } scroll;
};
inline phys_coords tp_phys_delta(tp_dispatch*,device_float_coords raw) { return {raw.x,raw.y}; }
struct motion_filter {};
struct touchpad_accelerator { motion_filter base; int dpi=1000; };
inline normalized_coords normalize_for_dpi(const device_float_coords* d,int dpi) { return {d->x*1000.0/dpi,d->y*1000.0/dpi}; }
#define TP_MAGIC_SLOWDOWN 0.2968
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "third_party/libinput/constant-filter.inc"
#pragma clang diagnostic pop
#include "third_party/libinput/scroll-constraints.inc"
#include "third_party/libinput/same-directions.inc"
constexpr double M_PI=3.14159265358979323846;
constexpr double M_1_PI=1/M_PI;
constexpr uint32_t bit(int n) { return 1u<<n; }
enum directions { N=bit(0),NE=bit(1),E=bit(2),SE=bit(3),S=bit(4),SW=bit(5),W=bit(6),NW=bit(7),UNDEFINED_DIRECTION=0xff };
#include "third_party/libinput/direction.inc"

struct Contact { uint32_t id; double x,y; };
class ScrollProcessor {
    tp_dispatch tp{};
    touchpad_accelerator filter{};
    std::map<uint32_t,Contact> previous,initial;
    bool scrolling=false,pinching=false;
    usec_t last=0,start=0;
    static uint32_t direction(double x,double y) {
        return xy_get_direction(x,y);
    }
public:
    void reset() { tp={}; previous.clear(); initial.clear(); scrolling=false; pinching=false; last=start=0; }
    normalized_coords feed(const std::vector<Contact>& contacts,usec_t now,bool clicked=false) {
        if(contacts.size()!=2 || clicked) { reset(); return {}; }
        std::map<uint32_t,Contact> current;
        for(const auto& c:contacts) current[c.id]=c;
        if(current.size()!=2) { reset(); return {}; }
        bool same=previous.size()==2;
        for(const auto& [id,c]:current) same=same && previous.count(id);
        if(!same || !last || now<=last || now-last>100000) {
            reset(); previous=initial=current; last=start=now; return {};
        }
        device_float_coords delta{};
        std::vector<phys_coords> moved;
        for(const auto& [id,c]:current) {
            delta.x+=(c.x-previous[id].x)/2; delta.y+=(c.y-previous[id].y)/2;
            moved.push_back({c.x-initial[id].x,c.y-initial[id].y});
        }
        previous=current; last=now;
        if(pinching) return {};
        if(scrolling && now-start<300000) {
            const auto& a=moved[0]; const auto& b=moved[1];
            if(std::hypot(a.x,a.y)>=1.5 && std::hypot(b.x,b.y)>=1.5 && !tp_gesture_same_directions(direction(a.x,a.y),direction(b.x,b.y))) {
                pinching=true; tp={}; return {};
            }
        }
        if(!scrolling) {
            const auto& a=moved[0]; const auto& b=moved[1];
            double da=std::hypot(a.x,a.y),db=std::hypot(b.x,b.y);
            // libinput's 2-finger detection thresholds (mm). Windows keeps zoom.
            if(da<1 && db<1) return {};
            if(da>=1.5 && db>=1.5) {
                if(!tp_gesture_same_directions(direction(a.x,a.y),direction(b.x,b.y))) { pinching=true; return {}; }
                scrolling=true;
            } else if(now-start>150000 || da>=4 || db>=4) scrolling=true;
            if(!scrolling) return {};
            tp={}; // libinput initializes constraints on scroll start.
            return {}; // Drop accumulated motion to avoid a start jump.
        }
        device_float_coords normalizedRaw{delta.x*1000/25.4,delta.y*1000/25.4};
        auto result=touchpad_constant_filter(&filter.base,&normalizedRaw,nullptr,now);
        tp_gesture_apply_scroll_constraints(&tp,&delta,&result,now);
        return result;
    }
};

// Windows vertical and horizontal wheels have opposite sign conventions.
class WheelAccumulator {
    double x=0,y=0;
public:
    void reset() { x=y=0; }
    struct Delta { int32_t x,y; };
    Delta feed(normalized_coords d,double scale,bool reversed) {
        x+=d.x*scale*(reversed?-1:1);
        y+=d.y*scale*(reversed?1:-1);
        int32_t ix=static_cast<int32_t>(x),iy=static_cast<int32_t>(y);
        x-=ix; y-=iy; return {ix,iy};
    }
};
