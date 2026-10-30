#pragma once

// Protección contra redefinición de vec3d_t (también definido en esp.hpp)
#ifndef VEC3D_T_DEFINED
#define VEC3D_T_DEFINED
struct vec3d_t {
    double x, y, z;
};
#endif

// === MOVIMIENTO TERRESTRE ===
class c_movement_ground {
public:
    c_movement_ground(const std::uintptr_t container_address = 0)
        : container_address(container_address) {
    }

    auto velocity() -> vec3_t {
        return TargetProcess->Read<vec3_t>(this->container_address + offsets::unit_offsets::groundmovement_velocity_offset);
    }

private:
    uintptr_t container_address;
};

// === MOVIMIENTO AÉREO ===
class c_movement_air {
public:
    c_movement_air(const std::uintptr_t container_address = 0)
        : container_address(container_address) {
    }

    auto velocity() -> vec3d_t {
        return TargetProcess->Read<vec3d_t>(this->container_address + offsets::unit_offsets::airmovement_offsets::velocity_offset);
    }

    auto acceleration() -> vec3d_t {
        return TargetProcess->Read<vec3d_t>(this->container_address + offsets::unit_offsets::airmovement_offsets::acceleration_offset);
    }

private:
    uintptr_t container_address;
};