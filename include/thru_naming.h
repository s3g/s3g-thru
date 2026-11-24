#pragma once
#include <array>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>
#include <cctype>
namespace s3g_thru_names {
using Name=std::array<char,41>;
inline bool valid(const std::string& s){if(s.size()>40)return false;for(unsigned char c:s)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-'))return false;return true;}
inline void assign(Name& out,const std::string& s){out={};std::copy_n(s.begin(),std::min<std::size_t>(40,s.size()),out.begin());}
struct Span {bool present=false,valid=true;std::size_t begin=0,end=0;};
inline Span span(const std::string& text){
 const std::string tag="\"names_v1\"";const auto at=text.find(tag);if(at==std::string::npos)return {};
 Span result;result.present=true;auto pos=at+tag.size();
 const auto spaces=[&]{while(pos<text.size()&&std::isspace(static_cast<unsigned char>(text[pos])))++pos;};
 spaces();if(pos>=text.size()||text[pos++]!=':'){result.valid=false;return result;}
 spaces();if(pos>=text.size()||text[pos++]!='"'){result.valid=false;return result;}
 result.begin=pos;result.end=text.find('"',pos);result.valid=result.end!=std::string::npos&&text.find(tag,result.end+1)==std::string::npos;return result;
}
inline std::string blob(const char* json){const std::string text=json?json:"";const auto s=span(text);return !s.valid?"!":s.present?text.substr(s.begin,s.end-s.begin):"";}
template<class Clips,class Banks> bool read(const char* json,Clips& clips,Banks& banks,Name& project,bool validateOnly=false){
 const auto text=blob(json);
 struct Item{char kind;int a,b;std::string name;};std::vector<Item> items;std::set<std::string> seen;std::size_t begin=0;
 while(begin<text.size()){
  const auto end=text.find('|',begin),colon=text.find(':',begin);if(colon==std::string::npos||(end!=std::string::npos&&colon>end))return false;
  const auto key=text.substr(begin,colon-begin),name=text.substr(colon+1,(end==std::string::npos?text.size():end)-colon-1);if(key.empty()||!valid(name)||!seen.insert(key).second)return false;
  int a=-1,b=-1;char tail=0;const char kind=key[0];bool ok=key=="P";
  if(kind=='C')ok=std::sscanf(key.c_str(),"C%d%c",&a,&tail)==1&&a>=0&&a<64;
  if(kind=='K')ok=std::sscanf(key.c_str(),"K%d%c",&a,&tail)==1&&a>=0&&a<16;
  if(kind=='B')ok=std::sscanf(key.c_str(),"B%d,%d%c",&a,&b,&tail)==2&&a>=0&&a<64&&b>=0&&b<16;
  if(!ok)return false;
  items.push_back({kind,a,b,name});if(end==std::string::npos)break;begin=end+1;
 }
 if(validateOnly)return true;
 project={};for(auto& c:clips){c.name={};for(auto& slot:c.bankSlots)slot.name={};}for(auto& bank:banks)bank.name={};
 for(const auto& i:items){if(i.kind=='P')assign(project,i.name);else if(i.kind=='C')assign(clips[i.a].name,i.name);else if(i.kind=='K')assign(banks[i.a].name,i.name);else assign(clips[i.a].bankSlots[i.b].name,i.name);}return true;
}
template<class Clips,class Banks> bool append(char* json,std::size_t size,const Clips& clips,const Banks& banks,const Name& project){
 std::string names;const auto add=[&](const std::string& key,const Name& n){if(!n[0])return;if(!names.empty())names+='|';names+=key+":"+n.data();};add("P",project);
 for(std::size_t c=0;c<clips.size();++c){add("C"+std::to_string(c),clips[c].name);for(int b=0;b<16;++b)add("B"+std::to_string(c)+","+std::to_string(b),clips[c].bankSlots[b].name);}
 for(std::size_t b=0;b<banks.size();++b)add("K"+std::to_string(b),banks[b].name);
 std::string text=json;const auto found=span(text);if(!found.valid)return false;
 if(found.present)text.replace(found.begin,found.end-found.begin,names);
 else if(!names.empty()){const auto end=text.rfind('}');if(end==std::string::npos)return false;text.insert(end,",\"names_v1\":\""+names+"\"");}
 if(text.size()>=size)return false;
 std::memcpy(json,text.c_str(),text.size()+1);return true;
}
} // namespace s3g_thru_names
