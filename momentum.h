#pragma once
#include "libinput_scroll.h"

// Momentum is a Windows adapter feature. libinput itself supplies finger deltas;
// compositor-side inertia is replaced here by timed, decaying wheel events.
class MomentumScroll {
    normalized_coords velocity{};
    uint64_t sampleTime=0,lastMotion=0,tickTime=0,started=0;
    bool running=false;
    unsigned previousContacts=0;
    bool pendingRelease=false;
    double friction=0.002;
public:
    void setFriction(double value) { friction=std::clamp(value,0.0001,0.05); }
    double getFriction() const { return friction; }
    void reset() { velocity={}; sampleTime=lastMotion=tickTime=started=0; running=false; previousContacts=0; pendingRelease=false; }
    void cancelActive() { running=false; }
    bool active() const { return running; }
    struct TouchResult { bool started=false,newTwoFingerTouch=false; };
    TouchResult updateContacts(unsigned count,bool pressed,normalized_coords delta,uint64_t eventTime,uint64_t wallTime) {
        const unsigned previous=previousContacts;
        TouchResult result{false,!pressed && count==2 && previous!=2};
        // A physical click also arrives with zero or one trusted contact.
        // Clear pending release and velocity so lifting cannot revive a fling.
        if(pressed) { reset(); previousContacts=count; return result; }
        if(count>=2) {
            // A fresh two-finger touch stops the old fling and discards its
            // velocity. Holding still and lifting again must not restart it.
            if(previous!=2 || count!=2) reset();
            else cancelActive();
            pendingRelease=false;
            if(count==2) observe(delta,eventTime);
        } else {
            // Allow 2 -> 1 -> 0 while lifting the scrolling fingers. A new
            // one-finger cursor gesture during an existing fling does not
            // cancel it or arm another release.
            if(previous==2) pendingRelease=true;
            if(count==0 && pendingRelease) {
                pendingRelease=false;
                result.started=release(eventTime,wallTime);
            }
        }
        previousContacts=count;
        return result;
    }
    void observe(normalized_coords delta,uint64_t time) {
        if(sampleTime && time>sampleTime && time-sampleTime<=100000) {
            double dt=static_cast<double>(time-sampleTime);
            double oldWeight=std::exp(-dt/40000.0);
            velocity.x=velocity.x*oldWeight+(delta.x*1000000/dt)*(1-oldWeight);
            velocity.y=velocity.y*oldWeight+(delta.y*1000000/dt)*(1-oldWeight);
        } else velocity={};
        sampleTime=time;
        if(delta.x || delta.y) lastMotion=time;
    }
    bool release(uint64_t eventTime,uint64_t wallTime) {
        double speed=std::hypot(velocity.x,velocity.y);
        if(!lastMotion || eventTime<lastMotion || eventTime-lastMotion>80000 || speed<50) { running=false; return false; }
        if(speed>6000) { velocity.x*=6000/speed; velocity.y*=6000/speed; }
        running=true; tickTime=started=wallTime; return true;
    }
    normalized_coords tick(uint64_t wallTime) {
        if(!running || wallTime<=tickTime) return {};
        uint64_t elapsed=wallTime-tickTime;
        if(elapsed>100000 || wallTime-started>4000000) { running=false; return {}; }
        // Firefox desktop APZ: v(t+dt) = v(t)*(1-friction)^dt_ms.
        // This is the equivalent exponential time constant, in seconds.
        const double tau=-0.001/std::log1p(-friction);
        double decay=std::exp(-static_cast<double>(elapsed)/1000000/tau);
        normalized_coords delta{velocity.x*tau*(1-decay),velocity.y*tau*(1-decay)};
        velocity.x*=decay; velocity.y*=decay; tickTime=wallTime;
        if(std::hypot(velocity.x,velocity.y)<8) running=false;
        return delta;
    }
};
