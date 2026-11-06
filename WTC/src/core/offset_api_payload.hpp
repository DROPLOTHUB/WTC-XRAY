#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace offset_api_payload {
struct json_value {
    enum class kind { null_, boolean, number, text, object, array } tag=kind::null_;
    std::string scalar;
    std::unordered_map<std::string,json_value> members;
    std::vector<json_value> elements;
    const json_value* get(std::string_view key) const {
        auto found=members.find(std::string(key));
        return found==members.end()?nullptr:&found->second;
    }
};
class reader {
    std::string_view input_;
    std::size_t index_=0, nodes_=0;
    void spaces() {while(index_<input_.size() && (input_[index_]==' ' || input_[index_]=='\n' ||
        input_[index_]=='\r' || input_[index_]=='\t')) ++index_;}
    bool take(char c) {spaces();if(index_>=input_.size()||input_[index_]!=c)return false;++index_;return true;}
    bool literal(std::string_view word) {
        if(input_.substr(index_,word.size())!=word)return false;
        index_+=word.size();return true;
    }
    static int hex(char c) {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='A'&&c<='F')return c-'A'+10;
        if(c>='a'&&c<='f')return c-'a'+10;
        return -1;
    }
    bool quoted(std::string& out) {
        if(!take('"'))return false;
        while(index_<input_.size()) {
            unsigned char c=static_cast<unsigned char>(input_[index_++]);
            if(c=='"')return true;
            if(c<32 || out.size()>4096)return false;
            if(c!='\\'){out.push_back(static_cast<char>(c));continue;}
            if(index_>=input_.size())return false;
            char escape=input_[index_++];
            switch(escape) {
            case '"': case '\\': case '/':out.push_back(escape);break;
            case 'b':out.push_back('\b');break;
            case 'f':out.push_back('\f');break;
            case 'n':out.push_back('\n');break;
            case 'r':out.push_back('\r');break;
            case 't':out.push_back('\t');break;
            case 'u': {
                if(index_+4>input_.size())return false;
                int value=0;
                for(int i=0;i<4;i++){int digit=hex(input_[index_++]);if(digit<0)return false;value=value*16+digit;}
                if(value>=0xD800 && value<=0xDFFF)return false;
                if(value<=0x7F)out.push_back(static_cast<char>(value));
                else if(value<=0x7FF){out.push_back(static_cast<char>(0xC0|(value>>6)));out.push_back(static_cast<char>(0x80|(value&63)));}
                else {out.push_back(static_cast<char>(0xE0|(value>>12)));out.push_back(static_cast<char>(0x80|((value>>6)&63)));out.push_back(static_cast<char>(0x80|(value&63)));}
                break;
            }
            default:return false;
            }
        }
        return false;
    }
    bool number(std::string& out) {
        const auto start=index_;
        if(index_<input_.size()&&input_[index_]=='-')++index_;
        if(index_>=input_.size())return false;
        if(input_[index_]=='0')++index_;
        else {
            if(input_[index_]<'1'||input_[index_]>'9')return false;
            while(index_<input_.size()&&input_[index_]>='0'&&input_[index_]<='9')++index_;
        }
        if(index_<input_.size()&&input_[index_]=='.') {
            ++index_;const auto begin=index_;
            while(index_<input_.size()&&input_[index_]>='0'&&input_[index_]<='9')++index_;
            if(index_==begin)return false;
        }
        if(index_<input_.size()&&(input_[index_]=='e'||input_[index_]=='E')) {
            ++index_;if(index_<input_.size()&&(input_[index_]=='+'||input_[index_]=='-'))++index_;
            const auto begin=index_;
            while(index_<input_.size()&&input_[index_]>='0'&&input_[index_]<='9')++index_;
            if(index_==begin)return false;
        }
        out=std::string(input_.substr(start,index_-start));return true;
    }
    bool value(json_value& out,int depth) {
        spaces();if(depth>12||++nodes_>12000||index_>=input_.size())return false;
        char c=input_[index_];
        if(c=='"'){out.tag=json_value::kind::text;return quoted(out.scalar);}
        if(c=='{'){
            out.tag=json_value::kind::object;++index_;if(take('}'))return true;
            do {
                std::string key;
                if(!quoted(key)||!take(':'))return false;
                json_value child;
                if(!value(child,depth+1)||!out.members.emplace(std::move(key),std::move(child)).second)return false;
                if(take('}'))return true;
            }while(take(','));
            return false;
        }
        if(c=='['){
            out.tag=json_value::kind::array;++index_;if(take(']'))return true;
            do {
                if(out.elements.size()>=2048)return false;
                json_value child;
                if(!value(child,depth+1))return false;
                out.elements.push_back(std::move(child));
                if(take(']'))return true;
            }while(take(','));
            return false;
        }
        if(literal("true")||literal("false")){out.tag=json_value::kind::boolean;return true;}
        if(literal("null")){out.tag=json_value::kind::null_;return true;}
        out.tag=json_value::kind::number;return number(out.scalar);
    }
public:
    explicit reader(std::string_view input):input_(input){}
    bool parse(json_value& out){return input_.size()<=512*1024 && value(out,0) && (spaces(),index_==input_.size());}
};
struct manifest {
    std::string version,tier;
    std::unordered_map<std::string,std::uintptr_t> entries;
};
inline bool parse_hex(std::string_view value,std::uintptr_t& result){
    if(value.size()<3||value.substr(0,2)!="0x")return false;
    value.remove_prefix(2);
    std::uint64_t parsed=0;
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),parsed,16);
    if(error!=std::errc{}||end!=value.data()+value.size()||parsed>0xFFFFFFFFULL)return false;
    result=static_cast<std::uintptr_t>(parsed);return true;
}
inline bool parse(std::string_view response,manifest& result,std::string& error){
    json_value root;
    if(!reader(response).parse(root)||root.tag!=json_value::kind::object){error="JSON inválido o demasiado grande";return false;}
    auto field=[&](const json_value& object,std::string_view key,json_value::kind kind)->const json_value*{
        auto found=object.get(key);return found&&found->tag==kind?found:nullptr;
    };
    const auto game=field(root,"game",json_value::kind::text);
    const auto channel=field(root,"channel",json_value::kind::text);
    const auto version=field(root,"version",json_value::kind::text);
    const auto tier=field(root,"tier",json_value::kind::text);
    const auto count=field(root,"count",json_value::kind::number);
    const auto records=field(root,"offsets",json_value::kind::array);
    if(!game||game->scalar!="warthunder"||!channel||channel->scalar!="live"||
       !version||!tier||!count||!records){error="Juego, canal o formato no coinciden";return false;}
    if(version->scalar.size()>32||version->scalar.find('.')==std::string::npos){error="Versión no reconocida";return false;}
    for(char c:version->scalar)if((c<'0'||c>'9')&&c!='.'){error="Versión no reconocida";return false;}
    std::size_t declared=0;
    const auto [end,ec]=std::from_chars(count->scalar.data(),count->scalar.data()+count->scalar.size(),declared);
    if(ec!=std::errc{}||end!=count->scalar.data()+count->scalar.size()||
       declared!=records->elements.size()||declared<60){error="Lista incompleta de offsets";return false;}
    manifest next{version->scalar,tier->scalar,{}};
    for(const auto& record:records->elements){
        if(record.tag!=json_value::kind::object){error="Registro de offset inválido";return false;}
        const auto group=field(record,"group",json_value::kind::text);
        const auto name=field(record,"name",json_value::kind::text);
        const auto value=field(record,"value",json_value::kind::text);
        if(!group||!name||!value||group->scalar.empty()||group->scalar.size()>70||
           name->scalar.empty()||name->scalar.size()>90){error="Offset sin grupo o nombre";return false;}
        for(char c:group->scalar)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c==':' )){error="Grupo de offset inválido";return false;}
        for(char c:name->scalar)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                                    (c>='0'&&c<='9')||c=='_')){error="Nombre de offset inválido";return false;}
        std::uintptr_t address=0;
        if(!parse_hex(value->scalar,address)){error="Valor hexadecimal inválido";return false;}
        if(!next.entries.emplace(group->scalar+"."+name->scalar,address).second){error="Offset duplicado";return false;}
    }
    result=std::move(next);error.clear();return true;
}
}
