#include "pointer_acceleration.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static unsigned checks=0;
static void check(bool value,const char* name) { ++checks; if(!value) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); } }
static bool near(double a,double b) { return std::fabs(a-b)<1e-10; }
int main() {
    using namespace LibinputPointer;
    LibinputPointer::touchpad_accelerator tp;
    check(near(touchpad_accel_profile_linear(&tp.base,nullptr,0,0),0.3*TP_MAGIC_SLOWDOWN),"touchpad slow movement has upstream deceleration");
    check(near(touchpad_accel_profile_linear(&tp.base,nullptr,50*1000/25.4/1000000,0),0.9*TP_MAGIC_SLOWDOWN),"touchpad plateau matches upstream physical units");
    double capped=touchpad_accel_profile_linear(&tp.base,nullptr,520*1000/25.4/1000000,0);
    check(near(capped,4.8*TP_MAGIC_SLOWDOWN),"touchpad fast-motion cap matches upstream curve");
    check(near(touchpad_accel_profile_linear(&tp.base,nullptr,10000,0),capped),"touchpad acceleration cannot grow beyond cap");
    for(double speed:{-1.0,-.5,0.0,.5,1.0}) {
        touchpad_accelerator_set_speed(&tp.base,speed);
        double factor=std::pow(speed+1,2.38)*.95+.05;
        check(near(touchpad_accel_profile_linear(&tp.base,nullptr,0,0),.3*TP_MAGIC_SLOWDOWN*factor),"all touchpad speed settings match upstream mapping");
        pointer_accelerator mouse; accelerator_set_speed(&mouse.base,speed);
        check(near(mouse.threshold,std::max(.0002,.0004-.00025*speed)),"mouse threshold follows upstream setting");
        check(near(pointer_accel_profile_linear(&mouse.base,nullptr,100,0),2+speed*1.5),"mouse maximum gain follows upstream setting");
    }
    for(unsigned dpi:{400u,800u,1000u,1600u,3200u}) {
        AdaptivePointer mouse(PointerDevice::Mouse,dpi);
        double slow=0,fast=0;
        mouse.reset(1000000);
        for(unsigned i=1;i<50;++i) slow=mouse.feed({dpi/1000.0,0},1000000+i*10000).x/(dpi/1000.0);
        mouse.reset(2000000);
        for(unsigned i=1;i<50;++i) fast=mouse.feed({dpi/100.0,0},2000000+i*1000).x/(dpi/100.0);
        check(fast>slow,"adaptive mouse gain increases with speed at low and high DPI");
    }
    AdaptivePointer a(PointerDevice::Mouse,1000),b(PointerDevice::Mouse,2000);
    a.reset(1000000); b.reset(1000000);
    for(unsigned i=1;i<150;++i) {
        auto x=a.feed({2.4,-1.3},1000000+i*8000),y=b.feed({4.8,-2.6},1000000+i*8000);
        check(near(x.x,y.x) && near(x.y,y.y),"mouse DPI normalization preserves physical motion");
    }
    TouchpadPointer cursor;
    check(cursor.feed({{1,40,30}},1000000).x==0,"landing anchors without cursor movement");
    auto movement=cursor.feed({{1,40.2,30}},1008000);
    check(movement.x>0 && movement.y==0,"single-finger motion accelerates along its axis");
    auto reversed=cursor.feed({{1,40.1,30}},1016000);
    check(reversed.x<0,"reversal changes pointer direction on the first reverse frame");
    check(cursor.feed({{1,40.1,30},{2,60,30}},1024000).x==0,"two fingers do not move the cursor");
    check(cursor.feed({{1,40.1,30}},1032000).x==0,"return from scrolling reanchors without a jump");
    check(cursor.feed({{2,80,40}},1040000).x==0,"replacement contact does not jump");
    check(cursor.feed({{2,100,40}},1200000).x==0,"long report gap reanchors");
    check(cursor.feed({{2,120.1,40}},1208000).x==0,"implausible contact jump is rejected");
    check(cursor.feed({{2,121,40}},1216000).x==0,"rejected jump forces a fresh anchor");
    AdaptivePointer invalid(PointerDevice::Touchpad);
    invalid.setSpeed(std::numeric_limits<double>::quiet_NaN());
    check(invalid.feed({std::numeric_limits<double>::infinity(),0},100).x==0,"invalid input cannot reach cursor output");
    check(invalid.feed({1,0},99).x==0,"backward timestamp resets instead of accelerating");
    std::printf("PASS: %u adaptive pointer checks; no OS input or settings used\n",checks);
}
