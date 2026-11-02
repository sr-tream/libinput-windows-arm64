#pragma once
#include "gesture_handoff.h"

// Recognize an explicitly selected touchscreen drag after a bounded short start.
class TouchscreenGestureHandoff {
public:
    using Route=HorizontalGestureHandoff::Route;
    using Result=HorizontalGestureHandoff::Result;
private:
    Route route=Route::Idle;
    Contact origin[2]{},previous[2]{};
    bool anchored=false,eligible=true;
    unsigned samples=0,customOutputFrames=0;
    double latestX=0,latestY=0,path=0,maxVertical=0;
    double sideX=0,sideY=0,sideA=0,sideB=0;
    void resetSide() { sideX=sideY=sideA=sideB=0; }
public:
    void reset() { *this=TouchscreenGestureHandoff{}; }
    Route current() const { return route; }
    normalized_coords lateralDisplacement() const { return {sideX,sideY}; }
    bool mayReconsider() const { return eligible; }
    void noteCustomOutput() { if(++customOutputFrames>3) eligible=false; }
    Result feed(const std::vector<Contact>& contacts,bool clicked=false,bool allowTouch=true) {
        if(contacts.empty()) {
            Result result{Route::Idle,false,route!=Route::Idle,samples,latestX,latestY,route}; reset(); return result;
        }
        if(route==Route::Touchscreen) return {route};
        if(clicked || contacts.size()>2) {
            const bool changed=route!=Route::Custom; eligible=false; route=Route::Custom;
            return {route,changed,false,++samples,latestX,latestY};
        }
        if(contacts.size()!=2) {
            if(route==Route::Custom) eligible=false;
            anchored=false; resetSide(); return {route};
        }
        ++samples;
        if(!anchored) {
            origin[0]=previous[0]=contacts[0]; origin[1]=previous[1]=contacts[1];
            anchored=true;
            if(route==Route::Idle) route=Route::Pending;
            return {route,false,false,samples};
        }
        const Contact* current[2]{};
        for(const auto& contact:contacts) for(unsigned i=0;i<2;++i) if(contact.id==origin[i].id) current[i]=&contact;
        if(!current[0] || !current[1] || current[0]==current[1]) {
            anchored=false; eligible=false; resetSide(); return {route,false,false,samples};
        }
        const double ax=current[0]->x-origin[0].x,ay=current[0]->y-origin[0].y;
        const double bx=current[1]->x-origin[1].x,by=current[1]->y-origin[1].y;
        const double dax=current[0]->x-previous[0].x,day=current[0]->y-previous[0].y;
        const double dbx=current[1]->x-previous[1].x,dby=current[1]->y-previous[1].y;
        previous[0]=*current[0]; previous[1]=*current[1];
        latestX=(ax+bx)/2; latestY=(ay+by)/2;
        const double dx=(dax+dbx)/2,dy=(day+dby)/2;
        path+=std::hypot(dx,dy); maxVertical=std::max(maxVertical,std::fabs(latestY));
        if(maxVertical>1.0 || path>2.0) eligible=false;
        // Opposing fingers, clicks and extra fingers must never become a screen drag.
        if(ax*bx+ay*by<0 && std::hypot(ax,ay)>=0.12 && std::hypot(bx,by)>=0.12) {
            const bool changed=route!=Route::Custom; eligible=false; route=Route::Custom;
            return {route,changed,false,samples,latestX,latestY};
        }
        const bool horizontal=std::fabs(dx)>0.00001 && std::fabs(dx)>=2*std::fabs(dy) && dax*dbx>=0;
        if(horizontal) {
            if(sideX*dx<0) resetSide();
            sideX+=dx; sideY+=dy; sideA+=dax; sideB+=dbx;
        } else if(std::hypot(dx,dy)>0.015) resetSide();
        const bool aligned=std::fabs(sideX)>=0.12 && std::fabs(sideX)>=2.5*std::fabs(sideY) &&
            std::fabs(sideA)>=0.06 && std::fabs(sideB)>=0.06 && sideA*sideB>0;
        // A screen contact needs enough initial travel to become a drag,
        // rather than a tap if fingers are lifted immediately afterwards.
        if(eligible && allowTouch && aligned && std::fabs(sideX)>=0.8) {
            route=Route::Touchscreen; return {route,true,false,samples,latestX,latestY};
        }
        // This soft choice starts vertical scrolling at the native driver mode threshold.
        // A short downward/diagonal start remains eligible for lateral handoff.
        if(route==Route::Pending && std::hypot(latestX,latestY)>=0.06) {
            route=Route::Custom; return {route,true,false,samples,latestX,latestY};
        }
        return {route,false,false,samples,latestX,latestY};
    }
};
