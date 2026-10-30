#pragma once
#include <corecrt_math_defines.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "ground_prediction.hpp"
#include "ballistic_validation.hpp"
#include "aircraft_ballistics_policy.hpp"
#include "aircraft_ammo_trace.hpp"
#include "../../core/render_work_policy.hpp"

namespace aimbot
{
    struct AimCacheEntry
    {
        vec3_t aim_point{};
        vec3_t raw_aim_point{};
        bool initialized = false;
        const char* status = "Esperando siguiente muestra";
        bool have_motion = false;
        uintptr_t owner = 0, movement = 0;
        vec3_t velocity{}, acceleration{}, last_position{};
        std::chrono::steady_clock::time_point last_seen{}, last_read{}, last_sample{}, last_solve{}, last_render{};
    };

    struct BallisticsData
    {
        float mass = 0.0f;
        float caliber = 0.0f;
        float velocity = 0.0f;
        float length = 0.0f;
        float drag = 0.0f;
        float max_dist = 0.0f;
        bool caliber_in_mm = false; // weapon tables convert to mm; global fallback retains meters
    };

    inline BallisticsData ballisticsData;
    inline const char* aircraft_ballistics_status="Sin muestra de municion";
    inline AircraftAmmoTrace aircraft_weapon_trace{};
    inline std::chrono::steady_clock::time_point aircraft_trace_sample{};
    inline AircraftAmmoDiagnostics aircraft_ammo_diag{};
    inline bool aircraft_global_fallback=false;

    // -----------------------------------------------------------------------
    // Utilidades de armamento
    // -----------------------------------------------------------------------
    struct WeaponInfoData {
        float mass = 0.0f;
        float caliber = 0.0f;
        float velocity = 0.0f;
        float length = 0.0f;
        float drag = 0.0f;
        float max_dist = 0.0f;
    };

    inline bool TryGetWeaponInfoFromGun(uintptr_t gun, WeaponInfoData& out,
        AircraftAmmoDiagnostics& diag,const char*& status)
    {
        diag.slot=-1;diag.ammo_count=-1;diag.props_id=0;diag.table_max_id=0;
        diag.cartridge_speed=0.f;diag.table_speed=0.f;
        diag.cartridge_read=false;diag.table_read=false;
        diag.table_pointer=false;diag.table_id_in_range=false;diag.table_entry=false;
        diag.array_state=diag.link_state=diag.cartridge_state=ammo_pointer_state::not_read;
        status="Puntero de canon invalido";
        if (!ballistic_validation::plausible_pointer(gun)) return false;

        uintptr_t wInfo = TargetProcess->Read<uintptr_t>(gun + offsets::weapon_offsets::weapon_info);
        status="WeaponInfo no disponible";
        if (!ballistic_validation::plausible_pointer(wInfo)) return false;

        status="Slot activo no legible";
        int8_t activeSlot=-1;
        if (!TargetProcess->ReadOptional(gun+offsets::weapon_offsets::active_slot,&activeSlot,sizeof(activeSlot))) return false;
        diag.slot=activeSlot;
        if (activeSlot < 0 || activeSlot > 31) {status="Slot activo fuera de rango";return false;}
        uintptr_t slotBase = wInfo + static_cast<uintptr_t>(static_cast<int>(activeSlot)) * 0xA0;

        int32_t ammoCount=-1;
        if (!TargetProcess->ReadOptional(slotBase+0xA8,&ammoCount,sizeof(ammoCount))) {
            status="Contador de cartuchos no legible";return false;
        }
        diag.ammo_count=ammoCount;
        if (ammoCount<0 || ammoCount>4096) {status="Contador de cartuchos fuera de rango";return false;}
        uintptr_t ammo=0;
        const auto read_pointer=[](uintptr_t address,void* out,size_t size) {
            return TargetProcess->ReadOptional(address,out,size);
        };
        if(!read_aircraft_cartridge(read_pointer,slotBase,offsets::weapon_info_offsets::ammo,
            ammoCount,diag,ammo,status)) return false;

        float speed=0.f;
        diag.cartridge_read=TargetProcess->ReadOptional(ammo+0x20,&speed,sizeof(speed));
        if(!diag.cartridge_read) speed=0.f;
        diag.cartridge_speed=speed;
        int propsId=0;
        status="ID de cartucho no legible";
        if (!TargetProcess->ReadOptional(ammo+0x4,&propsId,sizeof(propsId))) return false;
        if (propsId==-1 && !TargetProcess->ReadOptional(ammo,&propsId,sizeof(propsId))) return false;
        diag.props_id=propsId;

        const uintptr_t module_base = TargetProcess->GetBaseAddress("aces.exe");
        if (!module_base) return false;

        if (!aircraft_ballistics_policy::real_speed(speed)) {
            status=diag.cartridge_read ? "Velocidad del cartucho fuera de rango" : "Velocidad del cartucho no legible";
            if (propsId==0) {status="ID de proyectil cero: sin tabla";return false;}
            const auto table_offset=propsId<0 ? offsets::bullet_props_negative_offset
                : offsets::bullet_props_positive_offset;
            uintptr_t bulletTable=TargetProcess->Read<uintptr_t>(module_base+table_offset);
            diag.table_pointer=ballistic_validation::plausible_pointer(bulletTable);
            if (!diag.table_pointer) {status="Tabla de velocidades no disponible";return false;}
            const uint32_t id=static_cast<uint32_t>(propsId) & 0x7FFFFFFF;
            uint32_t maxId=0;
            status="Limite de tabla no legible";
            if (!TargetProcess->ReadOptional(bulletTable+0x60,&maxId,sizeof(maxId))) return false;
            diag.table_max_id=maxId;
            diag.table_id_in_range=id<maxId;
            if (!diag.table_id_in_range) {status="ID de proyectil fuera de tabla";return false;}
            uintptr_t pagePtr=TargetProcess->Read<uintptr_t>(bulletTable+(id>>27)*8);
            if (!ballistic_validation::plausible_pointer(pagePtr)) {
                status="Pagina de velocidades no disponible";return false;
            }
            uintptr_t entry=TargetProcess->Read<uintptr_t>(pagePtr+(id&0x7FFFFFF)*8);
            diag.table_entry=ballistic_validation::plausible_pointer(entry);
            if (!diag.table_entry) {status="Entrada de velocidad no disponible";return false;}
            diag.table_read=TargetProcess->ReadOptional(entry+0x20,&speed,sizeof(speed));
            if (!diag.table_read) speed=0.f;
            diag.table_speed=speed;
            status=diag.table_read ? "Velocidad de tabla fuera de rango" : "Velocidad de tabla no legible";
            if (!aircraft_ballistics_policy::real_speed(speed)) return false;
        }


        uintptr_t projTable = TargetProcess->Read<uintptr_t>(module_base + offsets::proj_ballistics_props_positive_offset);
        if (propsId < 0) projTable = TargetProcess->Read<uintptr_t>(module_base + offsets::proj_ballistics_props_negative_offset);

        float mass = 0.0f, caliber = 0.0f, drag = 0.0f, maxDist = 0.0f;

        if (projTable && propsId != 0) {
            uint32_t id = static_cast<uint32_t>(propsId) & 0x7FFFFFFF;
            uint32_t maxId = TargetProcess->Read<uint32_t>(projTable + 0x60);
            if (id < maxId) {
                uintptr_t pagePtr = TargetProcess->Read<uintptr_t>(projTable + (id >> 27) * 8);
                if (pagePtr) {
                    uintptr_t entry = TargetProcess->Read<uintptr_t>(pagePtr + (id & 0x7FFFFFF) * 8);
                    if (entry) {
                        mass = TargetProcess->Read<float>(entry + 0x4);
                        caliber = TargetProcess->Read<float>(entry + 0x8);
                        drag = TargetProcess->Read<float>(entry + 0xC);
                        maxDist = TargetProcess->Read<float>(entry + 0x10);
                    }
                }
            }
        }

        // La velocidad proviene del cartucho/tabla real. Las propiedades
        // aerodinámicas auxiliares pueden faltar en la tabla local; en ese caso
        // el solver desactiva drag, pero NO inventa ni descarta la velocidad.
        const bool have_geometry=aircraft_ballistics_policy::usable_drag_geometry(mass,caliber);
        if (!have_geometry) {mass=0.f;caliber=0.f;drag=0.f;}
        if (!std::isfinite(drag) || drag < 0.0f) drag = 0.0f;
        out.velocity = speed;
        out.mass = mass;
        out.caliber = caliber * 1000.0f;   // milímetros
        out.length = caliber;              // metros (aprox)
        out.drag = drag;
        out.max_dist = maxDist;
        return true;
    }

    inline bool GetAircraftBallisticsInfo(uintptr_t snapshot_owner=0)
    {
        auto& diag=aircraft_ammo_diag;
        diag={};
        aircraft_weapon_trace.reset();
        aircraft_trace_sample=std::chrono::steady_clock::now();
        const char*& status=aircraft_ballistics_status;
        const int weapon_index=sdk::air_lead_weapon_selection;
        const bool use_heaviest=sdk::air_lead_use_heaviest_weapon;
        status="Modulo del juego no disponible";
        const uintptr_t module_base = TargetProcess->GetBaseAddress("aces.exe");
        if (!module_base) return false;
        uintptr_t local_unit=snapshot_owner;
        if (!local_unit) {
            const uintptr_t raw=TargetProcess->Read<uintptr_t>(module_base+offsets::my_unit_offset);
            local_unit=raw & ~uintptr_t{1};
        }
        status="Avion local no disponible";
        if (!ballistic_validation::plausible_pointer(local_unit)) return false;
        uintptr_t armory=TargetProcess->Read<uintptr_t>(local_unit+offsets::unit_offsets::turret_offset);
        status="Armamento del avion no disponible";
        if (!ballistic_validation::plausible_pointer(armory)) return false;
        uintptr_t weapons=TargetProcess->Read<uintptr_t>(armory+offsets::armory_offsets::weapons);
        status="Lista de canones no disponible";
        if (!ballistic_validation::plausible_pointer(weapons)) return false;
        int32_t weapon_count=TargetProcess->Read<int32_t>(armory+offsets::armory_offsets::weapon_count);
        status="Cantidad de armas invalida";
        diag.weapon_count=weapon_count;
        if (weapon_count<=0 || weapon_count>128) return false;

        WeaponInfoData selected_info;
        AircraftAmmoDiagnostics selected_diag{};
        bool found = false;
        const auto attempt=[&](int index,WeaponInfoData& info) {
            const uintptr_t gun=TargetProcess->Read<uintptr_t>(weapons+index*sizeof(uintptr_t));
            ++diag.attempts;
            const bool ok=TryGetWeaponInfoFromGun(gun,info,diag,status);
            if(ok) status=info.mass>0.f && info.caliber>0.f
                ? "Municion valida" : "Velocidad valida sin masa/calibre";
            if(ok && use_heaviest && info.caliber<=0.f)
                status="Calibre no disponible para arma mayor";
            aircraft_weapon_trace.add(index,diag,status,ok);
            return ok;
        };

        if (weapon_index >= 0) {
            if (weapon_index >= weapon_count) {
                status="Indice de arma fuera de rango";return false;
            } // no elegir otra arma a escondidas
            found=attempt(weapon_index,selected_info);
            if(found) {selected_diag=diag;aircraft_weapon_trace.selected=weapon_index;}
        }
        else if (use_heaviest) {
            float best_caliber = -1.0f;
            for (int i = 0; i < weapon_count; ++i) {
                WeaponInfoData info;
                if (attempt(i,info)) {
                    if (info.caliber<=0.f) status="Calibre no disponible para arma mayor";
                    if (info.caliber > 0.f && info.caliber > best_caliber) {
                        best_caliber = info.caliber;
                        selected_info = info;
                        selected_diag=diag;aircraft_weapon_trace.selected=i;
                        found = true;
                    }
                }
            }
        }
        else {
            // automático: usar primera arma que funcione (comportamiento anterior)
            for (int i = 0; i < weapon_count; ++i) {
                if (attempt(i,selected_info)) {
                    selected_diag=diag;aircraft_weapon_trace.selected=i;
                    found = true;
                    break;
                }
            }
        }

        if (!found) return false;
        const int attempts=diag.attempts;
        diag=selected_diag;diag.attempts=attempts;

        BallisticsData& result=ballisticsData;
        result.velocity = selected_info.velocity;
        result.mass = selected_info.mass;
        result.caliber = selected_info.caliber;
        result.caliber_in_mm = true;
        result.length = selected_info.length;
        result.drag = selected_info.drag;
        result.max_dist = selected_info.max_dist;
        status=result.mass>0.f && result.caliber>0.f
            ? "Velocidad real + propiedades de municion"
            : "Velocidad real; sin drag (faltan masa/calibre)";
        return true;
    }

    inline bool GetLocalWeaponLocalPosition(vec3_t& out,uintptr_t snapshot_owner=0)
    {
        uintptr_t local_unit=snapshot_owner;
        if(!local_unit) {
            const uintptr_t module_base=TargetProcess->GetBaseAddress("aces.exe");
            if(!module_base)return false;
            const auto raw_unit=TargetProcess->Read<uintptr_t>(module_base+offsets::my_unit_offset);
            local_unit=(raw_unit&1) ? raw_unit-1 : raw_unit;
        }
        if(!local_unit)return false;

        uintptr_t armory = TargetProcess->Read<uintptr_t>(local_unit + offsets::unit_offsets::turret_offset);
        if (!armory) return false;

        uintptr_t weapon_pos_arr = TargetProcess->Read<uintptr_t>(armory + offsets::armory_offsets::weapon_position_info);
        int32_t   weapon_pos_count = TargetProcess->Read<int32_t>(armory + offsets::armory_offsets::weapon_position_info_count);
        if (!weapon_pos_arr || weapon_pos_count < 1) return false;

        if(!TargetProcess->Read(weapon_pos_arr + offsets::weapon_position_info_offsets::position,
            &out,sizeof(out))) return false;
        return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z) && out.length() < 100.0f;
    }

    // -----------------------------------------------------------------------
    // Obtener datos balísticos para tanques (cañón principal)
    // -----------------------------------------------------------------------
    inline bool GetBallisticsInfo()
    {
        // Sólo munición activa; escoger el cañón de mayor calibre daba el
        // cartucho equivocado y a veces una velocidad de otro proyectil.
        if (!sdk::cGame || !ballistic_validation::plausible_pointer(sdk::cGame->get_base())) {
            sdk::ground_ballistics_source = "Contenedor de juego no disponible";
            return false;
        }
        uintptr_t container=0;
        if (!TargetProcess->Read(sdk::cGame->get_base()+offsets::cgame_offsets::ballistics_offset,
                                 &container,sizeof(container)) ||
            !ballistic_validation::plausible_pointer(container)) {
            sdk::ground_ballistics_source = "No se pudo leer contenedor balístico";
            return false;
        }
        ballistic_validation::round_data round{};
        const auto read_float=[&](uintptr_t offset,float& value){
            return offset!=0 && offset<0x100000 &&
                   TargetProcess->Read(container+offset,&value,sizeof(value));
        };
        using namespace offsets::cgame_offsets::ballistic_offsets;
        if (!read_float(round_velocity_offset,round.speed) ||
            !read_float(round_mass_offset,round.mass) ||
            !read_float(round_caliber_offset,round.caliber_m) ||
            !read_float(round_length_offset,round.length_m) ||
            !ballistic_validation::normalize_caliber(round.caliber_m,round.caliber_m) ||
            !ballistic_validation::valid_round(round)) {
            sdk::ground_ballistics_source = "Lectura balística fallida o valores incoherentes";
            return false;
        }
        ballisticsData.velocity=round.speed;
        ballisticsData.mass=round.mass;
        ballisticsData.caliber=round.caliber_m;
        ballisticsData.length=round.length_m;
        ballisticsData.caliber_in_mm=false;
        ballisticsData.drag=0.f;
        ballisticsData.max_dist=0.f;
        sdk::ground_ballistics_source="Contenedor balístico: munición activa";
        return true;
    }

    // -----------------------------------------------------------------------
    // BeginFrame
    // -----------------------------------------------------------------------
    inline auto BeginFrame(bool local_is_plane, uintptr_t owner=0, uint64_t epoch=0) -> void
    {
        static std::chrono::steady_clock::time_point next_update{};
        static render_work_policy::failed_read_gate aircraft_retry;
        static const char* last_aircraft_status="Sin muestra de municion";
        static uintptr_t last_owner=0;static uint64_t last_epoch=0;
        static int last_weapon=-2;static bool last_heaviest=false,last_plane=false;
        const auto now = std::chrono::steady_clock::now();
        const bool changed=owner!=last_owner || epoch!=last_epoch || local_is_plane!=last_plane ||
            sdk::air_lead_weapon_selection!=last_weapon || sdk::air_lead_use_heaviest_weapon!=last_heaviest;
        if (!changed && now < next_update) return;
        last_owner=owner;last_epoch=epoch;last_plane=local_is_plane;
        last_weapon=sdk::air_lead_weapon_selection;last_heaviest=sdk::air_lead_use_heaviest_weapon;

        ballisticsData = {}; // Never reuse ballistics from a previous unit on failure.
        aircraft_global_fallback=false;
        if (local_is_plane) {
            bool aircraft_ok=false;
            if(aircraft_retry.due(now,changed)) {
                aircraft_ok=GetAircraftBallisticsInfo(owner);
                aircraft_retry.record(now,aircraft_ok);
                last_aircraft_status=aircraft_ballistics_status;
            }
            aircraft_ballistics_status=last_aircraft_status;
            if (aircraft_ok) {
                sdk::ground_ballistics_source="Tabla de municion del avion";
            } else if (sdk::air_lead_weapon_selection<0 &&
                !sdk::air_lead_use_heaviest_weapon && GetBallisticsInfo()) {
                // Ruta histórica: el contenedor local de la munición activa
                // podía entregar velocidad aun si la tabla del cañón fallaba.
                // Nunca sustituir una selección explícita por otra arma.
                aircraft_global_fallback=true;
                aircraft_ballistics_status="Contenedor balistico local: municion activa (origen no confirmado)";
            }
        } else GetBallisticsInfo();

        next_update = now + std::chrono::milliseconds(50);
    }

    inline bool ReadGroundVelocity(uintptr_t base, vec3_t& velocity, uintptr_t& source)
    {
        source=0;
        const auto reader=[](uintptr_t address,void* data,size_t size) {
            return TargetProcess->Read(address,data,size);
        };
        if(ballistic_validation::read_ground_velocity(base,
            offsets::unit_offsets::ground_velocity_offset,
            offsets::unit_offsets::groundmovement_velocity_offset,
            reader,velocity,source)) {
            sdk::ground_velocity_source="Unit.m_Velocity -> GroundMovement.m_Velocity";
            return true;
        }
        sdk::ground_velocity_source="Lectura fallida: puntero/contenedor/vec3";
        return false;
    }

    // Only the selected ground target reaches this path: one fixed-size cache.
    // Solving and DMA are paced independently of rendering (60/120 ms).
    inline auto run(c_unit& unit, vec3_t unit_position, vec3_t local_position,
        ViewMatrix_t camera_matrix, std::chrono::steady_clock::time_point captured_at) -> void {
        static AimCacheEntry cache{};
        static std::chrono::steady_clock::time_point next_work{}; // survives target switches
        const auto now = std::chrono::steady_clock::now();
        const auto base = unit.get_base();
        if (cache.owner != base || now-cache.last_seen > std::chrono::milliseconds(500)) {
            cache = {};
            cache.owner = base;
        }
        cache.last_seen = now;
        sdk::ground_prediction_status = cache.status;
        struct StatusGuard { AimCacheEntry& cache; ~StatusGuard() { cache.status = sdk::ground_prediction_status; } } status_guard{cache};
        if (!ground_prediction::finite(unit_position) || !ground_prediction::finite(local_position)) {
            cache.initialized = false; sdk::ground_prediction_status = "Posicion invalida"; return;
        }
        const float sample_age = std::chrono::duration<float>(now-captured_at).count();
        if (sample_age < 0 || sample_age > 0.25f) { cache.initialized = false; sdk::ground_prediction_status = "Snapshot atrasado (>250 ms)"; return; }
        // Cheap discontinuity check on every frame, before reusing a previous marker.
        const float position_dt = std::chrono::duration<float>(captured_at-cache.last_sample).count();
        if (cache.have_motion && (position_dt < 0 ||
            (unit_position-cache.last_position).length() > 5.0f + cache.velocity.length()*std::max(0.0f,position_dt)*2.0f)) {
            cache.have_motion = false;
            cache.initialized = false;
            cache.acceleration = {};
        }
        if (now >= next_work) {
            next_work = now + std::chrono::milliseconds(60); // throttle failures and target switches too
            uintptr_t movement = 0;
            vec3_t velocity{};
            if (!ReadGroundVelocity(base, velocity, movement)) {
                sdk::ground_prediction_status = "No se pudo leer velocidad terrestre";
                cache.initialized = false;
                cache.have_motion = false;
                return;
            }
            const float dt = std::chrono::duration<float>(now-cache.last_read).count();
            if (cache.movement != movement) {
                cache.have_motion = false;
                cache.initialized = false;
            }
            if (cache.have_motion && dt >= 0.025f && dt <= 0.3f) {
                vec3_t measured = (velocity-cache.velocity)/dt;
                measured.y = 0;
                if (ground_prediction::finite(measured) && measured.length() < 25.0f) {
                    const float alpha = 1.0f-std::exp(-dt/0.18f);
                    cache.acceleration += (measured-cache.acceleration)*alpha;
                } else cache.acceleration = {};
            } else cache.acceleration = {};
            cache.movement = movement;
            cache.velocity = velocity;
            cache.last_read = now;
            cache.last_sample = captured_at;
            cache.last_position = unit_position;
            cache.have_motion = true;
            const float speed = velocity.length();
            const vec3_t historical_acceleration = cache.acceleration;
            if (speed < 0.2f) cache.acceleration = {};
            next_work = now + std::chrono::milliseconds(speed > 0.1f ? 60 : 120);
            // Preserve existing marker height; ballistic drop is handled only by the solver.
            const float dx = unit_position.x-local_position.x, dz = unit_position.z-local_position.z;
            vec3_t target = ground_prediction::align_position(unit_position,velocity,historical_acceleration,sample_age);
            target.y += 1.0f + std::sqrt(dx*dx+dz*dz)*(0.143f/150.0f);
            const ground_prediction::Ballistics b{ballisticsData.velocity, ballisticsData.mass,
                ballisticsData.caliber * (ballisticsData.caliber_in_mm ? 0.001f : 1.0f),
                ballisticsData.length, ballisticsData.drag};
            if (!std::isfinite(b.speed) || b.speed < 50.0f || b.speed > 4000.0f) {
                cache.initialized = false; sdk::ground_prediction_status = "Velocidad de municion invalida"; return;
            }
            const auto solution = ground_prediction::solve(local_position,target,velocity,cache.acceleration,b);
            if (!solution.valid) { cache.initialized = false; sdk::ground_prediction_status = "Sin solucion balistica convergente"; return; }
            cache.raw_aim_point = solution.point;
            cache.last_solve = now;
            if (!cache.initialized) {
                cache.aim_point = solution.point;
                cache.last_render = now;
                cache.initialized = true;
            }
        }
        if (!cache.initialized) return;
        const float render_dt = std::clamp(std::chrono::duration<float>(now-cache.last_render).count(),0.0f,0.1f);
        const float raw_age = std::clamp(std::chrono::duration<float>(now-cache.last_solve).count(),0.0f,0.15f);
        cache.last_render = now;
        const vec3_t live = cache.raw_aim_point + cache.velocity*raw_age;
        // Time-invariant smoothing with velocity feed-forward, not a frame-dependent lag.
        cache.aim_point += cache.velocity*render_dt;
        cache.aim_point += (live-cache.aim_point)*(1.0f-std::exp(-render_dt/0.045f));
        vec2_t screen;
        if (!g_render->world_to_screen(cache.aim_point,screen,camera_matrix)) {
            sdk::ground_prediction_status = "Prediccion detras de la camara"; return;
        }
        const ImGuiViewport* viewport=ImGui::GetMainViewport();
        if (!viewport || !g_render->draw_list()) {
            sdk::ground_prediction_status = "Monitor ESP no disponible"; return;
        }
        sdk::ground_prediction_screen_x=screen.x;
        sdk::ground_prediction_screen_y=screen.y;
        sdk::ground_prediction_viewport_width=viewport->Size.x;
        sdk::ground_prediction_viewport_height=viewport->Size.y;
        sdk::ground_prediction_screen_known=true;
        if (!ground_prediction::screen_visible(screen.x,screen.y,viewport->Pos.x,viewport->Pos.y,
            viewport->Size.x,viewport->Size.y)) {
            sdk::ground_prediction_status = "Solucion fuera del monitor ESP"; return;
        }
        vec2_t unit_screen{};
        if(g_render->world_to_screen(unit_position,unit_screen,camera_matrix) &&
            ground_prediction::screen_visible(unit_screen.x,unit_screen.y,viewport->Pos.x,viewport->Pos.y,
                viewport->Size.x,viewport->Size.y))
            g_render->line(unit_screen.x,unit_screen.y,screen.x,screen.y,
                IM_COL32(255,255,255,225),1.f);
        g_render->circle_filled(screen.x,screen.y,3.f,IM_COL32(255,0,0,255),12);
        sdk::ground_prediction_status = "Dibujando";
    }
}
