// Real frame routing; settings and wheel output are simulated by this fixture.
#include "input_test_fixture.h"
static bool waitMode(GestureMode mode) {
    auto until=GetTickCount64()+2000;
    do { if(profileRuntime.forWindow(nullptr).mode==mode) return true; Sleep(5); } while(GetTickCount64()<until);
    return false;
}
int main(int argc,char** argv) {
    using Mode=GestureMode; using Route=HorizontalGestureHandoff::Route;
    check(argc>1,"test directory supplied");
    std::string utf8=argv[1]; int length=MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,nullptr,0); std::wstring directory(length,L'\0'); MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),-1,directory.data(),length); directory.resize(length-1);
    auto path=directory+L"\\app-profiles-test.ini";
    GestureSettings config; config.defaults.mode=Mode::None;
    config.pointer={true,true,.35,-.25,800};
    config.applications={{L"Firefox.exe",{Mode::WindowsV1,{10,false,false}}},{L"C:\\Tést\\firefox.exe",{Mode::Touchscreen,{16.5,true,true}}}};
    check(profileForImage(config,L"C:\\Apps\\FIREFOX.EXE").mode==Mode::WindowsV1,"basename matching ignores case");
    check(profileForImage(config,L"c:\\tést\\FIREFOX.exe").mode==Mode::Touchscreen,"exact path takes precedence over basename rule");
    check(profileForImage(config,L"C:\\Apps\\other.exe").mode==Mode::None,"unlisted application uses default");
    check(saveGestureSettings(path,config),"atomic config save succeeds");
    auto loaded=loadGestureSettings(path); auto selected=profileForImage(loaded,L"C:\\Tést\\firefox.exe");
    check(loaded.pointer==config.pointer,"all pointer settings round-trip independently of application rules");
    check(loaded.applications.size()==2 && selected.mode==Mode::Touchscreen && selected.touch.pixelsPerMm==16.5 && selected.touch.invertHorizontal && selected.touch.lockVertical,"Unicode profiles and all touch settings round-trip");
    check(!saveGestureSettings(path+L"\\missing.ini",config),"failed save reports error");
    check(loadGestureSettings(path).applications.size()==2,"failed save leaves previous file intact");
    config.applications.clear(); config.defaults.mode=Mode::WindowsV1; check(saveGestureSettings(path,config),"initial runtime profile saved");
    check(profileRuntime.start(path),"background profile runtime starts"); check(waitMode(Mode::WindowsV1),"initial profile is available");
    check(profileRuntime.pointerOptions()==config.pointer,"pointer settings are available from the immutable runtime snapshot");
    prepare(); appProfiles=true; simulatedPan=handoffPanEnabled=false; profileLatched=false; momentumEnabled=false;
    sendFrame({{1,40,30},{2,60,30}},100); sendFrame({{1,40.1,30},{2,60.1,30}},180);
    check(simulatedPan && nativeGestures==1 && !injectedHorizontal,"real handler preserves the Windows driver threshold");
    config.defaults.mode=Mode::None; check(saveGestureSettings(path,config),"profile changes saved while a gesture is active");
    check(waitMode(Mode::None),"saved changes reload without restarting worker");
    sendFrame({{1,41,30},{2,61,30}},260);
    check(simulatedPan && profileRouter.selected().mode==Mode::WindowsV1,"in-progress gesture keeps its original policy after reload");
    sendFrame({},340); check(!simulatedPan && !profileLatched,"full release disables native pan and unlocks next profile selection");
    for(unsigned i=0;i<35;++i) sendFrame({{1,40+i*.4,30},{2,60+i*.4,30}},420+i*80);
    check(!simulatedPan && injectedHorizontal>0 && profileRouter.selected().mode==Mode::None,"next gesture uses reloaded none mode while preserving horizontal scrolling");
    sendFrame({},3300); profileRuntime.stop(); appProfiles=false;
    ProfileGestureRouter router;
    router.begin({Mode::WindowsV1,{}}); router.feed({{1,40,30},{2,60,30}});
    check(router.feed({{1,40,30.16},{2,60,30}}).route==Route::Custom,"v1 retains its original early vertical choice");
    check(router.feed({{1,41,30.16},{2,61,30}}).route==Route::Custom,"v1 custom choice stays fixed until full release");
    router.begin({Mode::Touchscreen,{}}); router.feed({{1,40,30},{2,60,30}});
    router.feed({{1,40,30.16},{2,60,30}});
    check(router.feed({{1,40.9,30.16},{2,60.9,30}}).route==Route::Touchscreen,"explicit touchscreen profile uses screen gesture after short vertical drift");
    router.begin({Mode::None,{}});
    check(router.feed({{1,40,30},{2,60,30}}).route==Route::Custom,"none mode has no direction-recognition waiting phase");
    router.feed({}); check(router.current()==Route::Idle,"none mode releases cleanly");
    std::printf("PASS: %u application profile checks; no real settings changes or input injection\n",checks);
}
