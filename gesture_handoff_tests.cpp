#include "gesture_handoff.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
static unsigned checks=0;
static void check(bool value,const char* name) { ++checks; if(!value) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); } }
int main(int argc,char** argv) {
    using Route=HorizontalGestureHandoff::Route;
    const std::vector<Contact> origin{{1,40,30},{2,60,30}};
    for(double sign:{-1.0,1.0}) {
        HorizontalGestureHandoff router;
        check(router.feed(origin).route==Route::Pending,"two fingers wait for movement");
        check(router.feed({{2,60+sign*.1,30},{1,40+sign*.1,30}}).route==Route::Native,"horizontal swipe uses the original threshold regardless of contact order");
        check(router.feed({{1,40,35},{2,60,35}}).route==Route::Native,"native gesture retains its owner on direction changes");
        check(router.feed({{1,40,35}}).route==Route::Native,"lifting one finger preserves ownership");
        check(router.feed({}).lifted && router.current()==Route::Idle,"all fingers lifted reset ownership");
    }
    HorizontalGestureHandoff router;
    router.feed(origin);
    check(router.feed({{1,40,30.02},{2,60,30.02}}).route==Route::Pending,"landing noise stays pending");
    check(router.feed({{1,40,30.1},{2,60,30.1}}).route==Route::Custom,"vertical scroll retains its early start");
    check(router.feed({{1,41,30.1},{2,61,30.1}}).route==Route::Custom,"vertical choice cannot become a navigation gesture before release");
    router.feed({}); router.feed(origin);
    check(router.feed({{1,39.8,30},{2,60.2,30}}).route==Route::Custom,"pinch stays with the zoom path");
    router.reset(); router.feed(origin);
    check(router.feed(origin,true).route==Route::Custom,"click cannot become navigation");
    router.reset(); router.feed(origin);
    check(router.feed({{1,40,30},{2,60,30},{3,70,30}}).route==Route::Custom,"extra fingers cannot become navigation");
    router.reset(); router.feed(origin);
    check(router.feed({{1,40.1,30.1},{2,60.1,30.1}}).route==Route::Pending,"small diagonal remains undecided");
    check(router.feed({{1,40.2,30.18},{2,60.2,30.18}}).route==Route::Native,"larger diagonal selects its dominant axis");
    if(argc>1) {
        std::ifstream file(argv[1]); check(file.is_open(),"recorded contacts file opens");
        HorizontalGestureHandoff replay; Route locked=Route::Idle;
        std::string line; unsigned records=0;
        while(std::getline(file,line)) {
            std::istringstream row(line); uint64_t time=0; unsigned count=0; row>>time>>count;
            std::vector<Contact> contacts;
            for(unsigned i=0;i<count;++i) { Contact contact{}; row>>contact.id>>contact.x>>contact.y; contacts.push_back(contact); }
            auto result=replay.feed(contacts);
            if(contacts.empty()) locked=Route::Idle;
            if(result.decided) locked=result.route;
            if(locked==Route::Native || locked==Route::Custom) check(result.route==locked,"recorded gesture keeps its owner until full release");
            ++records;
        }
        check(records>10000,"real contact recording is replayed");
        std::printf("REPLAY: %u reports\n",records);
    }
    std::printf("PASS: %u Windows driver gesture checks\n",checks);
}