#pragma once
#include "libinput_scroll.h"

// Windows driver policy: choose once, before the scroll detector starts.
// Horizontal gestures go to Windows; vertical gestures keep our wheel backend.
// Keep the choice until every finger is lifted, including a 2 -> 1 transition.
class HorizontalGestureHandoff {
public:
    enum class Route { Idle, Pending, Custom, Native, Touchscreen };
    struct Result {
        Route route=Route::Idle;
        bool decided=false;
        bool lifted=false;
        unsigned samples=0;
        double dx=0,dy=0;
        Route ended=Route::Idle;
    };
private:
    Route route=Route::Idle;
    Contact origin[2]{};
    bool anchored=false;
    unsigned samples=0;
    double latestDx=0,latestDy=0;
    void track(const std::vector<Contact>& contacts) {
        if(!anchored || contacts.size()!=2) return;
        const Contact* current[2]{};
        for(const auto& contact:contacts) for(unsigned i=0;i<2;++i) if(contact.id==origin[i].id) current[i]=&contact;
        if(current[0] && current[1] && current[0]!=current[1]) {
            latestDx=(current[0]->x-origin[0].x+current[1]->x-origin[1].x)/2;
            latestDy=(current[0]->y-origin[0].y+current[1]->y-origin[1].y)/2;
        }
    }
public:
    void reset() { route=Route::Idle; anchored=false; samples=0; latestDx=latestDy=0; }
    Route current() const { return route; }
    Result feed(const std::vector<Contact>& contacts,bool clicked=false) {
        if(contacts.empty()) {
            Result result{Route::Idle,false,route!=Route::Idle,samples,latestDx,latestDy,route};
            reset(); return result;
        }
        track(contacts);
        if(route==Route::Native || route==Route::Custom) return {route};
        if(clicked || contacts.size()>2) {
            route=Route::Custom; return {route,true,false,++samples};
        }
        if(contacts.size()!=2) { anchored=false; return {route}; }
        ++samples;
        if(!anchored) {
            origin[0]=contacts[0]; origin[1]=contacts[1];
            anchored=true; route=Route::Pending; return {route,false,false,samples};
        }
        const Contact* current[2]{};
        for(const auto& contact:contacts) for(unsigned i=0;i<2;++i) if(contact.id==origin[i].id) current[i]=&contact;
        if(!current[0] || !current[1] || current[0]==current[1]) {
            anchored=false; return {route,false,false,samples};
        }
        const double ax=current[0]->x-origin[0].x,ay=current[0]->y-origin[0].y;
        const double bx=current[1]->x-origin[1].x,by=current[1]->y-origin[1].y;
        double dx=(ax+bx)/2,dy=(ay+by)/2;
        // Opposing fingers are a pinch: the existing zoom path stays native.
        if(ax*bx+ay*by<0 && std::hypot(ax,ay)>=0.12 && std::hypot(bx,by)>=0.12) {
            route=Route::Custom; return {route,true,false,samples,dx,dy};
        }
        // Landing jitter must not commit the wrong owner before a real swipe.
        if(std::hypot(dx,dy)<0.06) return {route,false,false,samples,dx,dy};
        const double x=std::fabs(dx),y=std::fabs(dy);
        if(x>=y*1.25) route=Route::Native;
        else if(y>=x*1.25) route=Route::Custom;
        else if(std::hypot(dx,dy)>=0.20) route=x>y?Route::Native:Route::Custom;
        return {route,route!=Route::Pending,false,samples,dx,dy};
    }
};
