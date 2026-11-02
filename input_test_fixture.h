#pragma once
// Exercise the real frame handler with simulated settings and input delivery.
// No devices are registered, OS settings changed, or input injected.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
static bool simulatedPan=false;
static unsigned panWrites=0,injectedHorizontal=0,injectedVertical=0;
static BOOL WINAPI fakeSystemParametersInfoW(UINT action,UINT size,PVOID value,UINT) {
    if(size!=sizeof(TOUCHPAD_PARAMETERS_V1)) return FALSE;
    auto* settings=static_cast<TOUCHPAD_PARAMETERS_V1*>(value);
    if(action==SPI_GETTOUCHPADPARAMETERS) { *settings={}; settings->versionNumber=1; settings->panEnabled=simulatedPan; return TRUE; }
    if(action==SPI_SETTOUCHPADPARAMETERS) { simulatedPan=settings->panEnabled!=0; ++panWrites; return TRUE; }
    return FALSE;
}
static UINT WINAPI fakeSendInput(UINT count,LPINPUT input,int) {
    for(UINT i=0;i<count;++i) {
        if(input[i].mi.dwFlags&MOUSEEVENTF_HWHEEL) ++injectedHorizontal;
        if(input[i].mi.dwFlags&MOUSEEVENTF_WHEEL) ++injectedVertical;
    }
    return count;
}
#define SystemParametersInfoW fakeSystemParametersInfoW
#define SendInput fakeSendInput
#define wmain libinput_application_wmain
#include "touchpad.cpp"
#undef wmain
#undef SendInput
#undef SystemParametersInfoW
#include <fstream>
#include <sstream>
static unsigned checks=0;
static void check(bool condition,const char* name) {
    ++checks;
    if(!condition) { std::fprintf(stderr,"FAIL: %s\n",name); std::exit(1); }
}
static void prepare() {
    cancelMomentum(true); processor.reset(); processorNeedsReset=false;
    profileLatched=false; handoffPanEnabled=false;
    nativeGestures=customGestures=panWrites=injectedHorizontal=injectedVertical=0;
    simulatedPan=false; runMode=live=true; panChanged=false; lastReportWall=scanEpoch=previousScan=0;
    xUnit=yUnit=0.1; device.fields.clear();
    auto add=[](USHORT collection,USHORT page,USHORT usage) {
        Field f{}; f.index=static_cast<USHORT>(device.fields.size()+1); f.collection=collection; f.page=page; f.usage=usage; f.report=84; device.fields.push_back(f);
    };
    add(0,0x0d,0x54); add(0,0x0d,0x56); add(0,9,1);
    for(USHORT slot=1;slot<=5;++slot) { add(slot,0x0d,0x42); add(slot,0x0d,0x47); add(slot,0x0d,0x51); add(slot,1,0x30); add(slot,1,0x31); }
}
static void sendFrame(const std::vector<Contact>& contacts,unsigned scan,bool clicked=false) {
    std::map<USHORT,ULONG> values;
    for(const auto& f:device.fields) {
        ULONG value=0;
        if(f.collection==0) {
            if(f.page==0x0d && f.usage==0x54) value=static_cast<ULONG>(contacts.size());
            if(f.page==0x0d && f.usage==0x56) value=scan;
            if(f.page==9) value=clicked;
        } else if(f.collection<=contacts.size()) {
            const auto& contact=contacts[f.collection-1];
            if(f.page==0x0d && (f.usage==0x42 || f.usage==0x47)) value=1;
            if(f.page==0x0d && f.usage==0x51) value=contact.id;
            if(f.page==1 && f.usage==0x30) value=static_cast<ULONG>(std::lround(contact.x*10));
            if(f.page==1 && f.usage==0x31) value=static_cast<ULONG>(std::lround(contact.y*10));
        }
        values[f.index]=value;
    }
    frame(values,84);
}
