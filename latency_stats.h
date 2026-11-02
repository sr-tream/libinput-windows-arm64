#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

// Fixed storage: collecting timing statistics does not allocate or retain input.
class LatencyStats {
    static constexpr std::array<uint64_t,17> bounds={1,2,5,10,20,50,100,200,500,1000,2000,5000,10000,20000,50000,100000,1000000};
    std::array<uint64_t,18> histogram{};
    uint64_t samples=0,total=0,maximum=0;
public:
    void add(uint64_t microseconds) {
        auto bucket=std::lower_bound(bounds.begin(),bounds.end(),microseconds)-bounds.begin();
        ++histogram[bucket]; ++samples; total+=microseconds; maximum=std::max(maximum,microseconds);
    }
    uint64_t count() const { return samples; }
    uint64_t max() const { return maximum; }
    double mean() const { return samples?static_cast<double>(total)/samples:0; }
    uint64_t percentileUpper(unsigned percent) const {
        if(!samples) return 0;
        uint64_t target=(samples*percent+99)/100,seen=0;
        for(size_t i=0;i<histogram.size();++i) {
            seen+=histogram[i]; if(seen>=target) return i<bounds.size()?bounds[i]:maximum;
        }
        return 0;
    }
};
