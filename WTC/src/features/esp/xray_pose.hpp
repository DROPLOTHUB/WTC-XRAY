#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>

// 2.58+ contracts: bind = packed column-major 3x4; tree WTM = column-major 4x4.
// No addresses are inferred here: callers supply the verified offset table.
namespace xray::pose {
enum class failure { none, no_unit, no_dm, tree_read, no_tree,
    packed_read, count_read, no_packed, no_nodes, index_missing, index_range,
    matrix_read, matrix_invalid, not_attempted };
enum class source { none, animated, bind };
inline const char* name(failure value) {
    switch(value) {
#define XRAY_REASON(x) case failure::x: return #x;
        XRAY_REASON(none) XRAY_REASON(no_unit) XRAY_REASON(no_dm)
        XRAY_REASON(tree_read)
        XRAY_REASON(no_tree) XRAY_REASON(packed_read) XRAY_REASON(count_read)
        XRAY_REASON(no_packed) XRAY_REASON(no_nodes) XRAY_REASON(index_missing)
        XRAY_REASON(index_range) XRAY_REASON(matrix_read) XRAY_REASON(matrix_invalid)
        XRAY_REASON(not_attempted)
#undef XRAY_REASON
    }
    return "unknown";
}
inline const char* name(source value) {
    return value==source::animated?"animated":value==source::bind?"bind":"none";
}
struct layout { std::uintptr_t live_flag, model_transform, packed_base, node_count; };
struct skeleton_context {
    std::uintptr_t dm{}, tree{}, packed{};
    std::uint8_t valid_flag{};
    std::uint16_t tree_count{};
    bool valid_flag_read{}, animated_ok{};
    failure reason=failure::no_unit;
};
template<class Reader> bool read_at(Reader& read,std::uintptr_t base,
    std::uintptr_t offset,void* out,std::size_t size) {
    const auto max=(std::numeric_limits<std::uintptr_t>::max)();
    if(!base || !size || offset>max-base || size-1>max-(base+offset)) return false;
    return read(base+offset,out,size);
}
template<class Reader> skeleton_context read_context(std::uintptr_t unit,
    std::uintptr_t dm,const layout& offsets,Reader&& read) {
    skeleton_context ctx{};ctx.dm=dm;
    if(!unit) return ctx;
    ctx.reason=failure::no_dm;if(!dm) return ctx;
    // Live-game evidence: this legacy byte was 64 on a rendered active unit.
    // It is NOT a verified boolean. Preserve the observation, not an ==1 gate.
    // Existing unit/render filters plus checked tree/index/matrix reads gate poses.
    std::uint8_t legacy_flag{};
    ctx.valid_flag_read=read_at(read,dm,offsets.live_flag,&legacy_flag,sizeof(legacy_flag));
    if(ctx.valid_flag_read)ctx.valid_flag=legacy_flag;
    ctx.reason=failure::tree_read;
    if(!read_at(read,unit,offsets.model_transform,&ctx.tree,sizeof(ctx.tree)))return ctx;
    ctx.reason=failure::no_tree;if(!ctx.tree)return ctx;
    ctx.reason=failure::packed_read;
    if(!read_at(read,ctx.tree,offsets.packed_base,&ctx.packed,sizeof(ctx.packed)))return ctx;
    ctx.reason=failure::count_read;
    if(!read_at(read,ctx.tree,offsets.node_count,&ctx.tree_count,sizeof(ctx.tree_count)))return ctx;
    ctx.reason=failure::no_packed;if(!ctx.packed)return ctx;
    ctx.reason=failure::no_nodes;if(!ctx.tree_count)return ctx;
    ctx.reason=failure::none;ctx.animated_ok=true;return ctx;
}
struct point { float x{},y{},z{}; };
template<std::size_t Stride> point apply_cm(const float* f,point v) {
    static_assert(Stride==3 || Stride==4);
    return {f[0]*v.x+f[Stride]*v.y+f[2*Stride]*v.z+f[3*Stride],
            f[1]*v.x+f[Stride+1]*v.y+f[2*Stride+1]*v.z+f[3*Stride+1],
            f[2]*v.x+f[Stride+2]*v.y+f[2*Stride+2]*v.z+f[3*Stride+2]};
}
template<std::size_t Stride> bool valid_matrix(const std::array<float,16>& f) {
    double c[3][3]{};double norm[3]{};
    for(std::size_t col=0;col<4;++col)for(std::size_t row=0;row<3;++row) {
        const float value=f[col*Stride+row];if(!std::isfinite(value))return false;
        if(col<3){c[col][row]=value;norm[col]+=double(value)*value;}
    }
    // Permit scale/shear, but reject empty/collapsed bases. Padding is not used.
    const double det=c[0][0]*(c[1][1]*c[2][2]-c[1][2]*c[2][1])
        -c[1][0]*(c[0][1]*c[2][2]-c[0][2]*c[2][1])
        +c[2][0]*(c[0][1]*c[1][2]-c[0][2]*c[1][1]);
    const double scale=norm[0]*norm[1]*norm[2];
    return scale>0 && std::isfinite(scale) && std::isfinite(det) && det*det>1e-12*scale;
}
struct sample {std::array<float,16> f{};bool valid{};failure reason=failure::matrix_read;};
template<std::size_t Stride,class Reader> sample read_matrix(std::uintptr_t base,
    std::uint16_t index,std::uint32_t count,Reader&& read) {
    sample out{};
    if(index==0xffffu){out.reason=failure::index_missing;return out;}
    if(index>=count){out.reason=failure::index_range;return out;}
    std::array<float,16> bytes{};
    constexpr std::size_t size=Stride*4*sizeof(float);
    if(!read_at(read,base,std::uintptr_t(index)*size,bytes.data(),size))return out;
    if(!valid_matrix<Stride>(bytes)){out.reason=failure::matrix_invalid;return out;}
    out.f=bytes;out.valid=true;out.reason=failure::none;return out;
}
template<class Reader> sample read_animated(const skeleton_context& ctx,
    std::uint16_t tree_index,Reader&& read) {
    if(!ctx.animated_ok){sample out{};out.reason=ctx.reason;return out;}
    return read_matrix<4>(ctx.packed,tree_index,ctx.tree_count,read);
}
template<class Reader> sample read_bind(std::uintptr_t base,
    std::uint16_t transform_index,Reader&& read) {
    return read_matrix<3>(base,transform_index,4096u,read);
}
// A fallback is explicitly static; failure of BOTH readers is never identity.
inline source selected(bool animated,bool bind) {
    return animated?source::animated:bind?source::bind:source::none;
}
inline std::uint32_t display_color(source pose,std::uint32_t color) {
    return pose==source::bind ? (color&0xff000000u)|0x0028aaffu : color;
}
struct part_diagnostic {
    std::array<char,64> name{};
    std::uint16_t tree_index{},transform_index{};
    source used=source::none;
    failure animated=failure::not_attempted,bind=failure::not_attempted;
    std::array<float,16> matrix{};
};
struct diagnostics {
    std::uintptr_t unit{};skeleton_context ctx{};bool geometry_ready{};
    std::size_t animated{},bind{},missing{},count{};
    std::array<part_diagnostic,16> parts{};
    std::chrono::steady_clock::time_point sampled{};
};
inline std::mutex diagnostic_mutex;
inline diagnostics latest_diagnostics{};
struct diagnostic_frame {
    diagnostics data{};
    explicit diagnostic_frame(std::uintptr_t unit){data.unit=unit;}
    ~diagnostic_frame(){data.sampled=std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(diagnostic_mutex);latest_diagnostics=data;}
};
inline void record(diagnostics& data,const char* label,std::uint16_t tree_index,
    std::uint16_t transform_index,source used,failure animated,failure bind,
    const float* matrix,bool articulated) {
    if(used==source::animated)++data.animated;else if(used==source::bind)++data.bind;else ++data.missing;
    if(!articulated || data.count>=data.parts.size())return;
    auto& row=data.parts[data.count++];
    for(std::size_t i=0;label[i] && i+1<row.name.size();++i) {
        const unsigned char c=static_cast<unsigned char>(label[i]);
        row.name[i]=(c>=32 && c<127)?static_cast<char>(c):'?';
    }
    row.tree_index=tree_index;row.transform_index=transform_index;row.used=used;
    row.animated=animated;row.bind=bind;
    if(matrix)for(std::size_t i=0;i<16;++i)row.matrix[i]=matrix[i];
}
inline std::string diagnostic_report() {
    diagnostics data{};{std::lock_guard<std::mutex> lock(diagnostic_mutex);data=latest_diagnostics;}
    const auto age=data.sampled.time_since_epoch().count()?std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-data.sampled).count():-1;
    // Stable machine-readable field names, not translatable UI prose.
    std::ostringstream out;
    out<<"XRAY.pose schema=2 age_ms="<<age<<" unit=0x"<<std::hex<<data.unit<<" dm=0x"<<data.ctx.dm
       <<" tree=0x"<<data.ctx.tree<<" packed=0x"<<data.ctx.packed<<std::dec
       <<" legacy_flag="<<unsigned(data.ctx.valid_flag)<<" flag_read="<<data.ctx.valid_flag_read
       <<" nodes="<<data.ctx.tree_count
       <<" geometry="<<data.geometry_ready<<" context="<<name(data.ctx.reason)
       <<" animated="<<data.animated<<" bind="<<data.bind<<" missing="<<data.missing<<char(10);
    for(std::size_t i=0;i<data.count;++i){const auto& row=data.parts[i];
        out<<"XRAY.part name="<<row.name.data()<<" tr="<<row.tree_index<<" xf="<<row.transform_index
           <<" pose="<<name(row.used)<<" animated="<<name(row.animated)<<" bind="<<name(row.bind)<<" matrix=";
        for(float value:row.matrix)out<<value<<',';out<<char(10);
    }
    return out.str();
}
}
