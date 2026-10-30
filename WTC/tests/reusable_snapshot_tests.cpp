#include "../src/core/reusable_snapshot.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <semaphore>
#include <string_view>
#include <thread>
#include <vector>

struct position {float x=0,y=0,z=0;bool operator==(const position&) const=default;};
struct frame_state {bool valid=false;std::size_t units=0;};
using snapshot=reusable_snapshot::snapshot<frame_state,position>;
struct allocation_stats {std::size_t calls=0,live=0,peak=0;};
struct counted_resource:std::pmr::memory_resource {
    allocation_stats stats;
    void* do_allocate(std::size_t bytes,std::size_t alignment) override {
        void* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);
        ++stats.calls;stats.live+=bytes;stats.peak=(std::max)(stats.peak,stats.live);return p;
    }
    void do_deallocate(void* p,std::size_t bytes,std::size_t alignment) override {
        stats.live-=bytes;std::pmr::new_delete_resource()->deallocate(p,bytes,alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {return this==&other;}
};
template<class T> struct counted_allocator {
    using value_type=T;
    allocation_stats* stats;
    explicit counted_allocator(allocation_stats* s):stats(s) {}
    template<class U> counted_allocator(const counted_allocator<U>& other):stats(other.stats) {}
    T* allocate(std::size_t count) {
        T* p=std::allocator<T>{}.allocate(count);++stats->calls;
        stats->live+=count*sizeof(T);stats->peak=(std::max)(stats->peak,stats->live);return p;
    }
    void deallocate(T* p,std::size_t count) {stats->live-=count*sizeof(T);std::allocator<T>{}.deallocate(p,count);}
    template<class U> bool operator==(const counted_allocator<U>& other) const {return stats==other.stats;}
};
struct previous_snapshot {
    std::uint64_t frame_id=0;
    frame_state frame{};
    using entry=std::pair<const std::uintptr_t,position>;
    std::unordered_map<std::uintptr_t,position,std::hash<std::uintptr_t>,std::equal_to<std::uintptr_t>,counted_allocator<entry>> positions;
    explicit previous_snapshot(allocation_stats* stats):positions(0,std::hash<std::uintptr_t>{},std::equal_to<std::uintptr_t>{},counted_allocator<entry>(stats)) {}
};
static std::uintptr_t key(std::size_t i){return 0x100000+i*0x2000;}
static position expected(std::uint64_t id,std::size_t i){return {float(id%1024),float(i),float((i*7)%100)};}
template<class Snapshot> void fill(Snapshot& s,std::uint64_t id,std::size_t units) {
    s.frame_id=id;s.frame.valid=true;s.frame.units=units;s.positions.reserve(units);
    for(std::size_t i=0;i<units;++i)s.positions[key(i)]=expected(id,i);
}
template<class Snapshot> void check(const Snapshot& s) {
    assert(s.frame.valid && s.positions.size()==s.frame.units);
    for(std::size_t i=0;i<s.frame.units;++i){const auto p=s.positions.find(key(i));assert(p!=s.positions.end() && p->second==expected(s.frame_id,i));}
}
template<class Snapshot> std::uint64_t consume(const Snapshot& s) {
    std::uint64_t sum=0;
    for(std::size_t i=0;i<s.frame.units;++i)sum+=static_cast<std::uint64_t>(s.positions.find(key(i))->second.x);
    for(const auto& [address,p]:s.positions)sum+=address+static_cast<std::uint64_t>(p.y);
    return sum;
}
static void lifetime_tests() {
    reusable_snapshot::pool<snapshot> pool;
    std::vector<std::shared_ptr<const snapshot>> retained;
    for(std::uint64_t i=1;i<=12;++i){auto writable=pool.acquire();writable->reset();fill(*writable,i,32);retained.push_back(writable);}
    for(std::size_t i=0;i<retained.size();++i){check(*retained[i]);assert(retained[i]->frame_id==i+1);for(std::size_t j=0;j<i;++j)assert(retained[i].get()!=retained[j].get());}
    const auto first=retained[0].get();retained[0].reset();
    auto writable=pool.acquire();assert(writable.get()==first);writable->reset();
    assert(writable->positions.empty() && !writable->frame.valid && writable->frame_id==0 && writable->captured_at.time_since_epoch().count()==0);
    for(std::size_t i=1;i<retained.size();++i)check(*retained[i]);
    fill(*writable,100,4);writable->positions[key(0)]={9,8,7};assert(writable->positions.size()==4);
    assert(writable->positions.at(key(0))==position(9,8,7));
    writable->reset();fill(*writable,101,0);check(*writable);
    std::shared_ptr<const snapshot> survivor;
    {reusable_snapshot::pool<snapshot> temporary;auto value=temporary.acquire();value->reset();fill(*value,900,40);survivor=value;}
    check(*survivor); // allocator lives with the snapshot, not its pool
}
static void steady_state_tests() {
    counted_resource memory;
    {reusable_snapshot::pool<snapshot> pool(&memory);
     std::array<std::shared_ptr<const snapshot>,3> published{};
     for(int frame=0;frame<12;++frame){auto w=pool.acquire();w->reset();fill(*w,frame,512);published[frame%3]=std::move(w);}
     const auto before=memory.stats.calls;
     for(int frame=12;frame<612;++frame){auto w=pool.acquire();w->reset();fill(*w,frame,512);check(*w);published[frame%3]=std::move(w);}
     assert(memory.stats.calls==before); // zero map heap allocations after warmup
     for(auto& slot:published)slot.reset(); // reconnect/reset drops publication, not retained readers
     auto w=pool.acquire();w->reset();fill(*w,700,4096);check(*w);w->reset();assert(w->positions.empty());
    }
    assert(memory.stats.live==0); // resource destruction releases all retained allocation blocks
}
static void concurrent_reader_test() {
    reusable_snapshot::pool<snapshot> pool;
    std::array<std::atomic<std::shared_ptr<const snapshot>>,3> published;
    std::atomic<std::size_t> front=0;
    {auto w=pool.acquire();w->reset();fill(*w,1,64);published[0].store(std::move(w));}
    std::atomic<bool> done=false;std::binary_semaphore started(0);
    std::thread reader([&]{auto saved=published[0].load();check(*saved);started.release();
        while(!done.load(std::memory_order_acquire)) {
            auto s=published[front.load(std::memory_order_acquire)].load();
            if(s)check(*s);check(*saved);
        }
        check(*saved);
    });
    started.acquire();
    for(std::uint64_t frame=2;frame<2002;++frame){
        if(frame%37==0)for(auto& slot:published)slot.store({}); // reconnect-style clear while readers retain a frame
        auto w=pool.acquire();w->reset();fill(*w,frame,64);
        const auto index=static_cast<std::size_t>(frame%3);
        published[index].store(std::move(w));front.store(index,std::memory_order_release);
    }
    done.store(true,std::memory_order_release);reader.join();
}
static void integration_tests() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    const auto text=[](const std::filesystem::path& p) {
        std::ifstream in(p);assert(in);return std::string(std::istreambuf_iterator<char>(in),{});
    };
    const auto misc=text(root/"features/misc/misc.hpp"),radar=text(root/"features/esp/radar.hpp");
    assert(misc.find("reusable_snapshot::snapshot<frame_state_t,vec3_t>")!=std::string::npos);
    assert(misc.find("pool<frame_snapshot_t,frame_snapshot_slots_count+1>")!=std::string::npos);
    assert(misc.find("next_buffer->reset();")!=std::string::npos);
    assert(misc.find("publish_frame_snapshot(std::shared_ptr<const frame_snapshot_t> snapshot)")!=std::string::npos);
    assert(misc.find("std::make_shared< frame_snapshot_t >")==std::string::npos);
    std::size_t count=0,offset=0;
    while((offset=misc.find("publish_frame_snapshot(std::move(next_buffer));",offset))!=std::string::npos){++count;++offset;}
    assert(count==2); // both out-of-battle and successful publication move the same owned buffer
    assert(radar.find("template<class Allocator>")!=std::string::npos);
}
struct measurement {double us=0;std::size_t allocations=0,peak=0;std::uint64_t checksum=0;};
static measurement old_run(int frames,std::size_t units) {
    allocation_stats stats;measurement out;
    const auto start=std::chrono::steady_clock::now();
    {std::array<std::shared_ptr<const previous_snapshot>,3> ring;
     for(int f=0;f<frames;++f){auto s=std::make_shared<previous_snapshot>(&stats);fill(*s,f,units);out.checksum+=consume(*s);ring[f%3]=std::move(s);}}
    out.us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/frames;
    out.allocations=stats.calls;out.peak=stats.peak;assert(stats.live==0);return out;
}
static measurement new_run(int frames,std::size_t units) {
    counted_resource memory;measurement out;
    const auto start=std::chrono::steady_clock::now();
    {reusable_snapshot::pool<snapshot> pool(&memory);std::array<std::shared_ptr<const snapshot>,3> ring;
     for(int f=0;f<frames;++f){auto s=pool.acquire();s->reset();fill(*s,f,units);out.checksum+=consume(*s);ring[f%3]=std::move(s);}}
    out.us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/frames;
    out.allocations=memory.stats.calls;out.peak=memory.stats.peak;assert(memory.stats.live==0);return out;
}
static void benchmark() {
    constexpr int frames=1000;
    for(const std::size_t units:{64u,256u,1024u}) {
        std::vector<double> old_times,new_times;measurement old_result,new_result;
        for(int repeat=0;repeat<7;++repeat){
            if(repeat%2){new_result=new_run(frames,units);old_result=old_run(frames,units);}
            else {old_result=old_run(frames,units);new_result=new_run(frames,units);}
            assert(old_result.checksum==new_result.checksum);
            old_times.push_back(old_result.us);new_times.push_back(new_result.us);
        }
        std::sort(old_times.begin(),old_times.end());std::sort(new_times.begin(),new_times.end());
        std::printf("BENCH units=%zu frames=%d median7_us/frame old=%.3f new=%.3f map_heap_calls=%zu->%zu map_peak_bytes=%zu->%zu checksum=%llu\n",
            units,frames,old_times[3],new_times[3],old_result.allocations,new_result.allocations,old_result.peak,new_result.peak,static_cast<unsigned long long>(new_result.checksum));
    }
}
int main(int argc,char** argv) {
    lifetime_tests();steady_state_tests();concurrent_reader_test();integration_tests();
    std::puts("PASS immutable retained snapshots, spill when busy, reset, ownership lifetime, 4096 units, concurrent reader, zero warmed map heap calls");
    if(argc>1 && std::string_view(argv[1])=="--benchmark")benchmark();
}
