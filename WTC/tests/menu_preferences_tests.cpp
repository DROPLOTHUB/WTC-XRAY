#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>
#include "../src/gui/menu_preferences.hpp"
// The Spanish label table must key every English label once so each setting has
// a unique row and favorites stay unambiguous.
static std::vector<std::string> keys(const std::string& source) {
    std::vector<std::string> out;const std::size_t table=source.find("static constexpr entry labels[]");
    assert(table!=std::string::npos);
    const std::size_t limit=source.find("};",table);
    assert(limit!=std::string::npos);
    std::size_t i=table;
    while((i=source.find('"',i))!=std::string::npos && i<limit) {
        const std::size_t end=source.find('"',i+1);
        assert(end!=std::string::npos && end<limit);
        out.push_back(source.substr(i+1,end-i-1));
        i=end+1;
    }
    return out;
}
int main() {
    using namespace menu_preferences;
    assert(!current.advanced && current.g2a_key==0 && current.favorites.empty());
    assert(current.language==1 && current.filter_static_air);
    current.language=0; // Exercise the legacy Spanish table explicitly.
    assert(std::string(label("Render Distance"))=="Distancia del ESP");
    assert(std::string(label("Unknown Option"))=="Unknown Option");
    assert(std::string(label(""))=="");
    for(const char* key:{"Alerta visual de aproximacion","Marcar resultados"})
        assert(std::string(label(key))==key);
    for(const char* key:{"Enable Air Lead","Turn Prediction (3D)","Aimbot key","Only Incoming Missiles",
        "Live Projectile ESP (experimental ECS)","Vehicle Name","Distance","SPAA","Air Lead key"})
        assert(std::string(label(key))!=key);
    std::ifstream input(std::filesystem::path(__FILE__).parent_path().parent_path()/"src"/"gui"/"menu_preferences.hpp");
    assert(input);
    const std::string source{std::istreambuf_iterator<char>(input),{}};
    const auto english=keys(source);
    std::set<std::string> unique;
    for(std::size_t i=0;i<english.size();) {
        if(english[i]=="cell"){i+=2;continue;}
        if(i+1>=english.size())break;
        assert(unique.insert(english[i]).second);
        const char* translated=label(english[i].c_str());
        assert(translated!=english[i].c_str() || std::string(english[i])==english[i+1]);
        assert(std::string(translated)==english[i+1]);
        assert(!english[i+1].empty());
        i+=2;
    }
    assert(unique.size()>=60);
    toggle_favorite("Enable ESP");toggle_favorite("Render Distance");
    assert(current.favorites.size()==2 && current.favorites.contains("Render Distance"));
    toggle_favorite("Render Distance");
    assert(current.favorites.size()==1 && !current.favorites.contains("Render Distance"));
    toggle_favorite(std::string());
    toggle_favorite(std::string(97,'a'));
    toggle_favorite(std::string("bad\x01name"));
    assert(current.favorites.size()==1);
    for(int i=0;i<200;++i)toggle_favorite("favorite_"+std::to_string(i));
    assert(current.favorites.size()<=128);
    settings a,b;
    assert(a==b);
    b.advanced=true;
    assert(!(a==b));
    a.advanced=true;
    assert(a==b);
    b.language=0;
    assert(!(a==b));
    a.language=0;
    assert(a==b);
    b.filter_static_air=false;
    assert(!(a==b));
    a.filter_static_air=false;
    assert(a==b);
    a.g2a_key=0x47;
    assert(!(a==b) && a.g2a_key==0x47);
    std::puts("PASS favorites bounded/validated, unique reusable Spanish labels and preference equality");
}
