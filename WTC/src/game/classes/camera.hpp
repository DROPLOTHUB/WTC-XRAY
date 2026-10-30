#pragma once
#include "../../core/scatter_read_recovery.hpp"
#include "../../core/camera_batch_policy.hpp"

// Guard RAII para scatter handle de VMMDLL. Cierra el handle en cualquier ruta
// de salida (return temprano, excepción, etc.). Evita fugas de handles (~60/s)
// que degradan la FPGA.
struct scatter_handle_guard_t
{
	VMMDLL_SCATTER_HANDLE handle = nullptr;

	explicit scatter_handle_guard_t(VMMDLL_SCATTER_HANDLE h) noexcept : handle(h) {}
	~scatter_handle_guard_t() noexcept
	{
		if (handle)
			TargetProcess->CloseScatterHandle(handle);
	}

	scatter_handle_guard_t(const scatter_handle_guard_t&) = delete;
	scatter_handle_guard_t& operator=(const scatter_handle_guard_t&) = delete;

	scatter_handle_guard_t(scatter_handle_guard_t&& other) noexcept
		: handle(other.handle) {
		other.handle = nullptr;
	}

	scatter_handle_guard_t& operator=(scatter_handle_guard_t&& other) noexcept
	{
		if (this != &other)
		{
			if (handle) TargetProcess->CloseScatterHandle(handle);
			handle = other.handle;
			other.handle = nullptr;
		}
		return *this;
	}

	explicit operator bool() const noexcept { return handle != nullptr; }
};

class c_camera {
public:
	auto init(uintptr_t cGame) -> bool
	{
		uintptr_t addr = cGame + offsets::cgame_offsets::camera_offset;
		this->base_address = TargetProcess->Read<uintptr_t>(addr);
		return (this->base_address != 0);
	}
	auto init_from_validated_base(uintptr_t address) -> bool {
		this->base_address=address;return address!=0;
	}

public:

	auto getPosition() -> vec3_t
	{
		return TargetProcess->Read<vec3_t>(
			this->base_address
			+ offsets::cgame_offsets::camera_offsets::camera_position_offset
		);
	}

    struct frame_read_t {
        ViewMatrix_t matrix{};
        vec3_t position{};
    };
    struct frame_batch_t {
        frame_read_t data{};
        DWORD matrix_bytes=0,position_bytes=0;
        bool matrix_prepared=false,position_prepared=false;
    };

    // Usable con un handle compartido con las posiciones de unidades.
    // frame_batch_t y sus buffers sobreviven hasta cerrar el scatter handle.
    void prepareFrame(VMMDLL_SCATTER_HANDLE handle,frame_batch_t& batch) {
        if(!handle) return;
        const uintptr_t matrix_address=base_address+
            offsets::cgame_offsets::camera_offsets::camera_matrix_offset;
        const uintptr_t position_address=base_address+
            offsets::cgame_offsets::camera_offsets::camera_position_offset;
        batch.matrix_prepared=TargetProcess->AddScatterReadRequest(
            handle,matrix_address,&batch.data.matrix,sizeof(batch.data.matrix),&batch.matrix_bytes);
        batch.position_prepared=TargetProcess->AddScatterReadRequest(
            handle,position_address,&batch.data.position,sizeof(batch.data.position),&batch.position_bytes);
    }
    auto finishFrame(frame_batch_t& batch,bool executed) -> frame_read_t {
        const uintptr_t matrix_address=base_address+
            offsets::cgame_offsets::camera_offsets::camera_matrix_offset;
        if (!camera_batch_policy::complete(executed,batch.matrix_prepared,
            batch.matrix_bytes,sizeof(batch.data.matrix))) {
            batch.data.matrix=ViewMatrix_t{};
            if (TargetProcess->ReadOptional(matrix_address,&batch.data.matrix,sizeof(batch.data.matrix)))
                scatter_read_recovery::camera_fallback_ok.fetch_add(1,std::memory_order_relaxed);
            else {
                batch.data.matrix=ViewMatrix_t{};
                scatter_read_recovery::camera_unavailable.fetch_add(1,std::memory_order_relaxed);
            }
        }
        if (!camera_batch_policy::complete(executed,batch.position_prepared,
            batch.position_bytes,sizeof(batch.data.position)))
            batch.data.position=getPosition();
        return batch.data;
    }
    auto getFrame() -> frame_read_t {
        frame_batch_t batch{};
        scatter_handle_guard_t guard(TargetProcess->CreateScatterHandle());
        prepareFrame(guard.handle,batch);
        const bool executed=guard && TargetProcess->ExecuteReadScatter(guard.handle,0,false);
        return finishFrame(batch,executed);
    }

	auto getCameraMatrix() -> ViewMatrix_t
	{
		ViewMatrix_t matrix{};

		const uintptr_t address=this->base_address +
			offsets::cgame_offsets::camera_offsets::camera_matrix_offset;
		scatter_handle_guard_t guard(TargetProcess->CreateScatterHandle());
		if (guard && TargetProcess->AddScatterReadRequest(
			guard.handle,address,&matrix,sizeof(matrix)) &&
			TargetProcess->ExecuteReadScatter(guard.handle,0,true))
			return matrix;

		// La matriz en cero apaga toda la proyeccion: probar lectura directa
		// de la MISMA direccion, sin inventar offsets ni reutilizar frames viejos.
		matrix=ViewMatrix_t{};
		if (TargetProcess->ReadOptional(address,&matrix,sizeof(matrix))) {
			scatter_read_recovery::camera_fallback_ok.fetch_add(1,std::memory_order_relaxed);
			return matrix;
		}
		scatter_read_recovery::camera_unavailable.fetch_add(1,std::memory_order_relaxed);
		return ViewMatrix_t{};
		// guard.~scatter_handle_guard_t() cierra el handle automáticamente.
	}

private:
	uintptr_t base_address;
};