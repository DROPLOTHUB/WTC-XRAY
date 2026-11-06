#include "../src/core/offset_api_payload.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
int main() {
    const auto fixture=std::filesystem::path(__FILE__).parent_path()/"free_offsets_fixture.json";
    std::ifstream file(fixture,std::ios::binary);
    assert(file.is_open());
    const std::string json((std::istreambuf_iterator<char>(file)),{});
    offset_api_payload::manifest current;
    std::string error;
    assert(offset_api_payload::parse(json,current,error));
    assert(current.entries.at("Global.g_RocketListIndexPtr")!=0);
    assert(current.entries.at("MissileNameCont.m_Type")==0x870);
    assert(current.entries.size()>=60 && !current.version.empty());
    const auto previous=current.version;
    auto invalid=json;
    auto i=invalid.find("warthunder");assert(i!=std::string::npos);
    invalid.replace(i,10,"other_game");
    assert(!offset_api_payload::parse(invalid,current,error) && current.version==previous);
    invalid=json;i=invalid.find("0x870");assert(i!=std::string::npos);
    invalid.replace(i,5,"0xZ70");
    assert(!offset_api_payload::parse(invalid,current,error));
    invalid=json;i=invalid.find("\"count\":65");assert(i!=std::string::npos);
    invalid.replace(i,10,"\"count\":5 ");
    assert(!offset_api_payload::parse(invalid,current,error));
    std::puts("PASS public JSON schema, offsets and invalid payload rejection");
}
