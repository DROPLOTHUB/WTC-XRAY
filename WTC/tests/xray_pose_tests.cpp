#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>
#include "../src/features/esp/xray_pose.hpp"
#include "../src/game/offsets.hpp"
using namespace xray::pose;
struct memory {
    std::map<std::uintptr_t,std::vector<unsigned char>> fields;
    std::size_t calls{};bool partial{};
    template<class T> void put(std::uintptr_t address,const T& value) {
        const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
        fields[address]={bytes,bytes+sizeof(value)};
    }
    bool operator()(std::uintptr_t address,void* out,std::size_t size) {
        ++calls;const auto it=fields.find(address);
        if(it==fields.end() || it->second.size()!=size)return false;
        std::memcpy(out,it->second.data(),partial?size/2:size);
        return !partial;
    }
};
static void equal(point p,float x,float y,float z) {
    assert(std::fabs(p.x-x)<0.0001f && std::fabs(p.y-y)<0.0001f && std::fabs(p.z-z)<0.0001f);
}
static std::string file(const char* path) {
    std::ifstream input(std::filesystem::path(__FILE__).parent_path().parent_path()/path);
    assert(input);return {std::istreambuf_iterator<char>(input),{}};
}
int main() {
    constexpr std::uintptr_t unit=0x10000000,dm=0x20000000,tree=0x30000000;
    constexpr std::uintptr_t packed=0x40000000,bind=0x50000000;
    const layout fields{offsets::damage_model::m_ValidFlag,offsets::unit_offsets::modelTransform_offset,
        offsets::damage_model::geom_node_tree_base,offsets::damage_model::geom_node_tree_count};
    memory reader;
    reader.put(dm+fields.live_flag,std::uint8_t{1});
    reader.put(unit+fields.model_transform,tree);
    reader.put(tree+fields.packed_base,packed);
    reader.put(tree+fields.node_count,std::uint16_t{3}); // Any i32 read fails in this fake.
    auto ctx=read_context(unit,dm,fields,reader);
    assert(ctx.animated_ok && ctx.tree_count==3 && ctx.valid_flag==1);
    assert(reader.calls==4);
    const auto base_reader=reader;
    auto expect_context=[&](std::uintptr_t at,failure expected) {
        reader=base_reader;reader.fields.erase(at);
        const auto failed=read_context(unit,dm,fields,reader);
        assert(!failed.animated_ok && failed.reason==expected);
    };
    reader=base_reader;reader.fields.erase(dm+fields.live_flag);
    const auto missing_flag=read_context(unit,dm,fields,reader);
    assert(missing_flag.animated_ok && !missing_flag.valid_flag_read);
    expect_context(unit+fields.model_transform,failure::tree_read);
    expect_context(tree+fields.packed_base,failure::packed_read);
    expect_context(tree+fields.node_count,failure::count_read);
    reader=base_reader;reader.put(dm+fields.live_flag,std::uint8_t{0});
    assert(read_context(unit,dm,fields,reader).animated_ok); // Zero is not verified liveness either.
    reader=base_reader;reader.put(dm+fields.live_flag,std::uint8_t{64});
    ctx=read_context(unit,dm,fields,reader);
    assert(ctx.animated_ok && ctx.valid_flag==64 && ctx.valid_flag_read); // First live report.
    reader.put(dm+fields.live_flag,std::uint8_t{208});
    const auto other_vehicle=read_context(unit,dm,fields,reader);
    assert(other_vehicle.animated_ok && other_vehicle.valid_flag==208 && other_vehicle.valid_flag_read); // Second live report.
    reader=base_reader;reader.put(unit+fields.model_transform,std::uintptr_t{0});
    assert(read_context(unit,dm,fields,reader).reason==failure::no_tree);
    reader=base_reader;reader.put(tree+fields.packed_base,std::uintptr_t{0});
    assert(read_context(unit,dm,fields,reader).reason==failure::no_packed);
    reader=base_reader;reader.put(tree+fields.node_count,std::uint16_t{0});
    assert(read_context(unit,dm,fields,reader).reason==failure::no_nodes);
    assert(read_context(0,dm,fields,reader).reason==failure::no_unit);
    assert(read_context(unit,0,fields,reader).reason==failure::no_dm);
    reader=base_reader;reader.partial=true;
    const auto partial_context=read_context(unit,dm,fields,reader);
    assert(!partial_context.animated_ok && !partial_context.valid_flag_read);
    assert(partial_context.reason==failure::tree_read);
    reader=base_reader;

    // Different index spaces and exact byte counts; the fake forbids overreads.
    constexpr std::uint16_t tr=2,xf=7;
    std::array<float,16> live{1,0,0,0, 0,1,0,0, 0,0,1,0, 5,0,0,1};
    const std::array<float,12> rest{0,1,0, -1,0,0, 0,0,1, 10,20,30};
    reader.put(packed+tr*0x40,live);reader.put(bind+xf*0x30,rest);
    auto a=read_animated(ctx,tr,reader);auto b=read_bind(bind,xf,reader);
    assert(a.valid && b.valid && selected(a.valid,b.valid)==source::animated);
    equal(apply_cm<3>(b.f.data(),{2,3,4}),7,22,34);
    const std::array<float,16> same{0,1,0,0, -1,0,0,0, 0,0,1,0, 10,20,30,1};
    equal(apply_cm<4>(same.data(),{2,3,4}),7,22,34);
    equal(apply_cm<4>(a.f.data(),{2,0,0}),7,0,0);

    // Same pointers/indices between frames; only live WTM changes: yaw then pitch.
    live={0,1,0,0, -1,0,0,0, 0,0,1,0, 5,0,0,1};
    reader.put(packed+tr*0x40,live);
    const auto before=reader.calls;a=read_animated(ctx,tr,reader);
    assert(reader.calls==before+1 && a.valid);
    equal(apply_cm<4>(a.f.data(),{2,0,0}),5,2,0);
    // Unit rotation/translation composed once, after node-local -> model.
    const std::array<float,16> hull{0,1,0,0, -1,0,0,0, 0,0,1,0, 100,200,300,1};
    equal(apply_cm<4>(hull.data(),apply_cm<4>(a.f.data(),{2,0,0})),98,205,300);
    live={0,0,-1,0, -1,0,0,0, 0,1,0,0, 5,0,0,1};
    reader.put(packed+tr*0x40,live);a=read_animated(ctx,tr,reader);
    equal(apply_cm<4>(a.f.data(),{2,0,0}),5,0,-2);
    b=read_bind(bind,xf,reader);equal(apply_cm<3>(b.f.data(),{2,3,4}),7,22,34);
    assert(!read_animated(ctx,xf,reader).valid); // Never substitute xf for tr.

    const auto calls=reader.calls;
    assert(read_animated(ctx,3,reader).reason==failure::index_range);
    assert(read_animated(ctx,0xffff,reader).reason==failure::index_missing);
    assert(read_bind(bind,4096,reader).reason==failure::index_range);
    assert(read_bind(bind,0xffff,reader).reason==failure::index_missing);
    assert(reader.calls==calls);
    reader.put(bind,rest);reader.put(bind+4095*0x30,rest);
    assert(read_bind(bind,0,reader).valid && read_bind(bind,4095,reader).valid);

    reader.partial=true;a=read_animated(ctx,tr,reader);
    assert(!a.valid && a.reason==failure::matrix_read);
    for(float v:a.f)assert(v==0); // Partial failure must never publish old/partial pose.
    reader.partial=false;reader.fields.erase(packed+tr*0x40);
    assert(selected(read_animated(ctx,tr,reader).valid,b.valid)==source::bind);
    reader.fields.erase(bind+xf*0x30);
    assert(selected(read_animated(ctx,tr,reader).valid,read_bind(bind,xf,reader).valid)==source::none);
    reader.put(packed+tr*0x40,live);assert(read_animated(ctx,tr,reader).valid); // Next-frame recovery.
    for(float bad:{(std::numeric_limits<float>::quiet_NaN)(),(std::numeric_limits<float>::infinity)()}){
        auto corrupt=live;corrupt[0]=bad;reader.put(packed+tr*0x40,corrupt);
        assert(read_animated(ctx,tr,reader).reason==failure::matrix_invalid);
    }
    reader.put(packed+tr*0x40,std::array<float,16>{});
    assert(read_animated(ctx,tr,reader).reason==failure::matrix_invalid);
    std::array<float,16> scaled{.1f,0,0,0, 0,4,0,0, 0,0,1,0, 0,0,0,1};
    assert(valid_matrix<4>(scaled));
    char dummy{};const auto last_calls=reader.calls;
    assert(!read_at(reader,(std::numeric_limits<std::uintptr_t>::max)()-2,4,&dummy,1));
    assert(!read_at(reader,0,0,&dummy,1));assert(reader.calls==last_calls);

    assert(display_color(source::animated,0x81776655)==0x81776655);
    assert(display_color(source::bind,0x81776655)==0x8128aaff); // Keep alpha.
    {diagnostic_frame frame(unit);frame.data.ctx=ctx;frame.data.geometry_ready=true;
     record(frame.data,"gun_barrel_dm",tr,xf,source::bind,failure::index_range,failure::none,b.f.data(),true);}
    const auto report=diagnostic_report();
    assert(report.starts_with("XRAY.pose schema=2 "));
    assert(report.find("tr=2 xf=7 pose=bind animated=index_range")!=std::string::npos);
    assert(report.find("legacy_flag=64 flag_read=1 nodes=3 geometry=1 context=none")!=std::string::npos);
    // Secondary wiring guards: numerical tests above execute the production helper.
    const auto esp=file("src/features/esp/esp.hpp");
    for(const char* needle:{"xray::pose::apply_cm<4>","xray::pose::apply_cm<3>",
        "xray::pose::read_context", "xray::pose::read_animated", "xray::pose::read_bind",
        "s_animated_cache.clear();","s_bind_cache.clear();",
        "if (pose_source == xray::pose::source::none) continue;"})
        assert(esp.find(needle)!=std::string::npos);
    assert(file("src/gui/tactical_menu.hpp").find("::xray::pose::diagnostic_report()")!=std::string::npos);
    std::puts("PASS XRAY pose: live yaw/pitch, separate indices, 48/64-byte strides, context failures, safe fallback and diagnostics");
}
