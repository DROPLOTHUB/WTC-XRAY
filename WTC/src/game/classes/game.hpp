#pragma once
#include "..\\..\\core\\runtime_context.hpp"

class c_game {
public:
	auto init() -> bool {
		this->base_address = TargetProcess->Read< uintptr_t >(runtime::base_address() + offsets::cgame_offset);
		return this->base_address != 0;
	}
	auto init_from_validated_base(uintptr_t address) -> bool {
		this->base_address=address;return address!=0;
	}

	auto get_base() -> uintptr_t {
		return this->base_address;
	}

public:

	auto getUnitList() -> uintptr_t {
		return TargetProcess->Read< uintptr_t >(this->base_address + offsets::cgame_offsets::unit_list3);
	}

	auto getUnitCount() -> int {
		return TargetProcess->Read< int >(this->base_address + offsets::cgame_offsets::unit_count3);
	}

	c_ballistic* ballistics = new c_ballistic;
	c_camera* camera = new c_camera;

private:
	uintptr_t base_address;
};