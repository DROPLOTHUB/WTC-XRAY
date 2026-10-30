#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <vector>
#include "../../core/runtime_context.hpp"
#include "projectile_ecs.hpp"
#include "projectile_visibility.hpp"

namespace projectile_esp
{
    struct Projectile
    {
        vec3_t position;
        vec3_t velocity;
        float speed;
        bool is_bomb;
        char name[48]{};
    };

    namespace detail
    {
        inline constexpr size_t max_projectiles = 998;
        inline std::chrono::steady_clock::duration sample_interval = std::chrono::milliseconds(100);
        inline constexpr double max_extrapolation_seconds = 0.3;

        // Origen de la posicion del jugador local usada por el filtro de entrantes.
        enum class LocalSource { none = 0, confirmed = 1, direct = 2 };

        struct Track
        {
            projectile_ecs::Candidate sample;
            std::chrono::steady_clock::time_point sampled_at;
            bool confirmed = false;
        };

        struct State
        {
            std::vector<Projectile> projectiles;
            std::vector<projectile_ecs::Candidate> candidates;
            std::vector<Track> tracks;
            std::vector<Track> next_tracks;
            // Indices only: rebuilt before each sample merge; never retain Track pointers.
            std::vector<std::size_t> previous_track_index;
            std::array<projectile_ecs::Diagnostics, 2> diagnostics{};
            uintptr_t base = 0;
            uintptr_t rocket_rva = 0;
            uintptr_t bomb_rva = 0;
            uintptr_t manager_rva = 0;
            uintptr_t process = 0;
            std::array<bool, 2> had_candidates{};
            vec3_t local_position{};
            bool local_valid = false;
            LocalSource local_source = LocalSource::none;
            bool warned_local = false;
            std::size_t filtered = 0;
            int adaptive_budget = 0;
            float scan_ema_us = 0.f;
            unsigned scan_samples = 0;
            std::chrono::steady_clock::time_point sampled_at{};
            std::chrono::steady_clock::time_point next_log{};

            State()
            {
                projectiles.reserve(max_projectiles);
                candidates.reserve(max_projectiles);
                tracks.reserve(max_projectiles);
                next_tracks.reserve(max_projectiles);
                previous_track_index.reserve(max_projectiles);
            }
        };

        inline State& state()
        {
            static thread_local State value;
            return value;
        }
    }

    inline bool is_heap_ptr(uintptr_t p)
    {
        return projectile_ecs::valid_pointer(p);
    }

    inline void clear_cache()
    {
        auto& s = detail::state();
        s.projectiles.clear();
        s.candidates.clear();
        s.tracks.clear();
        s.next_tracks.clear();
        s.previous_track_index.clear();
        s.diagnostics = {};
        s.base = 0;
        s.rocket_rva = 0;
        s.bomb_rva = 0;
        s.manager_rva = 0;
        s.process = 0;
        s.had_candidates = {};
        s.local_position = {};
        s.local_valid = false;
        s.local_source = detail::LocalSource::none;
        s.warned_local = false;
        s.filtered = 0;
        s.adaptive_budget = 0;
        s.scan_ema_us = 0.f;
        s.scan_samples = 0;
        misc::prof_projectile_scan_us.store(0,std::memory_order_relaxed);
        misc::prof_projectile_adaptive_budget.store(0,std::memory_order_relaxed);
        misc::prof_projectile_reads.store(0,std::memory_order_relaxed);
        s.sampled_at = {};
    }

    namespace detail
    {
        template<class T>
        inline uintptr_t process_identity(const T& value)
        {
            uintptr_t identity = 0;
            if constexpr (requires { value.get(); })
                identity = reinterpret_cast<uintptr_t>(value.get());
            else
                identity = reinterpret_cast<uintptr_t>(value);
            if constexpr (requires { value->GetProcessID(); })
            {
                const auto pid = value ? static_cast<unsigned>(value->GetProcessID()) : 0u;
                identity ^= static_cast<uintptr_t>(pid) * 0x9E3779B97F4A7C15ULL;
            }
            return identity;
        }

        inline constexpr double min_miss_distance = 300.0;

        template<class Position, class Velocity, class Target>
        inline bool incoming(const Position& position, const Velocity& velocity, const Target& target,
            float max_distance, float min_cos)
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
                !std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z) ||
                !std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z) ||
                !std::isfinite(max_distance) || !std::isfinite(min_cos))
                return false;
            const double dx = static_cast<double>(target.x) - position.x;
            const double dy = static_cast<double>(target.y) - position.y;
            const double dz = static_cast<double>(target.z) - position.z;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(distance) || distance < 1.0 || distance > max_distance)
                return false;
            const double speed_sq = static_cast<double>(velocity.x) * velocity.x +
                static_cast<double>(velocity.y) * velocity.y +
                static_cast<double>(velocity.z) * velocity.z;
            if (!std::isfinite(speed_sq) || speed_sq <= 0.0)
                return false;
            const double closing = static_cast<double>(velocity.x) * dx +
                static_cast<double>(velocity.y) * dy +
                static_cast<double>(velocity.z) * dz;
            if (closing <= 0.0)
                return false;
            const double speed = std::sqrt(speed_sq);
            const double cos_angle = closing / (speed * distance);
            if (std::isfinite(cos_angle) && cos_angle >= min_cos)
                return true;
            const double time_to_closest = closing / speed_sq;
            const double miss_x = dx - static_cast<double>(velocity.x) * time_to_closest;
            const double miss_y = dy - static_cast<double>(velocity.y) * time_to_closest;
            const double miss_z = dz - static_cast<double>(velocity.z) * time_to_closest;
            const double miss = std::sqrt(miss_x * miss_x + miss_y * miss_y + miss_z * miss_z);
            return std::isfinite(miss) && miss <= min_miss_distance;
        }

        inline float min_cos_from_degrees(float degrees)
        {
            constexpr double pi = 3.14159265358979323846;
            if (!std::isfinite(degrees))
                return -1.0f;
            const double clamped = (std::min)(180.0, (std::max)(0.0, static_cast<double>(degrees)));
            return static_cast<float>(std::cos(clamped * pi / 180.0));
        }

        inline bool read_local_position(uintptr_t base, vec3_t& out)
        {
            uintptr_t raw = 0;
            if (!TargetProcess->Read(base + offsets::my_unit_offset, &raw, sizeof(raw)) || raw == 0)
                return false;
            const uintptr_t local = (raw & 1) ? raw - 1 : raw;
            if (!startup_pointer_probe::plausible_pointer(local))
                return false;
            vec3_t position{};
            if (!TargetProcess->Read(local + offsets::unit_offsets::position_offset, &position, sizeof(position)))
                return false;
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
                return false;
            out = position;
            return true;
        }

        inline bool is_zero_vec(const vec3_t& v)
        {
            return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
        }

        // Posicion del jugador local para el filtro de entrantes.
        // Prefiere el jugador local CONFIRMADO del snapshot del ESP (la misma
        // fuente que usa el radar/aimbot); si no esta disponible, cae a la
        // lectura directa por my_unit_offset. Nunca deja el filtro sin posicion
        // si una de las dos vias funciona.
        inline LocalSource resolve_local_position(uintptr_t base, const vec3_t* confirmed, vec3_t& out)
        {
            if (sdk::projectile_incoming_use_localplayer && confirmed &&
                std::isfinite(confirmed->x) && std::isfinite(confirmed->y) &&
                std::isfinite(confirmed->z) && !is_zero_vec(*confirmed))
            {
                out = *confirmed;
                return LocalSource::confirmed;
            }
            if (read_local_position(base, out))
                return LocalSource::direct;
            return LocalSource::none;
        }

        inline const char* local_source_name(LocalSource source)
        {
            switch (source)
            {
            case LocalSource::confirmed: return "conf";
            case LocalSource::direct: return "raw";
            default: return "none";
            }
        }

        inline void emit_confirmed(std::vector<Projectile>& out, const std::vector<Track>& tracks,
            std::chrono::steady_clock::time_point now)
        {
            for (const auto& track : tracks)
            {
                if (!track.confirmed)
                    continue;
                double age = std::chrono::duration<double>(now - track.sampled_at).count();
                if (!(age >= 0.0))
                    age = 0.0;
                if (age > max_extrapolation_seconds)
                    continue;
                Projectile entry{};
                entry.position = vec3_t{
                    static_cast<float>(track.sample.position.x + track.sample.velocity.x * age),
                    static_cast<float>(track.sample.position.y + track.sample.velocity.y * age),
                    static_cast<float>(track.sample.position.z + track.sample.velocity.z * age)};
                entry.velocity = vec3_t{
                    track.sample.velocity.x, track.sample.velocity.y, track.sample.velocity.z};
                entry.speed = track.sample.speed;
                entry.is_bomb = track.sample.is_bomb;
                std::snprintf(entry.name, sizeof(entry.name), "%s", track.sample.name);
                out.push_back(entry);
            }
        }

        inline bool finite(const vec3_t& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        inline bool project(const vec3_t& position, const ViewMatrix_t& matrix,
            ImVec2 screen_size, ImVec2& out)
        {
            if (!finite(position) || !std::isfinite(screen_size.x) ||
                !std::isfinite(screen_size.y) || screen_size.x <= 0.0f || screen_size.y <= 0.0f)
                return false;
            const float x = position.x * matrix[0][0] + position.y * matrix[1][0] +
                position.z * matrix[2][0] + matrix[3][0];
            const float y = position.x * matrix[0][1] + position.y * matrix[1][1] +
                position.z * matrix[2][1] + matrix[3][1];
            const float w = position.x * matrix[0][3] + position.y * matrix[1][3] +
                position.z * matrix[2][3] + matrix[3][3];
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0.0f)
                return false;
            out.x = (x / w + 1.0f) * screen_size.x * 0.5f;
            out.y = (1.0f - y / w) * screen_size.y * 0.5f;
            return std::isfinite(out.x) && std::isfinite(out.y);
        }
    }

    inline void read_all(uintptr_t base, std::vector<Projectile>& out,
        const vec3_t* confirmed_local = nullptr)
    {
        auto& s = detail::state();
        const uintptr_t rocket_rva = offsets::Global::g_RocketListIndexPtr;
        const uintptr_t bomb_rva = offsets::Global::g_BombListIndexPtr;
        const uintptr_t manager_rva = offsets::Global::g_EntityManager;
        const uintptr_t process = detail::process_identity(TargetProcess);
        if (s.base != base || s.rocket_rva != rocket_rva ||
            s.bomb_rva != bomb_rva || s.manager_rva != manager_rva ||
            s.process != process)
            clear_cache();
        s.base = base;
        s.rocket_rva = rocket_rva;
        s.bomb_rva = bomb_rva;
        s.manager_rva = manager_rva;
        s.process = process;
        out.clear();
        if (out.capacity() < detail::max_projectiles)
            out.reserve(detail::max_projectiles);

        if (!is_heap_ptr(base) || !TargetProcess)
        {
            s.tracks.clear();
            s.filtered = 0;
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (s.sampled_at.time_since_epoch().count() != 0 && now - s.sampled_at < detail::sample_interval)
        {
            detail::emit_confirmed(out, s.tracks, now);
            return;
        }
        s.sampled_at = now;
        s.candidates.clear();
        s.next_tracks.clear();
        s.previous_track_index.clear();
        s.diagnostics = {};
        s.filtered = 0;
        s.local_source = detail::LocalSource::none;
        if (sdk::projectile_incoming_only)
            s.local_source = detail::resolve_local_position(base, confirmed_local, s.local_position);
        s.local_valid = (s.local_source != detail::LocalSource::none);
        if (sdk::projectile_incoming_only)
        {
            if (!s.local_valid)
            {
                if (!s.warned_local)
                {
                    s.warned_local = true;
                    LOG("[!] Incoming-missile filter is enabled but the local unit position could not be read "
                        "(ni confirmada del snapshot ni directa); no projectile will be drawn.\n");
                }
            }
            else
                s.warned_local = false;
        }

        projectile_ecs::Layout layout{};
        layout.manager_rva = manager_rva;
        layout.rocket_selector_rva = rocket_rva;
        layout.bomb_selector_rva = bomb_rva;
        layout.missile_name_offset = offsets::projectile_offsets::missile_name_container;
        layout.bomb_name_offset = offsets::projectile_offsets::bomb_name_container;
        layout.bomb_name_text_offset = offsets::projectile_offsets::bomb_name_text;
        layout.position_offset = offsets::projectile_offsets::position;
        layout.velocity_offset = offsets::projectile_offsets::velocity;
        layout.owner_offset = offsets::projectile_offsets::owner;
        // Cadena mas reciente: Projectile::m_MissileNameCont (+0x720)
        // -> MissileNameCont::m_Type (+0x870), sin probar el esquema anterior.
        layout.missile_name_text_offset = offsets::projectile_offsets::missile_name_type;
        const int user_limit = std::clamp(sdk::projectile_scan_budget,256,4096);
        if(s.adaptive_budget < 256) s.adaptive_budget = user_limit;
        s.adaptive_budget = std::clamp(s.adaptive_budget,256,user_limit);
        layout.max_reads = static_cast<std::uintptr_t>(s.adaptive_budget);
        layout.aam_only = sdk::projectile_aam_only;
        layout.name_allowed = projectile_visibility::callback;
        layout.name_context = &layout.aam_only;
        auto reader = [](uintptr_t address, void* destination, size_t size) -> bool
        {
            // ECS puede desaparecer entre snapshots; informar en debug, sin
            // emitir un error DMA por cada registro disperso no mapeado.
            return TargetProcess->ReadOptional(address, destination, size);
        };
        const auto scan_start=std::chrono::steady_clock::now();
        projectile_ecs::read_query(reader, base, layout, false, s.diagnostics[0], s.candidates);
        if (sdk::projectile_aam_only)
        {
            // Con solo-aire-aire la lista de bombas no puede aportar nada: no se
            // escanea (mitad de lecturas) y su linea de log sale como skipped.
            s.diagnostics[1] = {};
            s.had_candidates[1] = false;
        }
        else {
            // Keep the original per-list limit: starving bombs would discard
            // their tracking history and prevent consecutive confirmations.
            projectile_ecs::read_query(reader,base,layout,true,s.diagnostics[1],s.candidates);
        }
        const auto scan_us=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()-scan_start).count());
        const uint64_t reads=static_cast<uint64_t>(s.diagnostics[0].read_calls)+s.diagnostics[1].read_calls;
        misc::prof_projectile_scan_us.store(scan_us,std::memory_order_relaxed);
        misc::prof_projectile_reads.store(reads,std::memory_order_relaxed);
        misc::prof_projectile_adaptive_budget.store(s.adaptive_budget,std::memory_order_relaxed);
        s.scan_ema_us = s.scan_ema_us > 0.f ? s.scan_ema_us*.75f + static_cast<float>(scan_us)*.25f : static_cast<float>(scan_us);
        if(++s.scan_samples >= 4) {
            s.scan_samples=0;
            if(s.scan_ema_us > 3500.f && s.adaptive_budget > 256)
                s.adaptive_budget=(std::max)(256,s.adaptive_budget-s.adaptive_budget/8);
            else if(s.scan_ema_us < 1400.f && s.adaptive_budget < user_limit && reads >= static_cast<uint64_t>(s.adaptive_budget/2))
                s.adaptive_budget=(std::min)(user_limit,s.adaptive_budget+128);
        }

        // Sort indices, not tracks: retain draw order and reuse allocated storage.
        const auto track_key = [](const projectile_ecs::Candidate& candidate) {
            return std::make_tuple(candidate.source_id, candidate.owner, candidate.is_bomb);
        };
        s.previous_track_index.resize(s.tracks.size());
        for (std::size_t i = 0; i < s.tracks.size(); ++i)
            s.previous_track_index[i] = i;
        std::sort(s.previous_track_index.begin(), s.previous_track_index.end(),
            [&](std::size_t a, std::size_t b) {
                const auto ka = track_key(s.tracks[a].sample);
                const auto kb = track_key(s.tracks[b].sample);
                return ka < kb || (ka == kb && a < b); // preserve first-match semantics
            });
        const float incoming_min_cos = sdk::projectile_incoming_only
            ? detail::min_cos_from_degrees(sdk::projectile_incoming_angle) : 0.0f;
        for (const auto& candidate : s.candidates)
        {
            if (sdk::projectile_incoming_only)
            {
                if (candidate.is_bomb)
                {
                    ++s.filtered;
                    continue;
                }
                if (!s.local_valid ||
                    !detail::incoming(candidate.position, candidate.velocity, s.local_position,
                        sdk::projectile_incoming_distance,
                        incoming_min_cos))
                {
                    ++s.filtered;
                    continue;
                }
            }
            const auto duplicate = std::find_if(s.next_tracks.begin(), s.next_tracks.end(),
                [&](const detail::Track& track)
                {
                    return track.sample.source_id == candidate.source_id &&
                        track.sample.owner == candidate.owner &&
                        track.sample.is_bomb == candidate.is_bomb;
                });
            if (duplicate != s.next_tracks.end())
                continue;

            detail::Track fresh{candidate, now, false};
            const auto key = track_key(candidate);
            const auto found = std::lower_bound(s.previous_track_index.begin(),
                s.previous_track_index.end(), key,
                [&](std::size_t index, const auto& wanted) {
                    return track_key(s.tracks[index].sample) < wanted;
                });
            if (found != s.previous_track_index.end() &&
                track_key(s.tracks[*found].sample) == key)
            {
                const auto* previous = &s.tracks[*found];
                const double dt = std::chrono::duration<double>(now - previous->sampled_at).count();
                if (dt > 0.0 && dt <= 0.5)
                {
                    const auto& position = previous->sample.position;
                    const double delta = std::hypot(
                        static_cast<double>(candidate.position.x) - position.x,
                        static_cast<double>(candidate.position.y) - position.y,
                        static_cast<double>(candidate.position.z) - position.z);
                    const double bound = (std::max)(previous->sample.speed, candidate.speed) * dt * 4.0 + 10.0;
                    if (std::isfinite(delta) && delta <= bound)
                        fresh.confirmed = previous->confirmed || delta > 0.001;
                }
            }
            s.next_tracks.push_back(fresh);
        }
        s.previous_track_index.clear(); // indices refer to the old tracks until this swap
        s.tracks.swap(s.next_tracks);
        detail::emit_confirmed(out, s.tracks, now);

        bool force_log = false;
        for (size_t i = 0; i < s.diagnostics.size(); ++i)
        {
            const bool has_candidates = s.diagnostics[i].accepted > 0;
            if (has_candidates && !s.had_candidates[i])
                force_log = true;
            s.had_candidates[i] = has_candidates;
        }

        if (sdk::projectile_debug && (force_log || now >= s.next_log))
        {
            s.next_log = now + std::chrono::seconds(5);
            for (size_t i = 0; i < s.diagnostics.size(); ++i)
            {
                const auto& d = s.diagnostics[i];
                if (i == 1 && sdk::projectile_aam_only)
                {
                    LOG("ECS EXPERIMENTAL kind=bomb stage=skipped (aam-only)\n");
                    continue;
                }
                const auto rva = i == 0 ? rocket_rva : bomb_rva;
                const bool want_bomb = (i == 1);
                const projectile_ecs::Candidate* first = nullptr;
                for (const auto& candidate : s.candidates)
                {
                    if (candidate.is_bomb == want_bomb)
                    {
                        first = &candidate;
                        break;
                    }
                }
                char first_text[256]{};
                if (first)
                    std::snprintf(first_text, sizeof(first_text),
                        "pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) spd=%.1f owner=0x%llX src=0x%llX name=\"%s\"",
                        first->position.x, first->position.y, first->position.z,
                        first->velocity.x, first->velocity.y, first->velocity.z,
                        first->speed,
                        static_cast<unsigned long long>(first->owner),
                        static_cast<unsigned long long>(first->source_id),
                        first->name[0] != '\0' ? first->name : "-");
                else
                    std::snprintf(first_text, sizeof(first_text), "none");
                char header[2 * 32 + 1]{};
                char descriptor[2 * 32 + 1]{};
                for (size_t b = 0; b < d.header.size(); ++b)
                    std::snprintf(header + b * 2, 3, "%02X", static_cast<unsigned>(d.header[b]));
                for (size_t b = 0; b < d.descriptor.size(); ++b)
                    std::snprintf(descriptor + b * 2, 3, "%02X", static_cast<unsigned>(d.descriptor[b]));
                static constexpr const char* name_stage[] = {"none","range","container-read","container-pointer","field-range","field-read","string-pointer","string-read","empty","ok"};
                const char* probe = d.name_probe_stage < std::size(name_stage) ? name_stage[d.name_probe_stage] : "unknown";
                LOG("ECS EXPERIMENTAL kind=%s BASE=0x%llX RVA=0x%llX mgrrva=0x%llX selector=%u raw=0x%08X record=0x%llX sublists=%u visited=%u rows=%u active=%u candidates=%u rejected=%u truncated=%u reads=%u stage=%s req=%u opt=%u sub=%u o0=%u o1=%u data=0x%llX index=0x%llX first=%s names=%u nameoff=0x%llX namefield=0x%llX probes=%u namestage=%s namesrc=0x%llX namecont=0x%llX nameptr=0x%llX rawname=\"%s\" local=%d localsrc=%s filtered=%u mmignored=%u notaam=%u notaamname=\"%s\" head=%s desc=%s\n",
                    i == 0 ? "rocket" : "bomb",
                    static_cast<unsigned long long>(base),
                    static_cast<unsigned long long>(rva),
                    static_cast<unsigned long long>(manager_rva),
                    static_cast<unsigned>(d.selector),
                    static_cast<unsigned>(d.raw_selector),
                    static_cast<unsigned long long>(d.record),
                    static_cast<unsigned>(d.sublist_count),
                    static_cast<unsigned>(d.sublists_visited),
                    static_cast<unsigned>(d.rows_seen),
                    static_cast<unsigned>(d.active_rows),
                    static_cast<unsigned>(d.accepted),
                    static_cast<unsigned>(d.rejected),
                    static_cast<unsigned>(d.truncated),
                    static_cast<unsigned>(d.read_calls),
                    projectile_ecs::failure_name(d.failure),
                    static_cast<unsigned>(d.required),
                    static_cast<unsigned>(d.optional),
                    static_cast<unsigned>(d.subcount),
                    static_cast<unsigned>(d.offset0),
                    static_cast<unsigned>(d.offset1),
                    static_cast<unsigned long long>(d.data_table),
                    static_cast<unsigned long long>(d.index_table),
                    first_text,
                    static_cast<unsigned>(d.names_read),
                    static_cast<unsigned long long>(d.name_offset),
                    static_cast<unsigned long long>(d.name_text_offset),
                    static_cast<unsigned>(d.name_probes),
                    probe,
                    static_cast<unsigned long long>(d.name_probe_source),
                    static_cast<unsigned long long>(d.name_probe_container),
                    static_cast<unsigned long long>(d.name_probe_text),
                    d.raw_name,
                    s.local_valid ? 1 : 0,
                    detail::local_source_name(s.local_source),
                    static_cast<unsigned>(s.filtered),
                    static_cast<unsigned>(d.caliber_ignored),
                    static_cast<unsigned>(d.not_aam),
                    d.not_aam_name,
                    header,
                    descriptor);
            }
        }
    }

    inline void draw(ImDrawList* dl, const ViewMatrix_t& view_matrix, ImVec2 screen_size,
        const vec3_t* confirmed_local = nullptr)
    {
        if (!sdk::projectile_esp_enabled || !dl || !std::isfinite(screen_size.x) ||
            !std::isfinite(screen_size.y) || screen_size.x <= 0.0f || screen_size.y <= 0.0f)
        {
            clear_cache();
            return;
        }
        const auto* viewport = ImGui::GetMainViewport();
        if (!viewport || !std::isfinite(viewport->Pos.x) || !std::isfinite(viewport->Pos.y))
        {
            clear_cache();
            return;
        }
        const uintptr_t base = runtime::base_address();
        if (!is_heap_ptr(base))
        {
            clear_cache();
            return;
        }
        auto& s = detail::state();
        read_all(base, s.projectiles, confirmed_local);
        for (const auto& projectile : s.projectiles)
        {
            vec3_t end{};
            end.x = projectile.position.x + projectile.velocity.x * 0.5f;
            end.y = projectile.position.y + projectile.velocity.y * 0.5f;
            end.z = projectile.position.z + projectile.velocity.z * 0.5f;
            ImVec2 start_screen{};
            ImVec2 end_screen{};
            if (!detail::project(projectile.position, view_matrix, screen_size, start_screen) ||
                !detail::project(end, view_matrix, screen_size, end_screen))
                continue;
            if (start_screen.x < -200.0f || start_screen.x > screen_size.x + 200.0f ||
                start_screen.y < -200.0f || start_screen.y > screen_size.y + 200.0f ||
                end_screen.x < -200.0f || end_screen.x > screen_size.x + 200.0f ||
                end_screen.y < -200.0f || end_screen.y > screen_size.y + 200.0f)
                continue;
            start_screen.x += viewport->Pos.x;
            start_screen.y += viewport->Pos.y;
            end_screen.x += viewport->Pos.x;
            end_screen.y += viewport->Pos.y;
            if (!std::isfinite(start_screen.x) || !std::isfinite(start_screen.y) ||
                !std::isfinite(end_screen.x) || !std::isfinite(end_screen.y))
                continue;
            const ImU32 color = projectile.is_bomb ? IM_COL32(255, 60, 60, 200) : IM_COL32(255, 140, 0, 200);
            dl->AddLine(start_screen, end_screen, color, 2.0f);
            dl->AddCircleFilled(start_screen, 2.5f, color, 6);
            if (projectile.name[0] != '\0')
                dl->AddText(ImVec2(start_screen.x + 5.0f, start_screen.y - 15.0f), color, projectile.name);
        }
    }
}
