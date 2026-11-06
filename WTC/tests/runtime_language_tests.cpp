#include "../src/gui/runtime_language.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <atomic>
#include <set>
int main(){
 using namespace runtime_language;
 assert(language()==1);set_language(0);assert(language()==0);set_language(-42);assert(language()==1);
 std::size_t count=0;
 for(const auto& item:messages){
  const auto en=text(item.es,1);
  if(en!=item.en){std::fprintf(stderr,"Catalog conflict at message %zu\n",count);return 1;}
  assert(!text(item.en,0).empty());
  const auto es_pattern=detail::parse(item.es),en_pattern=detail::parse(item.en);
  assert(es_pattern.valid==en_pattern.valid);
  if(es_pattern.valid)assert(es_pattern.types==en_pattern.types);
  ++count;
 }
 assert(count>=300);
 assert(format("%s",1,"Sin lectura")=="No read");
 assert(format("%s",0,"No read")=="Sin lectura");
 assert(format("%s | %.1f | %zu | %08X | %%",1,raw("Sin lectura 100%"),1.25,std::size_t{4096},42u)=="Sin lectura 100% | 1.2 | 4096 | 0000002A | %");
 assert(format("%s",1,raw(std::string("Sin lectura")))=="Sin lectura");
 assert(text("F-16C / user model 100%",0)=="F-16C / user model 100%");
 assert(text("Sin lectura\nF-16C / user model 100%\n",1)=="No read\nF-16C / user model 100%\n");
 // Exercise every formatted catalog template with data captured at runtime.
 unsigned templates=0;
 for(const auto& item:messages){
  const auto es=detail::parse(item.es),en=detail::parse(item.en);
  if(!es.valid||es.types.empty()||es.types!=en.types||es.literals.front().size()<3)continue;
  std::string input=es.literals.front(),expected=en.literals.front();
  for(std::size_t i=0;i<es.types.size();++i){
   const std::string value=es.types[i]=='s'?"MODEL_A100":es.types[i]=='c'?"Q":"123";
   input+=value;input+=es.literals[i+1];expected+=value;expected+=en.literals[i+1];
  }
  if(text(input,1)!=expected){std::fprintf(stderr,"Template translation failed at template %u\n",templates);return 2;}
  ++templates;
 }
 assert(templates>=10);
 std::atomic<bool> okay{true};
 std::thread en([&]{for(int i=0;i<2000;++i)if(text("Sin lectura",1)!="No read")okay=false;});
 std::thread es([&]{for(int i=0;i<2000;++i)if(text("No read",0)!="Sin lectura")okay=false;});
 for(int i=0;i<1000;++i)set_language(i%2);
 en.join();es.join();assert(okay);
 set_language(1);
 std::printf("PASS %zu runtime bilingual messages, %u dynamic templates, raw names/percent data, numeric formats and thread-local language switches\n",count,templates);
}
