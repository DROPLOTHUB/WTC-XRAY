#pragma once
#include "../../core/render_work_policy.hpp"
#include "../../core/reusable_snapshot.hpp"
#include "../../core/scatter_read_recovery.hpp"
#include "../esp/unit_type_filter.hpp"
#include "..\\..\\core\\runtime_context.hpp"
#include <shared_mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cfloat>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>
#include <chrono>

namespace misc
{
	inline std::vector< c_unit > unitsList;
	inline std::shared_mutex units_mutex;
	inline std::atomic<uint64_t> units_revision{0};
	inline std::atomic<uint64_t> unit_identity_epoch{0};
	inline std::atomic<uint64_t> known_synthetic_filtered{0};

	struct frame_state_t
	{
		ViewMatrix_t camera_matrix{};
		vec3_t camera_pos{};
		vec3_t local_pos{};
		uintptr_t local_unit_base = 0; // ya resuelto en el snapshot; no releer por render
		vec3_t bomb_impact{};
		GuiState gui_state = GuiState::NONE;
		uint16_t ccrp_projectile_code = 0;
		bool local_is_plane = false;
		bool local_bomb_selected = false;
		bool valid = false;
	};

	using frame_snapshot_t=reusable_snapshot::snapshot<frame_state_t,vec3_t>;

	inline constexpr size_t frame_snapshot_slots_count = 3;
	inline std::array< std::atomic< std::shared_ptr< const frame_snapshot_t > >, frame_snapshot_slots_count > frame_snapshot_slots{};
	inline std::atomic<uint8_t> frame_snapshot_front_slot{ 0 };
	inline std::atomic<uint64_t> frame_snapshot_publish_seq{ 0 };
	inline std::atomic<uint64_t> frame_sequence{ 0 };

	inline std::atomic<uint64_t> prof_update_entity_us{ 0 };
	inline std::atomic<uint64_t> prof_update_fast_us{ 0 };
	inline std::atomic<uint64_t> prof_update_frame_us{ 0 };
	inline std::atomic<uint64_t> prof_esp_us{ 0 };
	inline std::atomic<uint64_t> prof_esp_radar_us{0},prof_esp_units_us{0};
	inline std::atomic<uint64_t> prof_esp_xray_us{0},prof_esp_reload_us{0},prof_esp_invul_us{0};
	inline std::atomic<uint64_t> prof_projectile_us{ 0 };
	inline std::atomic<uint64_t> prof_aimbot_us{ 0 };
	inline std::atomic<uint64_t> prof_main_frame_us{ 0 };
	inline std::atomic<uint64_t> prof_snapshot_age_us{ 0 };
	inline std::atomic<uint64_t> prof_esp_w2s_calls{ 0 };
	inline std::atomic<uint64_t> prof_esp_obb_calls{ 0 };
	inline std::atomic<uint64_t> prof_esp_cull_distance{ 0 };
	inline std::atomic<uint64_t> prof_esp_cull_type{ 0 };
	inline std::atomic<uint64_t> prof_esp_cull_frustum{ 0 };
	inline std::atomic<uint64_t> prof_esp_cull_projection{ 0 };
	inline std::atomic<uint64_t> prof_esp_partial_boxes{ 0 };
	inline std::atomic<uint64_t> prof_update_entity_count{ 0 };
	inline std::atomic<uint64_t> prof_update_fast_count{ 0 };
	inline std::atomic<uint64_t> prof_update_frame_count{ 0 };
	inline std::atomic<uint64_t> prof_esp_count{ 0 };
	inline std::atomic<uint64_t> prof_aimbot_calls{ 0 };
	inline std::atomic<uint64_t> prof_main_frame_count{ 0 };
	inline std::atomic<uint64_t> prof_frame_p95_us{ 0 }, prof_frame_peak_us{ 0 };
	inline std::atomic<uint64_t> prof_snapshot_age_p95_us{ 0 }, prof_snapshot_age_peak_us{ 0 };
	inline std::atomic<uint64_t> prof_update_frame_p95_us{ 0 };
	inline std::atomic<uint64_t> prof_projectile_scan_us{ 0 };
	inline std::atomic<int> prof_projectile_adaptive_budget{ 0 };
	inline std::atomic<uint64_t> prof_reload_candidates{0}, prof_reload_drawn{0};
	inline std::atomic<uint64_t> prof_reload_failed{0}, prof_reload_unexpected{0};
	inline std::atomic<int> prof_reload_sample{-1};
	inline std::atomic<uint64_t> prof_invul_candidates{0}, prof_invul_shown{0}, prof_invul_failed{0}, prof_invul_invalid{0}, prof_invul_timer_shown{0};
	inline std::atomic<uint64_t> prof_projectile_reads{ 0 };
	inline std::atomic<bool> prof_snapshot_valid{ false };

	// Called only by the render thread. Bounded rolling samples avoid a lifetime
	// maximum that would continue reporting an old stall after recovery.
	struct rolling_samples_t
	{
		std::array<uint32_t, 120> values{};
		size_t count = 0, next = 0;
		void add(uint64_t sample, std::atomic<uint64_t>& p95, std::atomic<uint64_t>* peak = nullptr)
		{
			values[next] = static_cast<uint32_t>((std::min)(sample, static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)())));
			next = (next + 1) % values.size();
			count = (std::min)(count + 1, values.size());
			if (count < 16 || next % 30 != 0) return;
			auto sorted = values;
			std::sort(sorted.begin(), sorted.begin() + count);
			p95.store(sorted[(count * 95 - 1) / 100], std::memory_order_relaxed);
			if (peak) peak->store(sorted[count - 1], std::memory_order_relaxed);
		}
	};
	inline void record_performance_frame(uint64_t frame_us, uint64_t age_us, uint64_t snapshot_id)
	{
		static rolling_samples_t frames, ages, updates;
		static uint64_t last_snapshot = 0;
		frames.add(frame_us, prof_frame_p95_us, &prof_frame_peak_us);
		if (snapshot_id) ages.add(age_us, prof_snapshot_age_p95_us, &prof_snapshot_age_peak_us);
		if (snapshot_id && snapshot_id != last_snapshot) {
			updates.add(prof_update_frame_us.load(std::memory_order_relaxed), prof_update_frame_p95_us);
			last_snapshot = snapshot_id;
		}
	}

	struct scoped_profile_t
	{
		std::atomic<uint64_t>& output_us;
		std::atomic<uint64_t>* counter;
		std::chrono::steady_clock::time_point start;

		scoped_profile_t(std::atomic<uint64_t>& output, std::atomic<uint64_t>* count = nullptr)
			: output_us(output), counter(count), start(std::chrono::steady_clock::now())
		{
		}

		~scoped_profile_t()
		{
			const auto end = std::chrono::steady_clock::now();
			const auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
			output_us.store(static_cast<uint64_t>(us), std::memory_order_relaxed);
			if (counter)
				counter->fetch_add(1, std::memory_order_relaxed);
		}
	};

	inline auto snapshot_units() -> std::vector<c_unit>
	{
		std::shared_lock lock(units_mutex);
		return unitsList;
	}

	inline auto snapshot_units_into(std::vector<c_unit>& out) -> void
	{
		std::shared_lock lock(units_mutex);
		out = unitsList;
	}

	inline auto snapshot_units_into(std::vector<c_unit>& out,uint64_t& seen) -> void
	{
		if(seen==units_revision.load(std::memory_order_acquire))return;
		std::shared_lock lock(units_mutex);
		render_work_policy::copy_if_new(unitsList,units_revision.load(std::memory_order_relaxed),out,seen);
	}

	inline auto units_count() -> size_t
	{
		std::shared_lock lock(units_mutex);
		return unitsList.size();
	}

	inline auto snapshot_frame() -> std::shared_ptr< const frame_snapshot_t >
	{
		const uint8_t slot = frame_snapshot_front_slot.load(std::memory_order_acquire);
		return frame_snapshot_slots[slot].load(std::memory_order_acquire);
	}

	inline auto reset_runtime_cache() -> void
	{
		{
			std::unique_lock units_lock(units_mutex);
			unitsList.clear();
			units_revision.fetch_add(1,std::memory_order_release);
		}
		for (auto& slot : frame_snapshot_slots)
			slot.store(std::shared_ptr< const frame_snapshot_t >{}, std::memory_order_release);
		frame_snapshot_front_slot.store(0, std::memory_order_release);
		frame_snapshot_publish_seq.store(0, std::memory_order_release);
		unit_identity_epoch.fetch_add(1,std::memory_order_release);
		known_synthetic_filtered.store(0,std::memory_order_relaxed);
	}

	// -----------------------------------------------------------------------
	// publish_frame_snapshot
	// -----------------------------------------------------------------------
	inline auto publish_frame_snapshot(std::shared_ptr<const frame_snapshot_t> snapshot) -> void
	{
		const uint64_t seq = frame_snapshot_publish_seq.fetch_add(1, std::memory_order_acq_rel) + 1;
		const uint8_t back_slot = static_cast<uint8_t>(seq % frame_snapshot_slots_count);

		frame_snapshot_slots[back_slot].store(std::move(snapshot), std::memory_order_release);
		frame_snapshot_front_slot.store(back_slot, std::memory_order_release);
	}

	// -----------------------------------------------------------------------
	// UpdateFrameSnapshot
	// -----------------------------------------------------------------------
	inline auto UpdateFrameSnapshot(bool include_unit_positions=true) -> void
	{
		scoped_profile_t prof(prof_update_frame_us, &prof_update_frame_count);
		static uintptr_t cached_local_base = 0;
		static bool cached_is_plane = false;
		static auto next_type_read = std::chrono::steady_clock::time_point{};
		static thread_local reusable_snapshot::pool<frame_snapshot_t,frame_snapshot_slots_count+1> snapshot_buffers;
		auto next_buffer=snapshot_buffers.acquire();
		next_buffer->reset();
		auto& next=*next_buffer;
		next.captured_at = std::chrono::steady_clock::now();
		next.frame_id = frame_sequence.fetch_add(1, std::memory_order_relaxed) + 1;

		const auto gui_state = sdk::cLocalPlayer->getGuiState();
		next.frame.gui_state = static_cast<GuiState>(gui_state);
		if (gui_state != GuiState::ALIVE && gui_state != GuiState::SPEC)
		{
			reset_runtime_cache();
			cached_local_base=0;cached_is_plane=false;next_type_read={};
			publish_frame_snapshot(std::move(next_buffer));
			return;
		}

		c_unit local = sdk::cLocalPlayer->getLocalUnit();
		const uintptr_t local_base = local.get_base();
		if (startup_pointer_probe::plausible_pointer(local_base))
			next.frame.local_unit_base=local_base;
		if (local_base && sdk::esp_enabled)
		{
			// Vehicle type is stable between unit changes; keep position/camera fresh
			// every render while avoiding repeated metadata pointer-chain reads.
			const auto now=std::chrono::steady_clock::now();
			if (local_base!=cached_local_base || now>=next_type_read) {
				cached_local_base=local_base;
				cached_is_plane=local.getInfo().isPlane();
				next_type_read=now+std::chrono::milliseconds(250);
			}
			next.frame.local_is_plane=cached_is_plane;
			if (cached_is_plane && sdk::esp_show_bomb_impact) {
				next.frame.bomb_impact=sdk::cGame->ballistics->getBombImpactPoint();
				if(next.frame.bomb_impact.empty())
					next.frame.bomb_impact=sdk::cGame->ballistics->getImpactPoint();
			}
		}
		if(!local_base) {cached_local_base=0;cached_is_plane=false;next_type_read={};}

		static thread_local std::vector<c_unit> units;
		static thread_local uint64_t seen_units_revision=UINT64_MAX;
		if(include_unit_positions) snapshot_units_into(units,seen_units_revision);
		else {
			units.clear();
			seen_units_revision=UINT64_MAX; // re-enable must recopy even without a new publication
		}
		next.positions.reserve(units.size());

		// Cámara y posiciones comparten la misma ejecución scatter por frame.
		c_camera::frame_batch_t camera_batch{};
		vec3_t local_position{};
		DWORD local_position_bytes=0;
		VMMDLL_SCATTER_HANDLE hScatter=TargetProcess->CreateScatterHandle();
		sdk::cGame->camera->prepareFrame(hScatter,camera_batch);
		const bool local_prepared=next.frame.local_unit_base && hScatter &&
			TargetProcess->AddScatterReadRequest(hScatter,local_base+offsets::unit_offsets::position_offset,
				&local_position,sizeof(local_position),&local_position_bytes);
		bool scatter_executed=false;
		if (hScatter)
		{
			static thread_local std::vector< uintptr_t > bases;
			bases.clear();
			bases.reserve(units.size());

			for (c_unit& unit : units)
			{
				const uintptr_t base = unit.get_base();
				if (!base)
					continue;

				bases.emplace_back(base);
			}

			static thread_local std::vector< vec3_t > positions;
			positions.assign(bases.size(),vec3_t{});
			for (size_t i = 0; i < bases.size(); ++i)
			{
				TargetProcess->AddScatterReadRequest(
					hScatter,
					bases[i] + offsets::unit_offsets::position_offset,
					&positions[i],
					sizeof(vec3_t)
				);
			}

			// También ejecutar si no hay unidades: la cámara sigue vigente.
			scatter_executed=TargetProcess->ExecuteReadScatter(hScatter,0,false);
			if (scatter_executed)
			{
				for (size_t i = 0; i < bases.size(); ++i)
				{
					if (positions[i].empty())
						continue;

					next.positions[bases[i]] = positions[i];
				}
			}
			else
			{
				for (c_unit& unit : units)
				{
					const uintptr_t base = unit.get_base();
					if (!base)
						continue;

					vec3_t position = unit.getPosition();
					if (position.empty())
						continue;

					next.positions[base] = position;
				}
			}

			TargetProcess->CloseScatterHandle(hScatter);
		}
		else
		{
			for (c_unit& unit : units)
			{
				const uintptr_t base = unit.get_base();
				if (!base)
					continue;

				vec3_t position = unit.getPosition();
				if (position.empty())
					continue;

				next.positions[base] = position;
			}
		}
		const auto camera_frame=sdk::cGame->camera->finishFrame(camera_batch,scatter_executed);
		next.frame.camera_matrix=camera_frame.matrix;
		next.frame.camera_pos=camera_frame.position;
		if (next.frame.local_unit_base)
			next.frame.local_pos=(scatter_executed && local_prepared &&
				local_position_bytes==sizeof(local_position)) ? local_position : local.getPosition();

		next.frame.valid = true;
		publish_frame_snapshot(std::move(next_buffer));
	}

	// -----------------------------------------------------------------------
	// has_renderable_unit_metadata (nombre de vehiculo NO prueba jugador humano)
	// -----------------------------------------------------------------------
	inline bool has_renderable_unit_metadata(c_unit& unit,bool& synthetic)
	{
		using clock = std::chrono::steady_clock;
		struct nickname_cache_entry_t
		{
			bool has_name = false;
			bool synthetic = false;
			clock::time_point next_refresh{};
			clock::time_point last_seen{};
		};

		static std::unordered_map<uintptr_t, nickname_cache_entry_t> cache;
		static uint64_t cache_epoch=0;
		static auto next_gc = clock::now() + std::chrono::seconds(5);
		synthetic=false;
		const auto epoch=unit_identity_epoch.load(std::memory_order_acquire);
		if (epoch!=cache_epoch) {cache.clear();cache_epoch=epoch;}

		const uintptr_t unit_base = unit.get_base();
		if (!unit_base)
			return false;

		const auto now = clock::now();
		auto& entry = cache[unit_base];
		if (now >= entry.next_refresh)
		{
			auto info=unit.getInfo();
			const auto name=info.getVehicleName();
			const auto type=info.getUnitType();
			entry.synthetic=esp::unit_type_filter::noncombatant(type);
			entry.has_name=!name.empty() && name.length()<64 &&
				!type.empty() && !entry.synthetic;
			entry.next_refresh = now + std::chrono::milliseconds(1500);
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

		synthetic=entry.synthetic;
		return entry.has_name;
	}

	// -----------------------------------------------------------------------
	// UpdateEntityList
	// -----------------------------------------------------------------------
	inline auto UpdateEntityList() -> void
	{
		scoped_profile_t prof(prof_update_entity_us, &prof_update_entity_count);
		std::vector< c_unit > temp_units;

		const auto gui_state = sdk::cLocalPlayer->getGuiState();
		if (gui_state != GuiState::ALIVE && gui_state != GuiState::SPEC) {
			sdk::cLocalPlayer->init();
			reset_runtime_cache();
			return;
		}

		c_unit local_unit=sdk::cLocalPlayer->getLocalUnit();
		const bool local_unit_ok=startup_pointer_probe::plausible_pointer(local_unit.get_base());
		if (!local_unit_ok && gui_state==GuiState::ALIVE) {
			std::unique_lock lock(units_mutex);
			unitsList.clear();
			units_revision.fetch_add(1,std::memory_order_release);
			return; // batalla en transicion: no lanzar lecturas a punteros invalidos
		}

		const int unit_count = sdk::cGame->getUnitCount();
		const uintptr_t unit_list_base = sdk::cGame->getUnitList();
		if (!scatter_read_recovery::valid_unit_list(unit_list_base,unit_count))
			return; // cantidad o direccion incompatible: no lanzar scatter masivo

		temp_units.reserve(static_cast<size_t>(unit_count));
		VMMDLL_SCATTER_HANDLE hScatter = TargetProcess->CreateScatterHandle();
		std::vector<std::uintptr_t> pointers(static_cast<size_t>(unit_count));
		// Los punteros viven en una matriz contigua: una preparación scatter
		// sustituye hasta 4096 preparaciones de 8 bytes sin cambiar offsets.
		const auto pointer_bytes=scatter_read_recovery::unit_list_bytes(unit_list_base,unit_count);
		DWORD pointer_bytes_read=0;
		const bool prepared=hScatter && pointer_bytes &&
			TargetProcess->AddScatterReadRequest(hScatter,unit_list_base,
				pointers.data(),pointer_bytes,&pointer_bytes_read);
		const bool scatter_ok=prepared && TargetProcess->ExecuteReadScatter(hScatter) &&
			pointer_bytes_read==pointer_bytes; // parcial: usar recuperación existente
		if (hScatter) TargetProcess->CloseScatterHandle(hScatter);
		if (!scatter_ok) {
			scatter_read_recovery::units_scatter_fail.fetch_add(1,std::memory_order_relaxed);
			pointers=scatter_read_recovery::read_unit_list_individually(
				unit_list_base,unit_count,[](std::uintptr_t address,void* dest,size_t bytes) {
					return TargetProcess->ReadOptional(address,dest,bytes);
				});
		}
		if (!scatter_ok) {
			auto& counter=pointers.empty()?scatter_read_recovery::units_unavailable:
				scatter_read_recovery::units_fallback_ok;
			counter.fetch_add(1,std::memory_order_relaxed);
		}
		std::vector<c_unit> units;
		units.reserve(pointers.size());
		for (const uintptr_t pointer:pointers)
			if (startup_pointer_probe::plausible_pointer(pointer))
				units.emplace_back(pointer);
		const uint8_t local_team = local_unit_ok ? local_unit.getTeam() : 0;
		std::vector< uint16_t > unit_states(units.size());
		std::vector< uint8_t > unit_teams(units.size());
		bool has_basic_scatter_data = false;

		VMMDLL_SCATTER_HANDLE hBasicScatter = TargetProcess->CreateScatterHandle();
		if (hBasicScatter)
		{
			for (size_t i = 0; i < units.size(); ++i)
			{
				const uintptr_t base = units[i].get_base();
				if (!base)
					continue;

				TargetProcess->AddScatterReadRequest(
					hBasicScatter,
					base + offsets::unit_offsets::unitState_offset,
					&unit_states[i],
					sizeof(uint16_t)
				);

				TargetProcess->AddScatterReadRequest(
					hBasicScatter,
					base + offsets::unit_offsets::unitArmyNo_offset,
					&unit_teams[i],
					sizeof(uint8_t)
				);
			}

			has_basic_scatter_data = TargetProcess->ExecuteReadScatter(hBasicScatter, 0, false);
			TargetProcess->CloseScatterHandle(hBasicScatter);
		}

		uint64_t synthetic_count=0;
		for (size_t i = 0; i < units.size(); ++i)
		{
			c_unit& unit = units[i];
			const uint16_t unit_state = has_basic_scatter_data ? unit_states[i] : unit.getUnitState();
			if (unit_state >= sdk::death_state_threshold)
				continue;

			const uint8_t unit_team = has_basic_scatter_data ? unit_teams[i] : unit.getTeam();
			if (unit_team == 0 || unit_team == local_team)
				continue;

			bool synthetic=false;
			if (!has_renderable_unit_metadata(unit,synthetic)) {
				if (synthetic) ++synthetic_count;
				continue;
			}

			temp_units.emplace_back(unit);
		}

		known_synthetic_filtered.store(synthetic_count,std::memory_order_relaxed);
		{
			std::unique_lock lock(units_mutex);
			unitsList = std::move(temp_units);
			units_revision.fetch_add(1,std::memory_order_release);
		}
	}
}