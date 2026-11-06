#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "language_catalog.hpp"

namespace runtime_language {
inline std::atomic<int> selected_language{1};
inline int normalize(int language){return language==0?0:1;}
inline void set_language(int language){selected_language.store(normalize(language),std::memory_order_relaxed);}
inline int language(){return selected_language.load(std::memory_order_relaxed);}
struct raw_value {std::string value;};
inline raw_value raw(std::string_view value){return {std::string(value)};}
inline raw_value raw(const char* value){return {value?value:""};}
namespace detail {
struct indexed {std::string_view key;const entry* message=nullptr;std::size_t order=0;};
inline const entry* exact(std::string_view source){
    static const auto index=[] {
        std::array<indexed,std::size(messages)*2> result{};std::size_t i=0;
        for(const auto& item:messages){result[i]={item.es,&item,i};++i;result[i]={item.en,&item,i};++i;}
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.key==b.key?a.order<b.order:a.key<b.key;});
        return result;
    }();
    const auto found=std::lower_bound(index.begin(),index.end(),source,[](const auto& item,std::string_view key){return item.key<key;});
    return found!=index.end()&&found->key==source?found->message:nullptr;
}
struct pattern {std::vector<std::string> literals;std::vector<char> types;bool valid=true;};
inline pattern parse(std::string_view source){
    pattern out;out.literals.emplace_back();
    for(std::size_t i=0;i<source.size();){
        if(source[i]!='%'){out.literals.back()+=source[i++];continue;}
        if(i+1<source.size()&&source[i+1]=='%'){out.literals.back()+='%';i+=2;continue;}
        ++i;
        while(i<source.size()&&std::string_view("-+ #0.123456789hljztLI*").find(source[i])!=std::string_view::npos)++i;
        if(i>=source.size()||std::string_view("diuoxXfFeEgGaAcsp").find(source[i])==std::string_view::npos){out.valid=false;return out;}
        out.types.push_back(source[i++]);out.literals.emplace_back();
    }
    return out;
}
struct paired_pattern {pattern es,en;};
inline const auto& patterns(){
    static const auto all=[] {
        std::vector<paired_pattern> out;
        for(const auto& item:messages){
            if(std::string_view(item.es).find('%')==std::string_view::npos&&std::string_view(item.en).find('%')==std::string_view::npos)continue;
            auto es=parse(item.es),en=parse(item.en);
            if(es.valid&&en.valid&&!es.types.empty()&&es.types==en.types)out.push_back({std::move(es),std::move(en)});
        }
        return out;
    }();return all;
}
inline bool match(std::string_view source,const pattern& pattern,std::vector<std::string_view>& fields){
    if(pattern.literals.front().size()<3||!source.starts_with(pattern.literals.front()))return false;
    std::size_t position=pattern.literals.front().size();fields.clear();
    for(std::size_t i=0;i<pattern.types.size();++i){
        const auto& delimiter=pattern.literals[i+1];
        const auto end=delimiter.empty()?source.size():source.find(delimiter,position);
        if(end==std::string_view::npos)return false;
        const auto field=source.substr(position,end-position);
        if(pattern.types[i]!='s'&&pattern.types[i]!='c'&&field.find_first_not_of("0123456789+-.,xXaAbBcCdDeEfFpPnNiItyTY ")!=std::string_view::npos)return false;
        fields.push_back(field);position=end+delimiter.size();
    }
    return position==source.size();
}
inline std::string translate(std::string_view source,int lang,unsigned depth=0){
    if(source.empty())return {};
    if(const auto* item=exact(source))return lang==0?item->es:item->en;
    if(depth>=5)return std::string(source);
    // Translate formatted diagnostic sentences without changing captured data.
    std::vector<std::string_view> fields;
    for(const auto& pair:patterns())for(int from=0;from<2;++from){
        const auto& original=from==0?pair.es:pair.en;
        if(!match(source,original,fields))continue;
        const auto& destination=lang==0?pair.es:pair.en;
        std::string result=destination.literals.front();
        for(std::size_t i=0;i<fields.size();++i){
            if(original.types[i]=='s'&&exact(fields[i]))result+=translate(fields[i],lang,depth+1);
            else result+=fields[i];
            result+=destination.literals[i+1];
        }
        return result;
    }
    // Prefix messages concatenate provider versions/errors or counters at runtime.
    const entry* prefix=nullptr;std::size_t prefix_size=0;
    for(const auto& item:messages)for(const char* key:{item.es,item.en}){
        const std::string_view part(key);
        if(part.size()<4||part.size()<=prefix_size||part.back()!=' '||!source.starts_with(part))continue;
        prefix=&item;prefix_size=part.size();
    }
    if(prefix)return std::string(lang==0?prefix->es:prefix->en)+translate(source.substr(prefix_size),lang,depth+1);
    if(source.find('\n')!=std::string_view::npos){
        std::string result;std::size_t position=0;
        while(position<source.size()){
            const auto end=source.find('\n',position);
            result+=translate(source.substr(position,end==std::string_view::npos?end:end-position),lang,depth+1);
            if(end==std::string_view::npos)break;
            result+='\n';position=end+1;
        }
        return result;
    }
    return std::string(source);
}
template<class T> inline auto argument(T value,int lang){
    if constexpr(std::is_same_v<std::decay_t<T>,raw_value>)return std::move(value.value);
    else if constexpr(std::is_convertible_v<T,const char*>){const char* p=value;return translate(p?std::string_view(p):std::string_view{},lang);}
    else if constexpr(std::is_same_v<std::decay_t<T>,std::string>||std::is_same_v<std::decay_t<T>,std::string_view>)return translate(value,lang);
    else return value;
}
template<class T> inline decltype(auto) pointer(T& value){
    if constexpr(std::is_same_v<T,std::string>)return value.c_str();else return (value);
}
}
inline std::string text(std::string_view source,int selected=-1){
    const int lang=selected<0?language():normalize(selected);
    struct cache {int language=-1;std::unordered_map<std::string,std::string> values;};
    thread_local cache previous;
    if(previous.language!=lang){previous.values.clear();previous.language=lang;}
    if(source.size()>8192)return detail::translate(source,lang);
    const auto key=std::string(source);
    if(const auto found=previous.values.find(key);found!=previous.values.end())return found->second;
    auto result=detail::translate(source,lang);
    if(previous.values.size()>=256)previous.values.clear();
    previous.values.emplace(key,result);return result;
}
inline std::string text(const char* source,int selected=-1){return text(source?std::string_view(source):std::string_view{},selected);}
template<class... A> inline std::string format(const char* source,int selected,A... values){
    const int lang=selected<0?language():normalize(selected);
    const auto translated=text(source,lang);
    auto arguments=std::tuple{detail::argument(std::move(values),lang)...};
    return std::apply([&](auto&... args){
        const int size=std::snprintf(nullptr,0,translated.c_str(),detail::pointer(args)...);
        if(size<0||size>1024*1024)return translated;
        std::string result(static_cast<std::size_t>(size),'\0');
        std::snprintf(result.data(),result.size()+1,translated.c_str(),detail::pointer(args)...);
        return result;
    },arguments);
}
}
