#pragma once
#include <string>
#include <string_view>

// Actualizar solo este nombre al publicar una entrega nueva.
namespace app_version {
    inline constexpr char release_name[]="v5.16 A2A Movimiento Relativo";
    inline const char* sidebar_label() {
        static const std::string label=[] {
            const std::string_view name(release_name);
            return std::string("TACTICAL / ")+std::string(name.substr(0,name.find(' ')));
        }();
        return label.c_str();
    }
}
