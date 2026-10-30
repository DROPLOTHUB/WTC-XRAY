#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>
#include <algorithm>

namespace projectile_ecs {

static_assert(sizeof(std::uintptr_t) == 8);

struct Vec3 { float x{}, y{}, z{}; };
static_assert(sizeof(Vec3) == 12);
struct Layout {
    std::uintptr_t manager_rva = 0x7E89BE8;
    std::uintptr_t rocket_selector_rva = 0x7A24860;
    std::uintptr_t bomb_selector_rva = 0x7A23320;
    std::uintptr_t data_offset = 0x178;
    std::uintptr_t index_offset = 0x570;
    std::uintptr_t position_offset = 0x2C0;
    std::uintptr_t velocity_offset = 0x2DC;
    std::uintptr_t owner_offset = 0x58;
    std::uintptr_t missile_name_offset = 0x720;
    std::uintptr_t bomb_name_offset = 0x718;
    std::uintptr_t missile_name_offset_alt = 0;   // fallback opt-in, not used by current offsets
    std::uintptr_t bomb_name_offset_alt = 0;
    std::uintptr_t missile_name_text_offset = 0x870;
    std::uintptr_t missile_name_text_offset_alt = 0x50; // only if the supplied field fails
    std::uintptr_t bomb_name_text_offset = 0x10;
    std::uintptr_t max_reads = 4096;
    bool aam_only = false;   // valor por defecto para nombres nuevos
    bool (*name_allowed)(const char*, void*) = nullptr;
    void* name_context = nullptr;
};
struct Candidate {
    std::uintptr_t source_id{}, owner{};
    Vec3 position{}, velocity{};
    float speed{};
    bool is_bomb{};
    char name[48]{};
};
enum class Failure { none, address, read, selector, layout, descriptor, budget, changed };
struct Diagnostics {
    Failure failure = Failure::none;
    std::uintptr_t manager{}, record{};
    std::uintptr_t data_table{}, index_table{};
    std::uint8_t required{}, optional{};
    std::uint16_t subcount{};
    std::uint16_t offset0{}, offset1{};
    std::uintptr_t name_container{}, name_text{};
    std::uintptr_t name_probe_source{}, name_probe_container{}, name_probe_text{};
    std::uint32_t name_probes{}, name_probe_stage{}; // 2=container read, 3=pointer, 5=text read, 6=pointer, 7=string read, 8=empty, 9=valid
    char raw_name[64]{};
    std::uint32_t names_read{};
    std::uintptr_t name_offset{}, name_text_offset{};
    std::array<std::uint8_t, 32> header{};
    std::array<std::uint8_t, 32> descriptor{};
    std::uint32_t raw_selector{}, selector{}, sublist_count{}, sublists_visited{}, rows_seen{};
    std::uint32_t active_rows{}, accepted{}, rejected{}, read_calls{}, truncated{};
    std::uint32_t flares{};
    std::uint32_t caliber_ignored{};
    std::uint32_t not_aam{};
    char not_aam_name[64]{};
};

// Bengalas y seÃ±uelos: cualquier proyectil cuyo nombre contenga "flare"
// (insensible a mayusculas) se descarta antes de leer su fisica, de modo que
// no consume lecturas ni genera trayectoria ni marcador.
inline bool name_has_flare(const char* name)
{
    if (!name) return false;
    static constexpr char token[] = "flare";
    for (const char* p = name; *p != '\0'; ++p) {
        std::size_t i = 0;
        while (token[i] != '\0' && p[i] != '\0') {
            const char c = (p[i] >= 'A' && p[i] <= 'Z')
                ? static_cast<char>(p[i] + ('a' - 'A')) : p[i];
            if (c != token[i]) break;
            ++i;
        }
        if (token[i] == '\0') return true;
    }
    return false;
}

// El ECS "rocket" tambien contiene proyectiles de canones como 100mm_...
// Por peticion del usuario: cualquier "mm" (sin distinguir mayusculas)
// no se registra ni se dibuja, aunque estuviera activado manualmente.
inline bool name_has_mm(const char* name) {
    if (!name) return false;
    for (const char* p=name; *p; ++p) {
        const auto lower=[](char c) { return c>='A' && c<='Z'
            ? static_cast<char>(c+('a'-'A')) : c; };
        if (lower(p[0])=='m' && lower(p[1])=='m') return true;
    }
    return false;
}

// Solo misiles aire-aire. La lista es por familia de arma (designador o nombre
// comercial) tal y como aparece en el nombre del proyectil. Todo lo que no
// coincida se descarta: aire-tierra (agm/kh), antiradiacion (harm), bombas,
// cohetes y señuelos. Si algun misil no aparece, el log de debug publica
// not_aam_name con el nombre exacto que hay que añadir aqui.
inline bool name_is_air_to_air(const char* name)
{
    if (!name || name[0] == '\0') return false;
    char lower[96]{};
    std::size_t length = 0;
    for (; name[length] != '\0' && length + 1 < sizeof(lower); ++length) {
        const char c = name[length];
        lower[length] = c == '-' ? '_' :
            ((c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c);
    }
    lower[length] = '\0';
    const std::string_view view(lower, length);
    // El prefijo antes de ^ identifica al avion; el sufijo completo identifica
    // el arma observada en el log. Evitar aceptar otros nombres parecidos.
    if (view.ends_with("^sws_flz_lwf_63.blk")) return true;
    static constexpr std::string_view tokens[] = {
        // Designadores (con y sin guion bajo)
        "aim9", "aim_9", "aim7", "aim_7", "aim120", "aim_120", "aim54", "aim_54", "aim132", "aim174",
        "r_3", "r3s", "r_13", "r13m", "r_23", "r23", "r_24", "r24", "r_27", "r27", "r_33", "r33",
        "r_37", "r37", "r_40", "r40", "r_60", "r60", "r_73", "r73", "r_74", "r74", "r_77", "r77",
        "pl_2", "pl2", "pl_5", "pl5", "pl_7", "pl7", "pl_8", "pl8", "pl_10", "pl10", "pl_12", "pl12",
        "pl_15", "pl15", "pl_17", "pl17",
        "rb_24", "rb24", "rb_27", "rb27", "rb_28", "rb28", "rb_71", "rb71", "rb_74", "rb74",
        "rb_99", "rb99", "aam_1", "aam1", "aam_3", "aam3", "aam_5", "aam5", "asraam",
        // Nombre comercial
        "sidewinder", "sparrow", "amraam", "phoenix", "magic", "mica", "r530", "r_530", "r550",
        "python", "derby", "shafrir", "skyflash", "firestreak", "red_top", "redtop", "meteor",
        "aspide", "iris_t",
        // Generico
        "air_to_air", "airtoair",
    };
    for (const auto token : tokens)
        if (view.find(token) != std::string_view::npos) return true;
    return false;
}

inline const char* failure_name(Failure failure) {
    switch (failure) {
    case Failure::none: return "none";
    case Failure::address: return "address";
    case Failure::read: return "read";
    case Failure::selector: return "selector";
    case Failure::layout: return "layout";
    case Failure::descriptor: return "descriptor";
    case Failure::budget: return "budget";
    case Failure::changed: return "changed";
    }
    return "none";
}

inline bool valid_pointer(std::uintptr_t value) {
    return value >= 0x10000 && value < 0x7FFFFFFFFFFF;
}

inline constexpr std::size_t max_candidates = 499;
inline constexpr std::size_t max_names = 64;
inline constexpr std::size_t record_size = 0x38;
inline constexpr std::uint32_t max_selector = 0xFFFFFF; // 24 bits: el ECS usa IDs dispersos, no indices de centenas

inline void clean_name(const char* raw, std::size_t raw_size, char* out, std::size_t out_size)
{
    if (out && out_size != 0) out[0] = '\0';
    if (!raw || !out || out_size == 0 || raw_size == 0) return;
    std::size_t length = 0;
    while (length < raw_size && raw[length] != '\0') {
        const auto c = static_cast<unsigned char>(raw[length]);
        if (c < 0x20 || c > 0x7E) break;
        ++length;
    }
    if (length < 2) return;
    std::string_view view(raw, length);
    const auto marker = view.find("/short");
    if (marker != std::string_view::npos) view = view.substr(0, marker);
    const auto slash = view.find_last_of('/');
    if (slash != std::string_view::npos) view = view.substr(slash + 1);
    const auto caret = view.find_last_of('^');
    if (caret != std::string_view::npos) view = view.substr(caret + 1);
    if (view.size() > 4) {
        std::string_view tail = view.substr(view.size() - 4);
        if (tail[0] == '.' && (tail[1] == 'b' || tail[1] == 'B') &&
            (tail[2] == 'l' || tail[2] == 'L') && (tail[3] == 'k' || tail[3] == 'K'))
            view = view.substr(0, view.size() - 4);
    }
    const std::size_t copy = (std::min)(view.size(), out_size - 1);
    std::memcpy(out, view.data(), copy);
    out[copy] = '\0';
}

template<class Read>
inline void read_query(Read&& reader, std::uintptr_t base, const Layout& layout,
                       bool is_bomb, Diagnostics& diagnostic,
                       std::vector<Candidate>& out) {
    diagnostic = {};
    const std::size_t start_size = out.size();
    constexpr std::uintptr_t limit = 0x7FFFFFFFFFFF;
    auto fail = [&](Failure value) {
        if (diagnostic.failure == Failure::budget) return;
        if (value == Failure::budget || value == Failure::changed ||
            diagnostic.failure == Failure::none) diagnostic.failure = value;
    };
    auto finish = [&] {
        // Presupuesto agotado: se conserva lo ya validado (resultado parcial) en
        // lugar de descartar la consulta entera. Asi un presupuesto bajo degrada
        // a "menos misiles" y nunca a "ninguno".
        diagnostic.accepted = static_cast<std::uint32_t>(out.size() - start_size);
    };
    auto range = [&](std::uintptr_t address, std::size_t size) {
        if (!valid_pointer(address) || size > limit - address) {
            fail(Failure::address);
            return false;
        }
        return true;
    };
    auto add = [&](std::uintptr_t a, std::uintptr_t b, std::uintptr_t& result) {
        if (b > UINTPTR_MAX - a) {
            fail(Failure::address);
            return false;
        }
        result = a + b;
        return range(result, 0);
    };
    auto multiply = [&](std::uintptr_t a, std::uintptr_t b,
                        std::uintptr_t& result) {
        if (a != 0 && b > UINTPTR_MAX / a) {
            fail(Failure::address);
            return false;
        }
        result = a * b;
        return true;
    };
    auto access = [&](std::uintptr_t address, void* destination, std::size_t size) {
        if (diagnostic.failure == Failure::budget) return false;
        if (!range(address, size)) return false;
        if (diagnostic.read_calls >= layout.max_reads) {
            fail(Failure::budget);
            return false;
        }
        ++diagnostic.read_calls;
        if (!reader(address, destination, size)) {
            fail(Failure::read);
            return false;
        }
        return true;
    };
    auto get = []<class T, std::size_t N>(const std::array<std::uint8_t, N>& bytes,
                                        std::size_t offset, T& value) {
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
    };
    auto soft_access = [&](std::uintptr_t address, void* destination, std::size_t size) {
        if (diagnostic.failure == Failure::budget) return false;
        if (!valid_pointer(address) || size > limit - address) return false;
        if (diagnostic.read_calls >= layout.max_reads) {
            fail(Failure::budget);
            return false;
        }
        ++diagnostic.read_calls;
        return reader(address, destination, size);
    };
    if (!valid_pointer(base)) {
        fail(Failure::address);
        return;
    }
    std::uintptr_t selector_address{}, data_address{}, index_address{}, displacement{};
    std::uint32_t raw_selector{};
    std::uint64_t data{}, index{};
    std::array<std::uint8_t, record_size> record{};
    if (!add(base, layout.manager_rva, diagnostic.manager) ||
        !add(base, is_bomb ? layout.bomb_selector_rva : layout.rocket_selector_rva,
             selector_address) ||
        !add(diagnostic.manager, layout.data_offset, data_address) ||
        !add(diagnostic.manager, layout.index_offset, index_address) ||
        !access(selector_address, &raw_selector, sizeof(raw_selector))) return;
    diagnostic.raw_selector = raw_selector;
    diagnostic.selector = raw_selector & max_selector;
    if (raw_selector == UINT32_MAX) {
        fail(Failure::selector);
        return;
    }
    // El selector observado 0x8F7A75 excede 20 bits pero pertenece al campo
    // enmascarado de 24 bits. No suponer indices densos: comprobar SIEMPRE
    // punteros/lecturas del record y validar su descriptor antes de emitir algo.
    if (!access(data_address, &data, sizeof(data)) ||
        !access(index_address, &index, sizeof(index)) ||
        !range(data, 0) || !range(index, 0) ||
        !multiply(diagnostic.selector, 0x40, displacement) ||
        !add(index, displacement, diagnostic.record) ||
        !access(diagnostic.record, record.data(), record.size())) return;
    std::uint8_t required{}, optional{};
    std::uint16_t subcount{};
    get(record, 0, required);
    get(record, 1, optional);
    get(record, 2, subcount);
    diagnostic.required = required;
    diagnostic.optional = optional;
    diagnostic.subcount = subcount;
    diagnostic.data_table = data;
    diagnostic.index_table = index;
    std::memcpy(diagnostic.header.data(), record.data(), diagnostic.header.size());
    const std::uint32_t total = std::uint32_t(required) + optional;
    diagnostic.sublist_count = subcount;
    if (total < 2 || total > 32 || subcount > 256) {
        fail(Failure::layout);
        return;
    }
    if (subcount == 0) return;
    std::array<std::uint16_t, 256> ids{};
    std::array<std::uint16_t, 8192> matrix{};
    std::uintptr_t cells{}, ids_bytes{}, matrix_bytes{}, ids_external{}, matrix_external{};
    if (!multiply(subcount, total, cells) ||
        !multiply(subcount, sizeof(std::uint16_t), ids_bytes) ||
        !multiply(cells, sizeof(std::uint16_t), matrix_bytes)) return;
    if (subcount <= 9) {
        std::memcpy(ids.data(), record.data() + 4, ids_bytes);
    } else {
        std::uint64_t ids_pointer{};
        get(record, 8, ids_pointer);
        ids_external = static_cast<std::uintptr_t>(ids_pointer);
        if (!access(ids_external, ids.data(), ids_bytes)) return;
    }
    if (cells <= 16) {
        std::memcpy(matrix.data(), record.data() + 0x18, matrix_bytes);
    } else {
        std::uint64_t matrix_pointer{};
        get(record, 0x18, matrix_pointer);
        matrix_external = static_cast<std::uintptr_t>(matrix_pointer);
        if (!access(matrix_external, matrix.data(), matrix_bytes)) return;
    }
    std::array<std::uint64_t, 2048> pointers{};
    std::array<std::uint8_t, 2048> active{};
    for (std::size_t sub = 0; sub < subcount &&
         out.size() - start_size < max_candidates; ++sub) {
        ++diagnostic.sublists_visited;
        std::uintptr_t descriptor_address{};
        std::array<std::uint8_t, 32> descriptor{};
        if (!multiply(ids[sub], 0x20, displacement) ||
            !add(data, displacement, descriptor_address) ||
            !access(descriptor_address, descriptor.data(), descriptor.size())) {
            if (diagnostic.failure == Failure::budget) { finish(); return; }
            continue;
        }
        std::uint64_t storage{};
        std::uint32_t count{};
        std::uint8_t shift{};
        get(descriptor, 0, storage);
        get(descriptor, 8, count);
        get(descriptor, 0xC, shift);
        const auto offset0 = matrix[sub * total];
        const auto offset1 = matrix[sub * total + 1];
        if (sub == 0) {
            diagnostic.offset0 = offset0;
            diagnostic.offset1 = offset1;
            std::memcpy(diagnostic.descriptor.data(), descriptor.data(), diagnostic.descriptor.size());
        }
        if (count > 2048 || shift > 16 ||
            count > (std::uint32_t{1} << shift) ||
            (count != 0 && !valid_pointer(storage)) ||
            offset0 == UINT16_MAX || offset1 == UINT16_MAX) {
            fail(Failure::descriptor);
            continue;
        }
        std::uintptr_t col0{}, col1{}, pointer_bytes{};
        if (count != 0) {
            const std::uintptr_t scale = std::uintptr_t{1} << shift;
            if (!multiply(offset0, scale, displacement) ||
                !add(storage, displacement, col0) ||
                !multiply(offset1, scale, displacement) ||
                !add(storage, displacement, col1) ||
                !multiply(count, sizeof(std::uint64_t), pointer_bytes) ||
                !range(col0, pointer_bytes) || !range(col1, count)) continue;
            if (col0 < col1 + count && col1 < col0 + pointer_bytes) {
                fail(Failure::descriptor);
                continue;
            }
        }
        if (diagnostic.rows_seen >= layout.max_reads || count > layout.max_reads - diagnostic.rows_seen) {
            fail(Failure::budget);
            finish();
            return;
        }
        if (count != 0 &&
            (!access(col0, pointers.data(), pointer_bytes) ||
             !access(col1, active.data(), count))) {
            if (diagnostic.failure == Failure::budget) { finish(); return; }
            continue;
        }
        const std::size_t descriptor_start = out.size();
        for (std::uint32_t i = 0; i < count &&
             out.size() - start_size < max_candidates; ++i) {
            ++diagnostic.rows_seen;
            if (active[i] != 1) continue;
            ++diagnostic.active_rows;
            const std::uintptr_t source = pointers[i];

            // Nombre primero: si es una bengala se descarta aqui, antes de
            // leer posicion/velocidad, asi no calcula trayectoria ni se dibuja.
            char raw_name[64]{};
            bool have_name = false;
            if (diagnostic.names_read < max_names &&
                diagnostic.read_calls + 12 <= layout.max_reads) {
                const std::uintptr_t text_offset = is_bomb ? layout.bomb_name_text_offset : layout.missile_name_text_offset;
                const std::uintptr_t container_offset = is_bomb ? layout.bomb_name_offset : layout.missile_name_offset;
                const auto try_name = [&](std::uintptr_t container_at, std::uintptr_t text_at) -> bool {
                    if (!container_at) return false;
                    ++diagnostic.name_probes;
                    const bool first = diagnostic.name_probe_source == 0;
                    if (first) diagnostic.name_probe_source = source;
                    const auto probe_stage = [&](std::uint32_t stage) {
                        if (diagnostic.name_probe_source == source) diagnostic.name_probe_stage = stage;
                    };
                    if (container_at > limit - source) { probe_stage(1); return false; }
                    std::uint64_t container{};
                    if (!soft_access(source + container_at, &container, sizeof(container))) {
                        probe_stage(2); return false;
                    }
                    if (first && diagnostic.name_probe_container == 0) diagnostic.name_probe_container = container;
                    if (!valid_pointer(container)) { probe_stage(3); return false; }
                    if (text_at > limit - container) { probe_stage(4); return false; }
                    std::uint64_t text{};
                    if (!soft_access(container + text_at, &text, sizeof(text))) {
                        probe_stage(5); return false;
                    }
                    if (diagnostic.name_probe_source == source && diagnostic.name_probe_text == 0)
                        diagnostic.name_probe_text = text;
                    if (!valid_pointer(text) || 63 > limit - text) { probe_stage(6); return false; }
                    std::memset(raw_name, 0, sizeof(raw_name));
                    if (!soft_access(text, raw_name, 63)) { probe_stage(7); return false; }
                    if (!raw_name[0]) { probe_stage(8); return false; }
                    probe_stage(9);
                    have_name = true;
                    ++diagnostic.names_read;
                    if (diagnostic.name_offset == 0) {
                        diagnostic.name_offset = container_at;
                        diagnostic.name_text_offset = text_at;
                    }
                    if (diagnostic.name_container == 0) {
                        diagnostic.name_container = container;
                        diagnostic.name_text = text;
                        std::memcpy(diagnostic.raw_name, raw_name, sizeof(diagnostic.raw_name));
                    }
                    return true;
                };
                // Prefer the supplied +0x720 -> +0x870 chain. Legacy +0x50 is
                // attempted only on failed reads; unknown names stay rejected in AAM mode.
                have_name = try_name(container_offset, text_offset);
                if (!have_name && !is_bomb && layout.missile_name_text_offset_alt)
                    have_name = try_name(container_offset, layout.missile_name_text_offset_alt);
                const auto alternate = is_bomb ? layout.bomb_name_offset_alt : layout.missile_name_offset_alt;
                if (!have_name && alternate) have_name = try_name(alternate, text_offset);
                if (!have_name && alternate && !is_bomb && layout.missile_name_text_offset_alt)
                    have_name = try_name(alternate, layout.missile_name_text_offset_alt);
                if (diagnostic.failure == Failure::budget) { finish(); return; }
                if (have_name && name_has_flare(raw_name)) {
                    ++diagnostic.flares;
                    continue;
                }
                if (have_name && name_has_mm(raw_name)) {
                    ++diagnostic.caliber_ignored;
                    continue; // antes de callback, posicion, velocidad y trayectoria
                }
                if (have_name && !is_bomb &&
                    !(layout.name_allowed ? layout.name_allowed(raw_name, layout.name_context)
                        : (!layout.aam_only || name_is_air_to_air(raw_name)))) {
                    ++diagnostic.not_aam;
                    if (diagnostic.not_aam_name[0] == '\0')
                        std::memcpy(diagnostic.not_aam_name, raw_name, sizeof(diagnostic.not_aam_name));
                    continue;
                }
            }
            // Sin nombre no existe una casilla segura que permita identificarlo.
            if (!have_name) {
                ++diagnostic.not_aam;
                continue;
            }

            Vec3 position{}, velocity{};
            std::uint64_t owner{};
            std::uintptr_t position_address{}, velocity_address{}, owner_address{};
            if (!range(source, 0) ||
                !add(source, layout.position_offset, position_address) ||
                !add(source, layout.velocity_offset, velocity_address) ||
                !add(source, layout.owner_offset, owner_address) ||
                !access(position_address, &position, 12) ||
                !access(velocity_address, &velocity, 12) ||
                !access(owner_address, &owner, sizeof(owner)) || !range(owner, 0)) {
                ++diagnostic.rejected;
                if (diagnostic.failure == Failure::budget) { finish(); return; }
                continue;
            }
            const auto coordinate = [](float value) {
                return std::isfinite(value) && std::abs(value) <= 1.0e7f;
            };
            const float speed = std::hypot(velocity.x, velocity.y, velocity.z);
            if (!coordinate(position.x) || !coordinate(position.y) ||
                !coordinate(position.z) || !std::isfinite(velocity.x) ||
                !std::isfinite(velocity.y) || !std::isfinite(velocity.z) ||
                !std::isfinite(speed) || speed < 20.0f || speed > 2500.0f) {
                ++diagnostic.rejected;
                continue;
            }
            std::uintptr_t active_address{}, pointer_address{};
            std::uint8_t current_active{};
            std::uint64_t current_pointer{};
            if (!add(col1, i, active_address) ||
                !multiply(i, sizeof(std::uint64_t), displacement) ||
                !add(col0, displacement, pointer_address) ||
                !access(active_address, &current_active, sizeof(current_active)) ||
                !access(pointer_address, &current_pointer, sizeof(current_pointer)) ||
                current_active != 1 || current_pointer != source) {
                ++diagnostic.rejected;
                if (diagnostic.failure == Failure::budget) { finish(); return; }
                continue;
            }
            Candidate candidate{source, owner, position, velocity, speed, is_bomb};
            if (have_name)
                clean_name(raw_name, sizeof(raw_name), candidate.name, sizeof(candidate.name));
            out.push_back(candidate);
        }
        std::array<std::uint8_t, 13> current_descriptor{};
        if (!access(descriptor_address, current_descriptor.data(), current_descriptor.size())) {
            out.resize(descriptor_start);
            if (diagnostic.failure == Failure::budget) { finish(); return; }
        } else if (!std::equal(current_descriptor.begin(), current_descriptor.end(),
                               descriptor.begin())) {
            out.resize(descriptor_start);
            fail(Failure::changed);
        }
    }
    if (out.size() - start_size >= max_candidates) diagnostic.truncated = 1;
    bool tables_ok = true;
    if (ids_external != 0) {
        std::array<std::uint16_t, 256> current_ids{};
        tables_ok = access(ids_external, current_ids.data(), ids_bytes) && current_ids == ids;
    }
    if (tables_ok && matrix_external != 0) {
        std::array<std::uint16_t, 8192> current_matrix{};
        tables_ok = access(matrix_external, current_matrix.data(), matrix_bytes) && current_matrix == matrix;
    }
    if (!tables_ok) {
        out.resize(start_size);
        fail(Failure::changed);
    }
    if (diagnostic.sublists_visited != 0) {
        std::uint32_t current_selector{};
        std::uint64_t current_data{}, current_index{};
        std::array<std::uint8_t, record_size> current_record{};
        const bool selector_ok = access(selector_address, &current_selector, sizeof(current_selector));
        const bool record_ok = access(diagnostic.record, current_record.data(), current_record.size());
        const bool data_ok = access(data_address, &current_data, sizeof(current_data));
        const bool index_ok = access(index_address, &current_index, sizeof(current_index));
        if (!selector_ok || !record_ok || !data_ok || !index_ok ||
            current_selector != raw_selector || current_record != record ||
            current_data != data || current_index != index) {
            out.resize(start_size);
            fail(Failure::changed);
        }
    }
    finish();
}

}
