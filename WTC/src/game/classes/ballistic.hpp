#pragma once

#include <array>
#include <cmath>
#include <cstdint>

class c_ballistic {
public:
	auto init(uintptr_t cGame) -> bool
	{
		uintptr_t addr = cGame + offsets::cgame_offsets::ballistics_offset;
		this->base_address = TargetProcess->Read<uintptr_t>(addr);
		return (this->base_address != 0);
	}
	auto init_from_validated_base(uintptr_t address) -> bool {
		this->base_address=address;return address!=0;
	}
public:

	auto getVelocity() -> float {
		return TargetProcess->Read< float >(this->base_address + offsets::cgame_offsets::ballistic_offsets::velocity);
	}

	auto getMass() -> float {
		return TargetProcess->Read< float >(this->base_address + offsets::cgame_offsets::ballistic_offsets::mass);
	}

	auto getCaliber() -> float {
		return TargetProcess->Read< float >(this->base_address + offsets::cgame_offsets::ballistic_offsets::caliber);
	}

	auto getLength() -> float {
		return TargetProcess->Read< float >(this->base_address + offsets::cgame_offsets::ballistic_offsets::length);
	}

	auto getImpactPoint() -> vec3_t {
		return readImpactPointWithFallback(offsets::cgame_offsets::ballistic_offsets::bullet_impact_point);
	}

	auto getMaxDistance() -> float {
		return TargetProcess->Read< float >(this->base_address + offsets::cgame_offsets::ballistic_offsets::max_dist);
	}

	auto getBombImpactPoint() -> vec3_t {
		return readImpactPointWithFallback(offsets::cgame_offsets::ballistic_offsets::bomb_impact_point);
	}

private:
	static auto is_valid_impact_point(const vec3_t& point) -> bool
	{
		if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
			return false;

		if (point.empty())
			return false;

		constexpr float kMaxReasonableAbsCoord = 500000.0f;
		return std::fabs(point.x) <= kMaxReasonableAbsCoord &&
			std::fabs(point.y) <= kMaxReasonableAbsCoord &&
			std::fabs(point.z) <= kMaxReasonableAbsCoord;
	}

	auto readImpactPointWithFallback(uintptr_t primary_offset) const -> vec3_t
	{
		// 2.59: bomb_prediction_offset = 0x1CCC. Se prueban dos rutas:
		//  (a) valor directo en el offset
		//  (b) valor en offset + 0x20 (struct anidado)
		//  (c) si el valor directo es un puntero válido, se lee desde ahí.
		const std::array<uintptr_t, 2> direct_candidates{
			primary_offset,
			primary_offset + 0x20
		};

		for (const uintptr_t offset : direct_candidates)
		{
			const vec3_t point = TargetProcess->Read< vec3_t >(this->base_address + offset);
			if (is_valid_impact_point(point))
				return point;
		}

		const uintptr_t impact_ptr = TargetProcess->Read<uintptr_t>(this->base_address + primary_offset);
		if (impact_ptr > 0x10000)
		{
			const std::array<uintptr_t, 2> ptr_candidates{ impact_ptr, impact_ptr + 0x20 };
			for (const uintptr_t addr : ptr_candidates)
			{
				const vec3_t point = TargetProcess->Read< vec3_t >(addr);
				if (is_valid_impact_point(point))
					return point;
			}
		}

		return {};
	}

private:
	uintptr_t base_address;
};