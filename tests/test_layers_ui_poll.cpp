#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using Json=s3g_thru_json::Value;
int main(int argc,char** argv){
 char folder[]="/tmp/thru-layer-ui.XXXXXX";assert(mkdtemp(folder));auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->setPerformanceMode(5);setParam(t.get(),"layers_glide","5");
 auto poll=[&](const char* group){char b[8192]{};const std::string key="ui_poll_"+std::string(group);const auto n=getParamValue(t.get(),key.c_str(),b,sizeof(b));assert(n>0&&n<int(sizeof(b)));auto result=s3g_thru_json::parse(b);assert(result.at("v").number()==1);return result;};
 auto layers=[&](){return s3g_thru_json::parse(poll("fast").at("values").at("layers_cursor_state").string());};
 t->padNoteOn(68+28,100);t->layersPadPressure(28,127);t->padNoteOff(68+28);t->beginClipRecording();std::array<int16_t,256> out{};auto render=[&](int blocks){for(int i=0;i<blocks;++i)t->render(out.data(),128);};
 render(70);t->padNoteOn(68+31,100);t->layersPadPressure(31,127);render(70);t->padNoteOff(68+31);t->padNoteOn(68+4,100);t->layersPadPressure(4,127);render(70);t->padNoteOff(68+4);t->finishClipRecording();t->setClipPlaying(false);t->setClipPlaying(true);
 Json frames=Json::array();bool visitedA=false,visitedB=false,visitedC=false;std::size_t largestFast=0;
 for(int i=0;i<240;++i){render(1);if(i%7)continue;const auto fast=poll("fast");largestFast=std::max(largestFast,s3g_thru_json::stringify(fast).size());const auto current=s3g_thru_json::parse(fast.at("values").at("layers_cursor_state").string());assert(current.at("clip").number()==0&&current.at("active").number()==1);const auto& mix=current.at("mix").arrayValue();const auto& real=t->layersRack.currentWeights();for(int c=0;c<4;++c)assert(std::abs(mix[c].number()-real[c])<.00001);
  visitedA|=mix[0].number()>.99;visitedB|=mix[1].number()>.99;visitedC|=mix[2].number()>.99;assert(current.at("down").number()==0&&current.at("latched").number()==0);frames.push(fast);
 }
 assert(visitedA&&visitedB&&visitedC);const auto status=poll("layers");const auto full=s3g_thru_json::parse(status.at("values").at("layers_view").string());assert(full.at("clip").number()==0&&full.at("active").number()==1);
 // Single clip data is explicitly owned by that clip too. Returning to the
 // Layers clip must not accept that inactive snapshot as its current layout.
 t->captureActiveClip(true);t->clipSlots[1]=*t->activeClipSlot();t->clipSlots[1].layers.active=false;t->clipSlots[1].splitEnabled=false;t->clipSlots[1].sequence.active=false;t->activateStoredClip(1,false);assert(t->performanceMode()==0);const auto single=poll("layers");const auto singleLayer=s3g_thru_json::parse(single.at("values").at("layers_view").string());assert(singleLayer.at("clip").number()==1&&singleLayer.at("active").number()==0);
 t->activateStoredClip(0,false);assert(t->performanceMode()==5);assert(layers().at("clip").number()==0);const auto returned=poll("layers");assert(s3g_thru_json::parse(returned.at("values").at("layers_view").string()).at("active").number()==1);
 t->setClipPlaying(false);setParam(t.get(),"layers_fx_x","0.5");setParam(t.get(),"layers_fx_y","0.5");render(40);auto center=layers();for(const auto& w:center.at("mix").arrayValue())assert(std::abs(w.number()-.25)<.001);
 t->padNoteOn(68+31,100);auto held=layers();assert(held.at("down").number()==double(std::uint32_t(1)<<31));t->shiftHeld=true;t->padNoteOn(68+7,100);auto latched=layers();assert(latched.at("latched").number()==double(std::uint32_t(1)<<7));t->padNoteOff(68+31);t->padNoteOff(68+7);t->shiftHeld=false;
 setParam(t.get(),"algorithm","20");assert(poll("status").at("values").at("partials_state").isString());
 t->setPerformanceMode(3);for(int p=0;p<4;++p){t->selectSplitPart(p);setParam(t.get(),"algorithm","2");setParam(t.get(),"live_source","1");}const auto quad=poll("fast");const auto bytes=s3g_thru_json::stringify(quad).size();assert(bytes<8192);
 if(argc>1){auto fixture=Json::object();fixture["frames"]=std::move(frames);fixture["layersStatus"]=returned;fixture["singleStatus"]=single;fixture["quadFastBytes"]=double(bytes);std::ofstream(argv[1])<<s3g_thru_json::stringify(fixture);}
 std::cout<<"Layers fast UI: native clip replay mix/LEDs, stopped XY, touch/latch bits, owned Single/return snapshots and 8192-byte packet bounds: PASS (active "<<largestFast<<", quad "<<bytes<<")\n";
 t.reset();std::filesystem::remove_all(folder);
}
