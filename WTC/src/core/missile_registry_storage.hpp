#pragma once
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include "../features/esp/projectile_visibility.hpp"

// Registro de NOMBRES, no de preferencias: no modifica wtc_menu.cfg ni su .bak.
// Un nombre legible por linea; lineas incompletas o invalidas se ignoran.
namespace missile_registry_storage {
inline std::string status = "Registro pendiente de inicializacion";
inline std::chrono::steady_clock::time_point retry_after{};

inline std::filesystem::path file_path() {
    wchar_t executable[32768]{};
    const DWORD length=GetModuleFileNameW(nullptr,executable,32768);
    if (!length || length>=32768) return {};
    return std::filesystem::path(executable).parent_path()/L"missiles"/L"registry.txt";
}
inline bool load_at(const std::filesystem::path& path) {
    if (path.empty()) { status="Registro: ruta del EXE no disponible"; return false; }
    std::error_code error;
    const bool exists=std::filesystem::exists(path,error);
    if (error) { status="Registro: no se pudo consultar el archivo"; return false; }
    if (!exists) { status="Registro: nuevo, se creara al detectar misiles"; return true; }
    const auto bytes=std::filesystem::file_size(path,error);
    if (error || bytes > 16u*1024u*1024u) {
        status="Registro: archivo demasiado grande o ilegible"; return false;
    }
    std::ifstream input(path,std::ios::binary);
    if (!input) { status="Registro: no se pudo abrir para lectura"; return false; }
    std::set<std::string> names;
    std::string line;
    while (std::getline(input,line)) {
        if (input.eof()) break; // ultima linea sin salto: escritura interrumpida
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (names.size()<projectile_visibility::max_names &&
            projectile_visibility::valid_name(line) &&
            !projectile_ecs::name_has_flare(line.c_str()) &&
            !projectile_ecs::name_has_mm(line.c_str())) names.emplace(line);
    }
    if (input.bad()) { status="Registro: fallo de lectura"; return false; }
    projectile_visibility::restore_observed(std::vector<std::string>(names.begin(),names.end()),false);
    status="Registro: nombres cargados desde carpeta del EXE";
    return true;
}
inline bool load() { return load_at(file_path()); }
inline bool flush_at(const std::filesystem::path& path) {
    const auto pending=projectile_visibility::registry_pending();
    if (pending.empty()) return true;
    if (path.empty()) { status="Registro: ruta del EXE no disponible"; return false; }
    const auto now=std::chrono::steady_clock::now();
    if (now<retry_after) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(),error);
    if (error) {
        status="Registro: no se puede crear la carpeta junto al EXE";
        retry_after=now+std::chrono::seconds(5);
        return false;
    }
    // Si un cierre interrumpio la ultima linea, invalidarla antes de anadir
    // nombres: el cargador nunca debe aceptar un identificador cortado.
    bool torn_line=false;
    if (std::filesystem::exists(path,error) && !error) {
        std::ifstream tail(path,std::ios::binary|std::ios::ate);
        if (!tail) torn_line=true;
        else if (tail.tellg()>0) {
            tail.seekg(-1,std::ios::end);
            char last=0;
            if (!tail.get(last) || last!='\n') torn_line=true;
        }
    }
    std::ofstream output(path,std::ios::binary|std::ios::app);
    if (output) {
        if (torn_line) output << '\t' << '\n';
        for (const auto& name:pending)
            output << name << '\n';
        output.flush();
    }
    if (!output.good()) {
        status="Registro: no se puede escribir junto al EXE; nombres en memoria";
        retry_after=now+std::chrono::seconds(5);
        return false;
    }
    projectile_visibility::registry_acknowledge(pending);
    status="Registro: nombres nuevos guardados automaticamente";
    return true;
}
inline bool flush() {
    return !projectile_visibility::registry_has_pending() || flush_at(file_path());
}
}
