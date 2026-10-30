#pragma once
// ============================================================================
//   features/esp/xray_bvh.hpp
//   Decoder de Packed BVH (Damage Model) — port C++ del Lua de referencia.
//   Cadena: unit + 0x10A8 → +0x58 → +0x78 (header) → records + pool + names.
//   NO toca ImGui. Solo decodifica la geometría a listas de vértices e índices.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include "xray_parts.hpp"
#include "xray_boxes.hpp"
#include "../../core/adjacent_pointer_fields.hpp"

namespace xray
{
    // ========================================================================
    // [SECCIÓN A] Helpers aritméticos
    // ========================================================================
    inline uint32_t dm_ashr(uint32_t val, int shift)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(val) >> shift);
    }

    inline int dm_popcnt32(uint32_t v)
    {
        int c = 0;
        while (v) { v &= (v - 1); ++c; }
        return c;
    }

    // ========================================================================
    // [SECCIÓN B] Lecturas seguras
    // ========================================================================
    inline uint32_t dm_read_u32(const uint8_t* buf, size_t size, size_t off)
    {
        if (!buf || off > size || size - off < sizeof(uint32_t))
            return 0;
        uint32_t v = 0;
        std::memcpy(&v, buf + off, sizeof(v));
        return v;
    }

    inline float dm_read_f32(const uint8_t* buf, size_t size, size_t off)
    {
        if (!buf || off > size || size - off < sizeof(float))
            return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, buf + off, sizeof(v));
        return v;
    }

    inline uint64_t dm_read_u64(const uint8_t* buf, size_t size, size_t off)
    {
        if (!buf || off > size || size - off < sizeof(uint64_t))
            return 0;
        uint64_t v = 0;
        std::memcpy(&v, buf + off, sizeof(v));
        return v;
    }

    // ========================================================================
    // [SECCIÓN C] Estructuras públicas
    // ========================================================================
    struct XrayPart
    {
        std::string            name;
        xray_part_kind_t       kind = xray_part_kind_t::Body; // clasificación cacheada al decodificar
        std::vector<vec3_t>    vertices;         // decodificados en espacio local del part
        boxes::bounds3         local_bounds{};   // caja calculada UNA vez por decodificación
        std::vector<uint32_t>  indices;          // triples: [i0,i1,i2, i3,i4,i5, ...]
        uint16_t               transform_index = 0;      // bind pose index (m_TransformIndex)
        uint16_t               tree_index = 0xFFFF; // animated skeleton index (m_TreeIndex)
    };

    // Cache por unidad. Guardamos los PUNTEROS de la cadena para detectar
    // cuándo el juego reasigna el DamageModelContainer (típico al pasar de
    // parado a moverse). Comparar POR UNIDAD evita bug con thread_local.
    struct XrayCacheEntry
    {
        std::vector<XrayPart>  parts;

        // cadena de punteros (para detección de cambios)
        uintptr_t              dm_root = 0;
        uintptr_t              dm_mid = 0;
        uintptr_t              header_addr = 0;
        uintptr_t              matrix_array_base = 0;

        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    // ========================================================================
    // [SECCIÓN D] Decoder BVH
    // ========================================================================
    struct BVHDecoder
    {
        const uint8_t* tree = nullptr;
        size_t         tree_size = 0;
        uint32_t       aligned_pad = 0;
        mutable uint32_t node_budget = 0;

        inline uint32_t read32(size_t off) const
        {
            return dm_read_u32(tree, tree_size, off);
        }

        void decode_leaf(
            uint32_t a2_offset,
            uint32_t a2_desc,
            bool is_short,
            std::vector<uint32_t>& out) const
        {
            const uint32_t v5 = a2_desc;
            uint32_t v4 = 0, v6 = 0, v7 = 0, v8 = 0, v9 = 0, v10 = 0;
            uint32_t v11 = 0, v12 = 0, v13 = 0;
            uint32_t v25 = 0, v26 = 0, v27_val = 0, v28 = 0, v29 = 0;
            uint32_t v30 = 0, v24 = 0;

            if (is_short)
            {
                v4 = read32(a2_offset);
                v6 = (4u * v4) & 0x1FFFFFCu;
                v7 = dm_ashr((v5 << 6), 19);
                const uint32_t combo = ((v5 >> 7) & 0xF80000u) | (v4 & 0xFF000000u);
                v8 = dm_ashr(combo, 19);
                v25 = (v8 == v7) ? 1u : 0u;
                v9 = (v4 >> 23) & 0x1FFu;
                v10 = 0; v28 = 0; v29 = 0; v30 = 0; v26 = 0; v27_val = 0; v24 = 1u;
            }
            else
            {
                v12 = read32(a2_offset);
                v13 = read32(a2_offset + 4);
                v11 = read32(a2_offset + 8);
                v6 = (4u * v12) & 0x3FFFFFCu;
                v27_val = v13 & 0xFFFFu;
                v7 = dm_ashr((v5 << 6), 19);
                const uint32_t combo = ((v5 >> 7) & 0xF80000u) | (v12 & 0xFF000000u);
                v8 = dm_ashr(combo, 19);
                v10 = dm_ashr((v11 << 19), 19);
                v25 = (v8 == v7) ? 1u : 0u;
                v9 = (v13 >> 29) & 7u;
                v28 = dm_ashr((8u * v13), 19);
                v30 = (v28 != v10) ? 1u : 0u;
                v26 = dm_ashr((v11 << 6), 19);
                v24 = (v26 == v10) ? 1u : 0u;
                v29 = (v13 >> 30) & 1u;
            }

            const uint32_t v14_idx = dm_ashr((v5 << 19), 19);
            const uint32_t v16 = ((v6 + a2_offset - aligned_pad) & 0xFFFFFFFFu) >> 3;

            out.push_back(v16);
            out.push_back(v16 + v14_idx);
            out.push_back(v16 + v7);

            if (!v25)
            {
                const uint32_t v19 = v16 + v8;
                uint32_t v20 = v7;
                uint32_t cur_v14 = v14_idx;
                if ((v9 & 1u) != 0)
                {
                    v20 = cur_v14;
                    cur_v14 = v7;
                }
                out.push_back(v16 + cur_v14);
                out.push_back(v16 + v20);
                out.push_back(v19);
            }

            if (v30)
            {
                const uint32_t v21 = v27_val + v16;
                out.push_back(v21);
                out.push_back(v21 + v28);
                out.push_back(v21 + v10);

                if (!v24)
                {
                    uint32_t v22 = v10;
                    uint32_t v23 = v28;
                    if (v29 != 0) { v22 = v28; v23 = v10; }
                    out.push_back(v21 + v23);
                    out.push_back(v21 + v22);
                    out.push_back(v21 + v26);
                }
            }
        }

        void traverse(uint32_t a2, std::vector<uint32_t>& out, int depth = 0) const
        {
            if (depth > 64) return;
            if (node_budget == 0) return;
            --node_budget;

            const uint32_t v4 = a2;
            const uint32_t v6 = a2 & 0x3FFFFFCu;
            const uint32_t v7 = v6;
            const uint32_t v8 = v4 & 3u;
            const uint32_t child_count = v8 + 1;
            const uint32_t v10_addr = v7 + 12u * child_count;

            const uint32_t v4_shifted = (v4 >> 26) & 0x3Fu;
            const uint32_t v13 = v4_shifted & 0xFu;

            uint32_t child_words[4] = {};
            uint32_t v11 = 0;
            for (int i = 0; i < 4; ++i)
            {
                const uint32_t dw = read32(v10_addr + i * 4);
                child_words[i] = dw;
                if ((dw & 0x80000000u) != 0)
                    v11 |= (1u << i);
            }

            const uint32_t v12 = v6 + 16u * child_count;
            const uint32_t v18 = static_cast<uint32_t>(-2) << v8;
            const uint32_t v17 = v4_shifted | (static_cast<uint32_t>(-2) << v8);

            for (uint32_t i = 0; i < child_count; ++i)
            {
                const uint32_t child_val = child_words[i];
                if ((child_val & 0x80000000u) == 0)
                {
                    traverse(child_val, out, depth + 1);
                }
                else
                {
                    const uint32_t shift_mask = static_cast<uint32_t>(-1) << i;
                    const uint32_t mask1 = v11 & ~(v18 | shift_mask);
                    const uint32_t mask2 = v11 & ~(shift_mask | v17);
                    const uint32_t leaf_offset =
                        v12 + 4u * static_cast<uint32_t>(dm_popcnt32(mask1))
                        + 8u * static_cast<uint32_t>(dm_popcnt32(mask2));
                    const bool is_short = (v13 & (1u << i)) != 0;
                    decode_leaf(leaf_offset, child_val, is_short, out);
                }
            }
        }
    };

    // ========================================================================
    // [SECCIÓN E] decode_part
    // ========================================================================
    inline bool decode_part(
        uintptr_t pool_base,
        uintptr_t name_pool_base,
        uint32_t  name_pool_size,
        uint32_t  vert_count,
        uint32_t  data_offset,
        uint32_t  name_offset,
        uint16_t  transform_index,
        uint16_t  tree_index,
        XrayPart& out)
    {
        if (vert_count == 0 ||
            vert_count > static_cast<uint32_t>(offsets::damage_model::max_vertices))
            return false;
        if (data_offset == 0xFFFFFFFFu)
            return false;

        // --- nombre ---
        out.name.clear(); // decode_part también puede llamarse con un XrayPart reutilizado.
        out.local_bounds = {};
        out.kind = xray_part_kind_t::Body;
        if (name_pool_base != 0 && name_offset != 0 && name_offset < name_pool_size)
        {
            char name_buf[64] = {};
            const size_t available = static_cast<size_t>(name_pool_size - name_offset);
            const size_t read_size = std::min(available, sizeof(name_buf) - 1);
            if (TargetProcess->Read(name_pool_base + name_offset, name_buf, read_size))
            {
                name_buf[read_size] = '\0';
                out.name = name_buf;
            }
        }
        if (out.name.empty())
            out.name = "<unknown>";

        // No decodificar vértices BVH de body ni piezas desconocidas: nunca
        // se dibujan. Esta misma clasificación cacheada se usa al renderizar.
        out.kind = classify_part_name(out.name);
        if (out.kind == xray_part_kind_t::Body)
            return false;

        // --- header del pool item (0x30 bytes) ---
        uint8_t header[0x30] = {};
        if (!TargetProcess->Read(pool_base + data_offset, header, sizeof(header)))
            return false;

        const float    scale_raw_x = dm_read_f32(header, sizeof(header), offsets::damage_model::geom_quant_scale + 0x00);
        const float    scale_raw_y = dm_read_f32(header, sizeof(header), offsets::damage_model::geom_quant_scale + 0x04);
        const float    scale_raw_z = dm_read_f32(header, sizeof(header), offsets::damage_model::geom_quant_scale + 0x08);
        const float    min_x = dm_read_f32(header, sizeof(header), offsets::damage_model::geom_quant_offset + 0x00);
        const float    min_y = dm_read_f32(header, sizeof(header), offsets::damage_model::geom_quant_offset + 0x04);
        const float    min_z = dm_read_f32(header, sizeof(header), offsets::damage_model::geom_quant_offset + 0x08);
        const uint32_t tree_bytes = dm_read_u32(header, sizeof(header), offsets::damage_model::geom_tree_bytes);
        const uint32_t root_desc = dm_read_u32(header, sizeof(header), offsets::damage_model::geom_root);

        constexpr float k_quant_factor = 0.03125f;
        const float scale_x = scale_raw_x * k_quant_factor;
        const float scale_y = scale_raw_y * k_quant_factor;
        const float scale_z = scale_raw_z * k_quant_factor;

        const uint64_t aligned_pad_wide = (static_cast<uint64_t>(tree_bytes) + 7ull) & ~7ull;
        const uint64_t total_item_size_wide = 0x30ull + aligned_pad_wide + static_cast<uint64_t>(vert_count) * 8ull;
        if (total_item_size_wide > (32ull << 20))
            return false;
        const uint32_t aligned_pad = static_cast<uint32_t>(aligned_pad_wide);
        const size_t total_item_size = static_cast<size_t>(total_item_size_wide);

        std::vector<uint8_t> item_buf(total_item_size, 0);
        if (!TargetProcess->Read(pool_base + data_offset, item_buf.data(), total_item_size))
            return false;

        // --- vértices ---
        out.vertices.clear();
        out.vertices.reserve(vert_count);

        const size_t verts_off = 0x30u + aligned_pad;
        for (uint32_t j = 0; j < vert_count; ++j)
        {
            const uint64_t packed = dm_read_u64(item_buf.data(), item_buf.size(), verts_off + j * 8u);
            const uint32_t rx = static_cast<uint32_t>(packed & 0x1FFFFFull);
            const uint32_t ry = static_cast<uint32_t>((packed >> 21) & 0x1FFFFFull);
            const uint32_t rz = static_cast<uint32_t>(packed >> 42);

            vec3_t v;
            v.x = min_x + static_cast<float>(rx) * scale_x;
            v.y = min_y + static_cast<float>(ry) * scale_y;
            v.z = min_z + static_cast<float>(rz) * scale_z;
            out.vertices.push_back(v);
        }

        out.transform_index = transform_index;
        out.tree_index = tree_index;

        // --- BVH traversal ---
        out.indices.clear();
        out.indices.reserve(static_cast<size_t>(vert_count) * 6);

        if ((root_desc & 0x80000000u) == 0)
        {
            BVHDecoder decoder;
            decoder.tree = item_buf.data() + 0x30;
            decoder.tree_size = tree_bytes;
            decoder.aligned_pad = aligned_pad;
            decoder.node_budget = static_cast<uint32_t>(offsets::damage_model::node_budget);

            if ((root_desc & 3u) != 0)
            {
                decoder.traverse(root_desc, out.indices);
            }
            else
            {
                const uint32_t v29 = root_desc & 0x1FFFFFCu;
                uint32_t v30 = v29 + 4;
                uint32_t v31 = 24u;
                if ((root_desc & 0x2000000u) != 0)
                    v30 = v29 + 32;
                else
                    v31 = 0;
                const uint32_t v32 = decoder.read32(v31 + v29);
                decoder.decode_leaf(v30, v32, false, out.indices);
            }
        }

        // --- filtrado de índices fuera de rango ---
        if (!out.indices.empty())
        {
            // Compact in place: write never overtakes the unread triangles.
            // Preserve order and discard incomplete trailing triples as before.
            size_t write_index = 0;
            for (size_t i = 0; i + 2 < out.indices.size(); i += 3)
            {
                const uint32_t a = out.indices[i];
                const uint32_t b = out.indices[i + 1];
                const uint32_t c = out.indices[i + 2];
                if (a < vert_count && b < vert_count && c < vert_count)
                {
                    out.indices[write_index++] = a;
                    out.indices[write_index++] = b;
                    out.indices[write_index++] = c;
                }
            }
            out.indices.resize(write_index);
        }
        // Sólo Ammo necesita una caja: no cargar este trabajo al resto.
        if(boxes::compact(out.kind))
            out.local_bounds=boxes::from_indexed_vertices(out.vertices,out.indices);

        return true;
    }

    // ========================================================================
    // [SECCIÓN F] get_xray_cache
    //
    // La cadena de punteros se RE-LEE CADA FRAME porque el juego puede
    // reasignar el contenedor cuando el vehículo cambia de estado. Los
    // parts sólo se re-decodifican cuando la cadena cambia o expira el TTL.
    // ========================================================================
    inline XrayCacheEntry& get_xray_cache(uintptr_t unit_base)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, XrayCacheEntry> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];

        // --- Re-leer cadena ---
        const uintptr_t dm_root_new = TargetProcess->Read<uintptr_t>(
            unit_base + offsets::unit_offsets::damageModelCont_offset);

        const uintptr_t dm_mid_new = dm_root_new
            ? TargetProcess->Read<uintptr_t>(dm_root_new + offsets::damage_model::m_ModelContainer)
            : 0;

        uintptr_t header_new=0,matrix_array_new=0;
        if (dm_mid_new && !adjacent_pointer_fields::read_pair(
            dm_mid_new,offsets::damage_model::m_PackedHeader,
            offsets::damage_model::m_TransformArray,header_new,matrix_array_new,
            [](uintptr_t address,void* dest,size_t bytes) {
                return TargetProcess->ReadOptional(address,dest,bytes);
            })) {
            // Offsets distantes o lectura parcial: conservar recuperación campo a campo.
            header_new=TargetProcess->Read<uintptr_t>(dm_mid_new+offsets::damage_model::m_PackedHeader);
            matrix_array_new=TargetProcess->Read<uintptr_t>(dm_mid_new+offsets::damage_model::m_TransformArray);
        }

        const bool chain_changed =
            dm_root_new != entry.dm_root ||
            dm_mid_new != entry.dm_mid ||
            header_new != entry.header_addr ||
            matrix_array_new != entry.matrix_array_base;

        entry.dm_root = dm_root_new;
        entry.dm_mid = dm_mid_new;
        entry.header_addr = header_new;
        entry.matrix_array_base = matrix_array_new;

        const bool need_refresh = chain_changed || (now >= entry.next_refresh);

        if (need_refresh)
        {
            entry.parts.clear();

            if (!dm_root_new || !dm_mid_new || !header_new)
            {
                entry.next_refresh = now + std::chrono::seconds(2);
                entry.last_seen = now;
                return entry;
            }

            // --- campos del header ---
            const uint32_t rel_offset = TargetProcess->Read<uint32_t>(header_new + offsets::damage_model::records_rel);
            const uint32_t loop_count = TargetProcess->Read<uint32_t>(header_new + offsets::damage_model::node_count);
            const uint32_t pool_offset = TargetProcess->Read<uint32_t>(header_new + offsets::damage_model::geometry_rel);
            const uint32_t names_rel = TargetProcess->Read<uint32_t>(header_new + offsets::damage_model::names_rel);
            const uint32_t names_size = TargetProcess->Read<uint32_t>(header_new + offsets::damage_model::names_valid);

            if (loop_count == 0 ||
                loop_count > static_cast<uint32_t>(offsets::damage_model::max_parts))
            {
                entry.next_refresh = now + std::chrono::seconds(2);
                entry.last_seen = now;
                return entry;
            }

            const uintptr_t list_base = header_new + rel_offset;
            const uintptr_t pool_base = header_new + pool_offset;
            const uintptr_t name_pool_base = header_new + names_rel;

            // --- leer records ---
            const size_t stride = offsets::damage_model::record_stride;
            std::vector<uint8_t> part_records(static_cast<size_t>(loop_count) * stride, 0);
            if (!TargetProcess->Read(list_base, part_records.data(), part_records.size()))
            {
                entry.next_refresh = now + std::chrono::seconds(2);
                entry.last_seen = now;
                return entry;
            }

            entry.parts.reserve(loop_count);
            for (uint32_t i = 0; i < loop_count; ++i)
            {
                const uint8_t* rec = part_records.data() + static_cast<size_t>(i) * stride;

                const uint32_t vert_count = dm_read_u32(rec, stride, offsets::damage_model::rec_vert_count);
                const uint32_t data_offset = dm_read_u32(rec, stride, offsets::damage_model::rec_geometry_rel);
                const uint32_t name_offset = dm_read_u32(rec, stride, offsets::damage_model::rec_name_offset);

                const uint16_t transform_index =
                    static_cast<uint16_t>(dm_read_u32(rec, stride, offsets::damage_model::rec_transform_index) & 0xFFFFu);

                // NUEVO: m_TreeIndex — índice al skeleton tree animado
                const uint16_t tree_index =
                    static_cast<uint16_t>(dm_read_u32(rec, stride, offsets::damage_model::rec_tree_index) & 0xFFFFu);

                if (vert_count == 0 || data_offset == 0xFFFFFFFFu)
                    continue;

                XrayPart part;
                if (decode_part(pool_base, name_pool_base, names_size,
                    vert_count, data_offset, name_offset,
                    transform_index, tree_index, part))
                {
                    entry.parts.push_back(std::move(part));
                }
            }

            entry.next_refresh = now + std::chrono::seconds(offsets::damage_model::cache_ttl_seconds);
        }

        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(15))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }
}