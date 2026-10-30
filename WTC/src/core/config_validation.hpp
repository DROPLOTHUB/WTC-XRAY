#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>

namespace config_validation
{
    inline float parse_float(const std::string& value, float fallback) {
        try {
            std::size_t consumed=0;
            const float parsed=std::stof(value,&consumed);
            return consumed==value.size() && std::isfinite(parsed)?parsed:fallback;
        } catch (...) { return fallback; }
    }
    inline int parse_int(const std::string& value, int fallback) {
        try {
            std::size_t consumed=0;
            const int parsed=std::stoi(value,&consumed);
            return consumed==value.size()?parsed:fallback;
        } catch (...) { return fallback; }
    }
    inline bool parse_rgba(const std::string& value, std::array<float,4>& out) {
        std::istringstream stream(value);
        std::array<float,4> next{};
        char a=0,b=0,c=0;
        if(!(stream>>next[0]>>a>>next[1]>>b>>next[2]>>c>>next[3])) return false;
        stream>>std::ws;
        if(!stream.eof() || a!=',' || b!=',' || c!=',') return false;
        for(auto& channel:next) {
            if(!std::isfinite(channel)) return false;
            channel=std::clamp(channel,0.f,1.f);
        }
        out=next;
        return true;
    }
}
