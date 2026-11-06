#include "../src/core/offset_credentials.hpp"
#include <cassert>
#include <cstdio>
int main() {
    const auto name=L"WTC.Private.OffsetTest."+std::to_wstring(GetCurrentProcessId())+
                    L"."+std::to_wstring(GetTickCount64());
    offset_credentials::store vault(name);
    assert(!vault.save("line\r\nbreak"));
    assert(vault.save("test-token-not-a-real-key"));
    assert(vault.exists());
    bool matched=false;
    assert(vault.with_secret([&](std::string_view token){matched=token=="test-token-not-a-real-key";}));
    assert(matched);
    assert(vault.erase());
    assert(!vault.exists());
    std::puts("PASS credential stored only in Windows vault (synthetic token)");
}
