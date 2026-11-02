#include "libinput_scroll.h"
#include "momentum.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
static int checks;
static void check(bool b,const char* name) { ++checks; if(!b) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); } }
int main(int argc,char** argv) {
    MomentumScroll momentum;
    for(unsigned i=0;i<30;++i) momentum.observe({0,1},100000+i*10000);
    check(momentum.release(400000,1000000),"fling starts after moving fingers lift");
    double momentumTravel=0;
    for(unsigned i=1;i<400 && momentum.active();++i) momentumTravel+=momentum.tick(1000000+i*8000).y;
    check(momentumTravel>44 && momentumTravel<51,"Firefox friction momentum has bounded travel");
    check(!momentum.active(),"momentum comes to rest");
    momentum.reset();
    for(unsigned i=0;i<30;++i) momentum.observe({0,1},100000+i*10000);
    check(!momentum.release(600000,1000000),"pause before lift does not trigger momentum");
    momentum.reset();
    for(unsigned i=0;i<30;++i) momentum.observe({1,0},100000+i*10000);
    momentum.release(400000,1000000); momentum.cancelActive();
    auto stopped=momentum.tick(1008000);
    check(stopped.x==0 && stopped.y==0,"explicit cancellation stops momentum immediately");
    momentum.release(400000,1000000); auto delayed=momentum.tick(1300000);
    check(delayed.x==0 && delayed.y==0 && !momentum.active(),"stalled callback cannot emit a large catch-up jump");
    MomentumScroll touchMomentum;
    for(unsigned i=0;i<30;++i) touchMomentum.updateContacts(2,false,{0,1},100000+i*10000,700000+i*10000);
    auto release=touchMomentum.updateContacts(0,false,{},400000,1000000);
    check(release.started && touchMomentum.active(),"two-finger lift starts momentum through contact policy");
    auto one=touchMomentum.updateContacts(1,false,{},410000,1010000);
    check(!one.started && touchMomentum.active(),"one-finger cursor contact preserves momentum");
    auto moving=touchMomentum.tick(1016000);
    check(moving.y>0,"momentum continues while a cursor finger is down");
    one=touchMomentum.updateContacts(1,false,{},650000,1250000);
    check(!one.started && touchMomentum.active(),"one-finger input after an idle gap preserves momentum");
    auto oneLift=touchMomentum.updateContacts(0,false,{},660000,1260000);
    check(!oneLift.started && touchMomentum.active(),"cursor-finger lift does not restart or cancel a fling");
    auto two=touchMomentum.updateContacts(2,false,{},670000,1270000);
    check(two.newTwoFingerTouch && !touchMomentum.active(),"stationary next two-finger touch immediately stops momentum");
    auto afterStop=touchMomentum.tick(1278000);
    check(afterStop.x==0 && afterStop.y==0,"stopped momentum emits no further scroll");
    auto noRestart=touchMomentum.updateContacts(0,false,{},680000,1280000);
    check(!noRestart.started && !touchMomentum.active(),"lifting stationary stop fingers cannot revive old momentum");
    touchMomentum.reset();
    for(unsigned i=0;i<30;++i) touchMomentum.updateContacts(2,false,{1,0},100000+i*10000,700000+i*10000);
    auto partialLift=touchMomentum.updateContacts(1,false,{},400000,1000000);
    check(!partialLift.started && !touchMomentum.active(),"partial two-to-one lift waits for full release");
    release=touchMomentum.updateContacts(0,false,{},410000,1010000);
    check(release.started && touchMomentum.active(),"two-to-one-to-zero release starts momentum once");
    auto noRepeatedRelease=touchMomentum.updateContacts(0,false,{},420000,1020000);
    check(!noRepeatedRelease.started && touchMomentum.active(),"repeated empty frames do not restart momentum");
    touchMomentum.updateContacts(3,false,{},430000,1030000);
    check(!touchMomentum.active(),"touching with at least two fingers stops the old fling");
    for(unsigned count: {0u,1u,2u,3u}) {
        touchMomentum.reset();
        for(unsigned i=0;i<30;++i) touchMomentum.updateContacts(2,false,{0,1},100000+i*10000,700000+i*10000);
        release=touchMomentum.updateContacts(0,false,{},400000,1000000);
        check(release.started,"click regression fixture starts momentum");
        auto click=touchMomentum.updateContacts(count,true,{},410000,1010000);
        check(!click.started && !touchMomentum.active(),"physical click stops momentum regardless of contact count");
        auto clickLift=touchMomentum.updateContacts(0,false,{},420000,1020000);
        check(!clickLift.started && !touchMomentum.active(),"lifting after a physical click cannot revive momentum");
    }
    touchMomentum.reset();
    for(unsigned i=0;i<30;++i) touchMomentum.updateContacts(2,false,{0,1},100000+i*10000,700000+i*10000);
    touchMomentum.updateContacts(1,false,{},400000,1000000);
    touchMomentum.updateContacts(1,true,{},405000,1005000);
    auto clickedPartialLift=touchMomentum.updateContacts(0,false,{},410000,1010000);
    check(!clickedPartialLift.started && !touchMomentum.active(),"click during a partial lift discards pending fling velocity");
    WheelAccumulator wheel;
    auto up=wheel.feed({0,-1},2,false);
    check(up.x==0 && up.y==2,"default fingers up maps to positive vertical wheel");
    auto left=wheel.feed({-1,0},2,false);
    check(left.x==-2 && left.y==0,"default fingers left maps to negative horizontal wheel");
    auto reverse=wheel.feed({-1,-1},2,true);
    check(reverse.x==2 && reverse.y==-2,"reverse preference flips both wheel directions");
    for(bool horizontal: {false,true}) {
        for(double firstDirection: {-1.0,1.0}) {
            ScrollProcessor reversing; WheelAccumulator reversingWheel;
            double position=0;
            for(unsigned i=0;i<80;++i) {
                position=firstDirection*i*0.1;
                std::vector<Contact> fingers=horizontal?std::vector<Contact>{{1,50+position,30},{2,65+position,30}}:std::vector<Contact>{{1,30,40+position},{2,45,40+position}};
                auto value=reversing.feed(fingers,100000+i*7000);
                reversingWheel.feed(value,3.5,true);
            }
            position-=firstDirection*0.1;
            std::vector<Contact> fingers=horizontal?std::vector<Contact>{{1,50+position,30},{2,65+position,30}}:std::vector<Contact>{{1,30,40+position},{2,45,40+position}};
            auto value=reversing.feed(fingers,660000);
            auto emitted=reversingWheel.feed(value,3.5,true);
            check((horizontal?value.x:value.y)*firstDirection<0,"same-axis reversal changes filtered sign on its first report");
            check((horizontal?emitted.x:-emitted.y)*firstDirection>0,"same-axis reversal reaches wheel output on its first report");
        }
    }
    wheel.reset();
    auto fraction=wheel.feed({0.2,0.2},2,false);
    check(fraction.x==0 && fraction.y==0,"sub-unit motion is retained");
    wheel.feed({0.2,0.2},2,false); fraction=wheel.feed({0.2,0.2},2,false);
    check(fraction.x==1 && fraction.y==-1,"fractional motion accumulates without truncation loss");
    touchpad_accelerator filter;
    device_float_coords raw{1000/25.4,1000/25.4};
    auto d=touchpad_constant_filter(&filter.base,&raw,nullptr,0);
    check(std::fabs(d.x-0.9*0.2968*1000/25.4)<1e-10,"libinput constant filter scale");
    tp_dispatch tp;
    for(unsigned i=0;i<30;++i) {
        device_float_coords mm{0.001,0.2}; normalized_coords delta{mm.x,mm.y};
        tp_gesture_apply_scroll_constraints(&tp,&mm,&delta,1000+i*8000);
        check(delta.x==0 && delta.y>0,"vertical scroll rejects lateral drift");
    }
    tp={};
    for(unsigned i=0;i<30;++i) {
        device_float_coords mm{0.2,0.001}; normalized_coords delta{mm.x,mm.y};
        tp_gesture_apply_scroll_constraints(&tp,&mm,&delta,1000+i*8000);
        check(delta.y==0 && delta.x>0,"horizontal scroll rejects vertical drift");
    }
    tp={};
    for(unsigned i=0;i<30;++i) {
        device_float_coords mm{0.2,0.2}; normalized_coords delta{mm.x,mm.y};
        tp_gesture_apply_scroll_constraints(&tp,&mm,&delta,1000+i*8000);
    }
    check(tp.scroll.active.h && tp.scroll.active.v,"deliberate diagonal unlocks both axes");
    ScrollProcessor p;
    for(unsigned i=0;i<40;++i) {
        auto delta=p.feed({{1,30,30+i*.3}},1000+i*8000);
        check(delta.x==0 && delta.y==0,"one finger cannot scroll");
    }
    if(argc>1) {
        std::ifstream f(argv[1]); check(f.is_open(),"recorded contacts file opens");
        std::string line; ScrollProcessor replay; unsigned records=0,outputs=0; double travel=0;
        while(std::getline(f,line)) {
            std::istringstream row(line); uint64_t time; unsigned count; row>>time>>count;
            std::vector<Contact> contacts;
            for(unsigned i=0;i<count;++i) { Contact c{}; row>>c.id>>c.x>>c.y; contacts.push_back(c); }
            auto out=replay.feed(contacts,time);
            if(count!=2) check(out.x==0 && out.y==0,"recorded non-scroll gesture produces no scroll");
            if(out.x || out.y) ++outputs;
            travel+=std::hypot(out.x,out.y); ++records;
        }
        check(records>10000,"real device trace has sufficient reports");
        check(outputs>100,"real device trace produces scroll output");
        std::printf("REPLAY: reports=%u output_frames=%u normalized_travel=%.2f\n",records,outputs,travel);
    }
    double total=0;
    for(unsigned i=0;i<40;++i) {
        auto delta=p.feed({{1,30,30+i*.3},{2,45,30+i*.3}},500000+i*8000);
        check(delta.x==0,"two finger vertical constraints"); total+=delta.y;
    }
    check(total>50 && total<150,"two finger scroll produces scaled output");
    auto lifted=p.feed({},900000);
    check(lifted.x==0 && lifted.y==0,"lift stops scroll");
    auto resumed=p.feed({{3,80,50},{4,95,50}},910000);
    check(resumed.x==0 && resumed.y==0,"new contact IDs do not jump");
    p.reset();
    for(unsigned i=0;i<40;++i) {
        auto delta=p.feed({{1,30-i*.3,30},{2,45+i*.3,30}},1000000+i*8000);
        check(delta.x==0 && delta.y==0,"pinch stays with Windows");
    }
    p.reset();
    for(unsigned i=0;i<40;++i) {
        auto delta=p.feed({{1,30,30+i*.3},{2,45,30+i*.3},{3,60,30+i*.3}},1500000+i*8000);
        check(delta.x==0 && delta.y==0,"three finger gesture stays with Windows");
    }
    p.reset();
    for(unsigned i=0;i<40;++i) {
        auto delta=p.feed({{1,30,30+i*.3},{2,45,30+i*.3}},2000000+i*8000,true);
        check(delta.x==0 && delta.y==0,"physical click prevents scrolling while dragging");
    }
    std::printf("PASS: %d checks\n",checks);
}
