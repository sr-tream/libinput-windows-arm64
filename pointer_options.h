#pragma once
struct PointerSettings {
    bool enabled=false,mouseAdaptive=false;
    double touchpadSpeed=0,mouseSpeed=0;
    unsigned mouseDpi=1000;
    bool operator==(const PointerSettings&) const=default;
};
