#include "../src/core/offset_auto_update.hpp"
#include <cstdio>
int main() {
    std::string body,error;
    if(!offset_auto_update::download({},body,error)) {
        std::fprintf(stderr,"Public endpoint: %s\n",error.c_str());return 1;
    }
    offset_api_payload::manifest payload;
    if(!offset_api_payload::parse(body,payload,error)) {
        std::fprintf(stderr,"Public JSON: %s\n",error.c_str());return 2;
    }
    if(payload.entries.size()<60)return 3;
    std::printf("PASS public HTTPS/WinHTTP JSON v%s, %zu offsets (no key)\n",
        payload.version.c_str(),payload.entries.size());
}
