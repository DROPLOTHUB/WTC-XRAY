#include <cassert>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "../src/gui/runtime_language.hpp"

static std::string source(const char* name) {
    std::ifstream input(std::filesystem::path(__FILE__).parent_path().parent_path()/name);
    assert(input); return {std::istreambuf_iterator<char>(input),{}};
}
static std::vector<std::string> contracts(std::string_view text) {
    static const std::regex spec(R"(%(?:%|[-+ #0]*(?:[0-9]+|\*)?(?:\.(?:[0-9]+|\*))?(?:hh|ll|I64|h|l|j|z|t|L)?[diuoxXfFeEgGaAcsp]))");
    const std::string value(text); std::vector<std::string> result;
    for(std::sregex_iterator it(value.begin(),value.end(),spec),end;it!=end;++it) result.push_back(it->str());
    return result;
}
static std::string unescape(std::string_view text) {
    std::string result;
    for(std::size_t i=0;i<text.size();++i) {
        if(text[i]!='\\'||i+1==text.size()){result+=text[i];continue;}
        switch(text[++i]) {case 'n':result+='\n';break;case 'r':result+='\r';break;case 't':result+='\t';break;case '0':result+='\0';break;default:result+=text[i];}
    }
    return result;
}
static bool known(std::string_view value) {
    for(const auto& item:runtime_language::messages)if(value==item.es||value==item.en)return true;
    return false;
}
static void covered(std::string_view value) {
    if(!known(value)){std::fprintf(stderr,"Missing runtime catalog source: %.*s\n",int(value.size()),value.data());assert(false);}
}
static void console_inventory() {
    const std::set<std::string> data_only={"vmm: 0x%zx\n","ftd: 0x%zx\n","leech: 0x%zx\n",
        "[+] VMMDLL_ConfigGet"," ID = %lli","FPGA ID: %llu\n","DEVICE ID: %llu\n","PID: %i\n"};
    static const std::regex literal(R"lit("((?:\\.|[^"\\])*)")lit");
    for(const char* file:{"src/main.cpp","src/memory/memory.cpp","src/core/core.hpp","src/core/config.hpp","src/features/esp/projectile_esp.hpp"}) {
        const auto content=source(file);
        std::size_t offset=0;
        while((offset=content.find("LOG(",offset))!=std::string::npos) {
            const auto line=content.rfind('\n',offset);
            const auto prefix=content.substr(line==std::string::npos?0:line+1,offset-(line==std::string::npos?0:line+1));
            offset+=4;
            if(prefix.find("//")!=std::string::npos)continue;
            auto begin=content.find_first_not_of(" \t\r\n",offset);
            if(begin==std::string::npos||content[begin]!='"')continue;
            std::string format;
            for(;;) {
                std::match_results<std::string::const_iterator> match;
                if(!std::regex_search(content.cbegin()+begin,content.cend(),match,literal,std::regex_constants::match_continuous))break;
                format+=unescape(match[1].str());
                begin=content.find_first_not_of(" \t\r\n",begin+match.length());
                if(begin==std::string::npos||content[begin]!='"')break;
            }
            if(!data_only.contains(format))covered(format);
        }
    }
}
static void producer_inventory() {
    static const std::regex literal(R"lit("((?:\\.|[^"\\])*)")lit");
    static const std::regex producer(R"((?:status|_source|quality|reason|error)\s*=)");
    const std::set<std::string> data_only={"Unit.m_Velocity -> GroundMovement.m_Velocity","."};
    for(const char* file:{"src/game/sdk.hpp","src/core/core.hpp","src/core/startup_pointer_probe.hpp",
        "src/core/missile_registry_storage.hpp","src/core/offset_auto_update.hpp",
        "src/core/offset_api_payload.hpp","src/core/offset_api_mapping.hpp",
        "src/features/aimbot/aimbot.hpp","src/features/aimbot/aircraft_ammo_trace.hpp",
        "src/features/esp/esp.hpp","src/features/esp/g2a_settings.hpp"}) {
        const auto content=source(file); std::size_t begin=0;
        while(begin<content.size()) {
            const auto end=content.find('\n',begin);
            const auto line=content.substr(begin,end==std::string::npos?std::string::npos:end-begin);
            if(std::regex_search(line,producer)&&line.find("//")!=0) {
                for(std::sregex_iterator it(line.begin(),line.end(),literal),last;it!=last;++it) {
                    const auto value=unescape((*it)[1].str());
                    if(!value.empty()&&!data_only.contains(value))covered(value);
                }
            }
            if(end==std::string::npos)break; begin=end+1;
        }
    }
}
int main() {
    std::map<std::string,std::pair<std::string,std::string>> unique;
    for(const auto& item:runtime_language::messages) {
        const std::string es(item.es),en(item.en); assert(!es.empty()&&!en.empty());
        assert(contracts(es)==contracts(en));
        assert(std::count(es.begin(),es.end(),'\n')==std::count(en.begin(),en.end(),'\n'));
        assert(std::count(es.begin(),es.end(),'\r')==std::count(en.begin(),en.end(),'\r'));
        for(const auto& key:{es,en}) {
            const auto [it,inserted]=unique.emplace(key,std::make_pair(es,en));
            assert(inserted||it->second==std::make_pair(es,en));
            assert(runtime_language::text(key,0)==es);
            assert(runtime_language::text(key,1)==en);
        }
    }
    for(const char* status:{"Esperando datos","Sin muestra de municion","Puntero de canon invalido",
        "Sin convergencia balistica A2A","G2G solucion caducada","G2A solucion fuera de pantalla",
        "Registro: nombres nuevos guardados automaticamente","Antigua: no usar para confirmar el arma",
        "Slot activo fuera de rango","Sin clave ni caché API: usando offsets incorporados.",
        "API incompleta: falta offset crítico; conservando copia anterior","Waiting for aces.exe...",
        "Atajos inválidos/duplicados reparados al cargar (F1/F2 reservadas).",
        "Atajo rechazado: %s (reservado o en uso).","Mouse 1","Unknown monitor","Cazatanques"}) covered(status);
    console_inventory();
    producer_inventory();
    assert(runtime_language::text("API versión 2.3.4: 64 campos validados. Activos al conectar.",1)==
        "API version 2.3.4: 64 validated fields. Active on connection.");
    assert(runtime_language::text("Proveedor respondió con HTTP 503",1)=="Provider responded with HTTP 503");
    assert(runtime_language::text("Caché API validada: versión 2.3.4",1)=="Validated API cache: version 2.3.4");
    assert(runtime_language::format("Process information of %s\n",0,runtime_language::raw("Engine"))=="Información del proceso Engine\n");
    for(const char* raw:{"aces.exe","AIM-9M","DISPLAY1","Unit.m_Velocity -> GroundMovement.m_Velocity","C:\\Games\\offsets.txt"}) {
        assert(runtime_language::text(raw,0)==raw);assert(runtime_language::text(raw,1)==raw);
    }
    std::puts("PASS runtime catalog: complete console inventory, bidirectional entries, identical printf/newline contracts, states and raw-data preservation");
}
