#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace esp::air_ghost_filter {
struct point {float x=0,y=0,z=0;};
inline bool finite(point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
// Only fresh position samples count. No game reads, allocation, wall-clock polling
// or velocity==0 shortcut. Frozen real aircraft on the ground may also be hidden.
class tracker {
public:
    static constexpr std::size_t sets=1024,ways=4,capacity=sets*ways;
    static constexpr double window_seconds=2.0,max_sample_gap=.5;
    static constexpr double epsilon_m=.03;
    static constexpr unsigned minimum_samples=4;
    void begin(std::uint64_t epoch,bool enabled){
        if(!_running||epoch!=_epoch||enabled!=_enabled){
            if(++_generation==0){_entries={};_generation=1;}
        }
        _epoch=epoch;_enabled=enabled;_running=enabled;
    }
    void suspend(){_running=false;}
    bool visible(std::uintptr_t id,bool airborne,point position,double sample_time){
        if(!airborne||!_running||!_enabled||!id)return true;
        if(!finite(position)||!std::isfinite(sample_time)||sample_time<=0){
            const auto slot=bucket(id)*ways;
            for(std::size_t i=0;i<ways;++i)if(_entries[slot+i].id==id)_entries[slot+i].generation=0;
            return true;
        }
        const auto base=bucket(id)*ways;
        entry* selected=nullptr;
        for(std::size_t i=0;i<ways;++i){
            auto& item=_entries[base+i];
            if(item.generation==_generation&&item.id==id){selected=&item;break;}
        }
        if(!selected){
            for(std::size_t i=0;i<ways;++i){
                auto& item=_entries[base+i];
                if(item.generation!=_generation){selected=&item;break;}
            }
            if(!selected){
                selected=&_entries[base];
                for(std::size_t i=1;i<ways;++i)if(_entries[base+i].last<selected->last)selected=&_entries[base+i];
            }
            restart(*selected,id,position,sample_time);return true;
        }
        auto& item=*selected;
        const double dx=double(position.x)-item.anchor.x,dy=double(position.y)-item.anchor.y,dz=double(position.z)-item.anchor.z;
        const bool moved=dx*dx+dy*dy+dz*dz>epsilon_m*epsilon_m;
        if(sample_time<item.last||sample_time-item.last>max_sample_gap||moved){
            restart(item,id,position,sample_time);return true;
        }
        if(sample_time==item.last)return !item.hidden;
        item.last=sample_time;
        if(item.samples<minimum_samples)++item.samples;
        item.hidden=item.samples>=minimum_samples&&sample_time-item.since>=window_seconds;
        return !item.hidden;
    }
private:
    struct entry {
        std::uintptr_t id=0;std::uint64_t generation=0;
        point anchor{};double since=0,last=0;unsigned samples=0;bool hidden=false;
    };
    std::array<entry,capacity> _entries{};
    std::uint64_t _epoch=0,_generation=0;bool _enabled=false,_running=false;
    static std::size_t bucket(std::uintptr_t id){
        std::uint64_t value=id;value^=value>>33;value*=0xff51afd7ed558ccdULL;value^=value>>33;
        return static_cast<std::size_t>(value)&(sets-1);
    }
    void restart(entry& item,std::uintptr_t id,point position,double time){
        item={};item.id=id;item.generation=_generation;item.anchor=position;
        item.since=time;item.last=time;item.samples=1;
    }
};
static_assert(sizeof(tracker)<=512*1024,"Air filter must remain bounded");
inline tracker current;
}
