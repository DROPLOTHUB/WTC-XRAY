#pragma once
#include "../game/offsets.hpp"
#include "offset_credentials.hpp"
#include "offset_api_mapping.hpp"
#include <Windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"Crypt32.lib")

namespace offset_auto_update {
inline std::mutex status_mutex;
inline std::string status="Introduce una clave en este equipo para activar actualizaciones automáticas.";
inline std::string applied_version="Offsets incorporados (sin API activa)";
inline std::atomic<bool> pending_validation=false;
inline std::atomic<bool> cache_quarantined=false; // API válida en JSON, incompatible con esta instancia del juego
inline void set_status(std::string message) {std::scoped_lock lock(status_mutex);status=std::move(message);}
inline std::string get_status() {std::scoped_lock lock(status_mutex);return status;}
inline std::string get_applied_version() {std::scoped_lock lock(status_mutex);return applied_version;}
inline void set_applied_version(std::string version) {
    std::scoped_lock lock(status_mutex);applied_version=std::move(version);
}
inline void restore_built_in_offsets() {
    offsets::restore_default_offsets();
    offsets::api_air_velocity_ready=false;
    offsets::api_air_acceleration_ready=false;
    set_applied_version("Offsets incorporados (sin API activa)");
}
inline offset_credentials::store vault{};

struct http_handle {
    HINTERNET value=nullptr;
    explicit http_handle(HINTERNET handle=nullptr):value(handle){}
    ~http_handle(){if(value)WinHttpCloseHandle(value);}
    http_handle(const http_handle&)=delete;
    http_handle& operator=(const http_handle&)=delete;
    explicit operator bool() const{return value!=nullptr;}
};
inline bool download(std::string_view secret,std::string& body,std::string& error) {
    http_handle session(WinHttpOpen(L"WTC-Private-Offsets/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session){error="WinHTTP no disponible";return false;}
    WinHttpSetTimeouts(session.value,2500,2500,3500,4500);
    http_handle connection(WinHttpConnect(session.value,L"api.monkrel.cc",INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection){error="No se pudo conectar al proveedor";return false;}
    http_handle request(WinHttpOpenRequest(connection.value,L"GET",L"/api/v1/offsets?game=warthunder",
        nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request){error="No se pudo crear la petición HTTPS";return false;}
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect))){
        error="No se pudo desactivar la redirección HTTPS";return false;
    }
    // Credential Manager memory is borrowed only for this synchronous request.
    // No URLs, log lines, files or CLI arguments ever contain the credential.
    std::wstring headers=L"Accept: application/json\r\n";
    if(!secret.empty()) {
        headers+=L"Authorization: Bearer ";
        for(unsigned char c:secret)headers.push_back(static_cast<wchar_t>(c));
        headers+=L"\r\n";
    }
    const bool sent=WinHttpSendRequest(request.value,headers.c_str(),static_cast<DWORD>(-1),
        WINHTTP_NO_REQUEST_DATA,0,0,0)!=FALSE;
    SecureZeroMemory(headers.data(),headers.size()*sizeof(wchar_t));
    headers.clear();
    if(!sent || !WinHttpReceiveResponse(request.value,nullptr)) {
        error="Fallo al recibir respuesta HTTPS";return false;
    }
    DWORD code=0,size=sizeof(code);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX)) {
        error="HTTP sin código de estado";return false;
    }
    if(code==401 || code==403){error="Credencial rechazada o plan sin acceso completo";return false;}
    if(code!=200){error="Proveedor respondió con HTTP "+std::to_string(code);return false;}
    body.clear();
    char chunk[8192];
    for(;;) {
        DWORD bytes=0;
        if(!WinHttpReadData(request.value,chunk,sizeof(chunk),&bytes)) {
            error="Respuesta HTTPS incompleta";return false;
        }
        if(bytes==0)break;
        if(body.size()+bytes>512*1024){error="Respuesta de API demasiado grande";return false;}
        body.append(chunk,bytes);
    }
    return true;
}
inline std::filesystem::path cache_path() {
    wchar_t executable[MAX_PATH]{};
    const DWORD length=GetModuleFileNameW(nullptr,executable,MAX_PATH);
    if(!length || length>=MAX_PATH)return {};
    return std::filesystem::path(executable).parent_path()/L"api-offsets.cache";
}
inline bool expected_cache_path(const std::filesystem::path& path) {
    return path.is_absolute() && path.filename()==L"api-offsets.cache";
}
inline std::unordered_map<std::string,std::uintptr_t> staged_defaults() {
    std::unordered_map<std::string,std::uintptr_t> values;
    for(const auto& entry:offsets::runtime_offsets_registry())values.emplace(entry.key,*entry.value);
    return values;
}
inline bool older(std::string_view proposed,std::string_view previous) {
    if(previous.empty())return false;
    std::vector<unsigned> a,b;
    const auto decode=[](std::string_view version,std::vector<unsigned>& out){
        while(!version.empty()) {
            auto dot=version.find('.');auto part=version.substr(0,dot);
            unsigned number=0;auto [end,ec]=std::from_chars(part.data(),part.data()+part.size(),number);
            if(ec!=std::errc{}||end!=part.data()+part.size())return false;
            out.push_back(number);
            if(dot==std::string_view::npos)break;
            version.remove_prefix(dot+1);
        }
        return out.size()>=2 && out.size()<=6;
    };
    return decode(proposed,a)&&decode(previous,b)&&std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end());
}
inline std::string canonical_manifest(const offset_api_payload::manifest& manifest) {
    // Only validated API fields are serialized. Neither keys nor HTTP headers
    // are ever stored in the cache.
    std::vector<std::string> keys;
    for(const auto& [key,value]:manifest.entries)keys.push_back(key);
    std::sort(keys.begin(),keys.end());
    std::ostringstream json;
    json<<"{\"game\":\"warthunder\",\"channel\":\"live\",\"version\":\""
        <<manifest.version<<"\",\"tier\":\"api\",\"count\":"<<keys.size()<<",\"offsets\":[";
    bool first=true;
    for(const auto& key:keys) {
        const auto dot=key.find('.');
        if(dot==std::string::npos)continue;
        if(!first)json<<',';
        first=false;
        json<<"{\"group\":\""<<key.substr(0,dot)<<"\",\"name\":\""
            <<key.substr(dot+1)<<"\",\"value\":\"0x"
            <<std::hex<<manifest.entries.at(key)<<std::dec<<"\"}";
    }
    json<<"]}";
    return json.str();
}
inline bool read_cache(const std::filesystem::path& path,
    offset_api_payload::manifest& result,std::string& error) {
    // An old offsets.txt is NEVER read. This cache is Windows-user protected,
    // and its complete API manifest is revalidated before any offset changes.
    if(!path.is_absolute() || (path.filename()!=L"api-offsets.cache" &&
       path.filename()!=L"api-offsets.cache.bak")) {
        error="Ruta de caché API no confiable";return false;
    }
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in){error="No hay caché API protegida";return false;}
    const auto size=in.tellg();
    if(size<16 || size>1024*1024){error="Tamaño de caché API inválido";return false;}
    std::vector<BYTE> cipher(static_cast<std::size_t>(size));
    in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(cipher.data()),static_cast<std::streamsize>(cipher.size()))) {
        error="Caché API incompleta";return false;
    }
    DATA_BLOB encrypted{static_cast<DWORD>(cipher.size()),cipher.data()},decrypted{};
    if(!CryptUnprotectData(&encrypted,nullptr,nullptr,nullptr,nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN,&decrypted)) {
        error="Caché API no pertenece al usuario actual o está dañada";return false;
    }
    std::string body(reinterpret_cast<const char*>(decrypted.pbData),decrypted.cbData);
    SecureZeroMemory(decrypted.pbData,decrypted.cbData);
    LocalFree(decrypted.pbData);
    offset_api_payload::manifest next;
    if(!offset_api_payload::parse(body,next,error))return false;
    std::unordered_map<std::string,std::uintptr_t> checked;
    std::size_t mapped=0;
    if(!offset_api_mapping::apply(next,staged_defaults(),checked,error,mapped))return false;
    result=std::move(next);
    return true;
}
inline std::string previous_version(const std::filesystem::path& path) {
    offset_api_payload::manifest saved;
    std::string error;
    return read_cache(path,saved,error)?saved.version:std::string{};
}
inline bool apply_manifest(const offset_api_payload::manifest& manifest) {
    std::unordered_map<std::string,std::uintptr_t> values;
    std::string error;
    std::size_t mapped=0;
    if(!offset_api_mapping::apply(manifest,staged_defaults(),values,error,mapped))return false;
    // Startup/reconnect only, never mutate while DMA workers read offsets.
    for(const auto& entry:offsets::runtime_offsets_registry())*entry.value=values.at(entry.key);
    offsets::api_ground_velocity_ready=offset_api_mapping::ground_velocity_available(manifest);
    offsets::api_ballistics_ready=offset_api_mapping::round_ballistics_available(manifest);
    offsets::api_reload_ready=offset_api_mapping::reload_available(manifest);
    offsets::api_invulnerable_ready=offset_api_mapping::invulnerable_available(manifest);
    offsets::api_invul_timer_ready=offset_api_mapping::invul_timer_available(manifest);
    offsets::api_air_velocity_ready=offset_api_mapping::air_velocity_available(manifest);
    offsets::api_air_acceleration_ready=offset_api_mapping::air_acceleration_available(manifest);
    set_applied_version("API "+manifest.version);
    return true;
}
inline bool install_at(const std::filesystem::path& path,
    const offset_api_payload::manifest& remote,std::size_t& changed,std::string& error) {
    if(!expected_cache_path(path)){error="Ruta de caché API no confiable";return false;}
    const auto previous=previous_version(path);
    if(older(remote.version,previous)){
        error="La API anuncia una versión anterior a la caché válida";return false;
    }
    std::unordered_map<std::string,std::uintptr_t> next;
    if(!offset_api_mapping::apply(remote,staged_defaults(),next,error,changed))return false;
    if(next==staged_defaults() && previous==remote.version)return true;
    const std::string body=canonical_manifest(remote);
    DATA_BLOB plain{static_cast<DWORD>(body.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(body.data()))},encrypted{};
    if(body.size()>512*1024 || !CryptProtectData(&plain,L"WTC private API offsets",
                              nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&encrypted)) {
        error="No se pudo proteger la caché API con Windows";return false;
    }
    auto temporary=path;temporary+=L".new";
    auto backup=path;backup+=L".bak";
    bool written=false;
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(out) {
            out.write(reinterpret_cast<const char*>(encrypted.pbData),encrypted.cbData);
            out.flush();written=static_cast<bool>(out);
        }
    }
    SecureZeroMemory(encrypted.pbData,encrypted.cbData);
    LocalFree(encrypted.pbData);
    if(!written){error="No se pudo guardar la caché API provisional";return false;}
    std::error_code ec;
    if(!previous.empty() && std::filesystem::exists(path,ec) &&
       !CopyFileW(path.c_str(),backup.c_str(),FALSE)) {
        error="No se pudo respaldar la caché API";return false;
    }
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error="No se pudo activar la caché API";return false;
    }
    pending_validation.store(true,std::memory_order_relaxed);
    return true;
}
inline bool install(const offset_api_payload::manifest& remote,std::size_t& changed,std::string& error) {
    return install_at(cache_path(),remote,changed,error);
}
inline bool refresh(bool manual=false) {
    if(!vault.exists()) {
        set_status("Sin clave local: guárdala en Ajustes > Offsets avanzados.");
        return false;
    }
    bool success=false;
    std::string message;
    vault.with_secret([&](std::string_view secret){
        std::string body,error;
        if(!download(secret,body,error)){message=error;return;}
        offset_api_payload::manifest remote;
        if(!offset_api_payload::parse(body,remote,error)){message=error;return;}
        std::size_t mapped=0;
        if(!install(remote,mapped,error)){message=error;return;}
        if(!manual && !apply_manifest(remote)) {
            message="Se guardó la caché API, pero no se pudo activar";return;
        }
        message="API versión "+remote.version+": "+std::to_string(mapped)+
            " campos validados. "+(manual?"Pulsa Recargar SDK para aplicarlos.":"Activos al conectar.");
        success=true;
        if(manual)cache_quarantined.store(false,std::memory_order_relaxed);
    });
    if(!success) {
        if(message.empty())message="No se pudo leer la credencial local";
        message+="; se conservan los últimos offsets validados o los incorporados.";
    }
    set_status(std::move(message));
    return success;
}
inline void startup() {
    // Capture built-in defaults BEFORE applying any cached offsets.
    restore_built_in_offsets();
    offsets::api_ground_velocity_ready=false;
    offsets::api_ballistics_ready=false;
    offsets::api_reload_ready=false;
    offsets::api_invulnerable_ready=false;
    offsets::api_invul_timer_ready=false;
    offsets::api_air_velocity_ready=false;
    offsets::api_air_acceleration_ready=false;
    auto path=cache_path();
    offset_api_payload::manifest cached;
    std::string error;
    bool loaded=read_cache(path,cached,error);
    if(!loaded && expected_cache_path(path)) {
        auto backup=path;backup+=L".bak";
        loaded=read_cache(backup,cached,error);
        if(loaded)CopyFileW(backup.c_str(),path.c_str(),FALSE);
    }
    if(loaded && apply_manifest(cached))
        set_status("Caché API validada: versión "+cached.version);
    if(vault.exists())refresh(false);
    else if(!loaded)set_status("Sin clave ni caché API: usando offsets incorporados.");
}
inline bool load_on_reconnect() {
    restore_built_in_offsets();
    offsets::api_ground_velocity_ready=false;
    offsets::api_ballistics_ready=false;
    offsets::api_reload_ready=false;
    offsets::api_invulnerable_ready=false;
    offsets::api_invul_timer_ready=false;
    offsets::api_air_velocity_ready=false;
    offsets::api_air_acceleration_ready=false;
    if(cache_quarantined.load(std::memory_order_relaxed))return false;
    offset_api_payload::manifest cached;
    std::string error;
    auto file=cache_path();
    if(read_cache(file,cached,error))return apply_manifest(cached);
    if(expected_cache_path(file)) {
        auto backup=file;backup+=L".bak";
        if(read_cache(backup,cached,error)) {
            CopyFileW(backup.c_str(),file.c_str(),FALSE);
            return apply_manifest(cached);
        }
    }
    return false; // built-in defaults, never legacy offsets.txt
}
inline bool load_backup_for_recovery_at(const std::filesystem::path& cache) {
    if(!expected_cache_path(cache))return false;
    auto file=cache;file+=L".bak";
    offset_api_payload::manifest backup;
    std::string error;
    if(!read_cache(file,backup,error))return false;
    restore_built_in_offsets();
    offsets::api_ground_velocity_ready=false;
    offsets::api_ballistics_ready=false;
    offsets::api_reload_ready=false;
    offsets::api_invulnerable_ready=false;
    offsets::api_invul_timer_ready=false;
    offsets::api_air_velocity_ready=false;
    offsets::api_air_acceleration_ready=false;
    return apply_manifest(backup); // still requires the live pointer check in sdk::init
}
inline bool load_backup_for_recovery() {return load_backup_for_recovery_at(cache_path());}
inline bool accept_recovered_backup_at(const std::filesystem::path& file) {
    if(!expected_cache_path(file))return false;
    auto backup=file;backup+=L".bak";
    if(!CopyFileW(backup.c_str(),file.c_str(),FALSE)) {
        cache_quarantined.store(true,std::memory_order_relaxed);
        set_status("API: respaldo válido en memoria pero no se pudo restaurar el archivo.");
        return false;
    }
    pending_validation.store(false,std::memory_order_relaxed);
    cache_quarantined.store(false,std::memory_order_relaxed);
    set_status("API: caché anterior restaurada tras validar punteros con aces.exe.");
    return true;
}
inline bool accept_recovered_backup(){return accept_recovered_backup_at(cache_path());}
inline void reject_incompatible_cache() {
    cache_quarantined.store(true,std::memory_order_relaxed);
    set_status("API: punteros incompatibles con aces.exe; caché omitida hasta actualizar manualmente. Se probaron los valores incorporados.");
}
inline void runtime_validated(){pending_validation.store(false,std::memory_order_relaxed);}
inline bool rollback_after_failure(){
    if(!pending_validation.exchange(false,std::memory_order_relaxed))return false;
    auto file=cache_path();
    if(!expected_cache_path(file))return false;
    auto backup=file;backup+=L".bak";
    offset_api_payload::manifest cached;
    std::string error;
    restore_built_in_offsets();
    offsets::api_ground_velocity_ready=false;
    offsets::api_ballistics_ready=false;
    offsets::api_reload_ready=false;
    offsets::api_invulnerable_ready=false;
    offsets::api_invul_timer_ready=false;
    offsets::api_air_velocity_ready=false;
    offsets::api_air_acceleration_ready=false;
    if(!read_cache(backup,cached,error)||!CopyFileW(backup.c_str(),file.c_str(),FALSE)) {
        auto failed=file;failed+=L".failed";
        MoveFileExW(file.c_str(),failed.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        set_status("API: SDK no inició; sin backup, usando valores incorporados.");return false;
    }
    apply_manifest(cached);
    set_status("API: offsets nuevos rechazados al iniciar; restaurada caché anterior.");
    return true;
}

}
