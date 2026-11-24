#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <chrono>
using Json=s3g_thru_json::Value;
int main(){
 char folder[]="/tmp/thru-release-stability.XXXXXX";assert(mkdtemp(folder));auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();t->setPerformanceMode(5);
 auto query=[&](const char* key){std::array<char,kStateTextSize> b{};const int n=getParam(t.get(),key,b.data(),b.size());assert(n>0&&n<int(b.size()));return std::string(b.data(),n);};
 const auto state=query("state"),digest=query("rui_poll");assert(digest.size()<96);const auto rev=t->persistentRevision;
 int snapshots=0;std::string previous;std::array<int16_t,256> out{};
 for(int i=0;i<100;++i){const auto d=query("rui_poll"),revision=d.substr(0,d.find(':'));if(revision!=previous){query("state");snapshots++;previous=revision;}renderBlock(t.get(),out.data(),128);}
 assert(snapshots==1&&query("rui_poll")==digest&&t->persistentRevision==rev&&query("state")==state);
 const auto bench=[&](const char* key){const auto start=std::chrono::steady_clock::now();for(int i=0;i<100;++i)query(key);return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/100;};
 std::cout<<"Manager idle: 100 polls -> "<<snapshots<<" full snapshot; state "<<state.size()<<" bytes, digest "<<digest.size()<<" bytes; host mean full/digest "<<bench("state")<<"/"<<bench("rui_poll")<<" us\n";
 setParam(t.get(),"layers_fx_x","0.7");assert(query("rui_poll")!=digest);
 t->setPerformanceMode(6);auto seq=s3g_thru_json::parse(query("seq_state")),sound=s3g_thru_json::parse(query("seq_sound_state"));assert(seq.at("clip").number()==t->activeClip&&sound.at("clip").number()==t->activeClip);
 t->setPerformanceMode(0);setParam(t.get(),"algorithm","2");auto inactive=s3g_thru_json::parse(query("source_motion_state"));assert(inactive.at("active").number()==0&&inactive.at("clip").number()==t->activeClip&&inactive.at("mode").number()==0);
 setParam(t.get(),"live_source","1");auto live=s3g_thru_json::parse(query("source_motion_state"));assert(live.at("active").number()==1&&live.at("clip").number()==t->activeClip);setParam(t.get(),"live_source","0");assert(s3g_thru_json::parse(query("source_motion_state")).at("active").number()==0);
 std::snprintf(t->composerPanelOwner.data(),65,"test");t->composerLease.store(ThruInstance::composerClock()+1000000);int serial=0;
 const auto send=[&](const std::string& text,bool expect=true){const int id=++serial;const auto packet="test|"+std::to_string(id)+"|"+text;setParam(t.get(),"composer_edit",packet.c_str());const auto receipt=t->composerEditReceipts[id%64].load();assert(receipt==std::uint64_t(id*2+int(expect)));return id;};
 t->clearClipSlot(0);send("loop|0|0|2000|1");auto* c=t->activeClipSlot();c->takeCount=2;
 send("point|0|0|0|0|2|255|0|0|1|0");send("point|0|0|0|0|2|255|1|300|2|0");std::uint32_t id=0;for(int i=0;i<c->clipPointCount;++i)if(ThruInstance::clipPointTake(c->clipPoints[i])==1)id=c->clipPoints[i].editorId;assert(id);
 c->touchCount=1;c->touchSpans[0]={std::uint16_t(LockParameter::speed),1,200,400};ThruInstance::rebuildTouchLanes(*c);
 const auto revision=t->persistentRevision;const auto priorUndo=t->undoAvailable;
 send("point|0|0|"+std::to_string(id)+"|0|2|255|1|900|2|0",false);assert(t->persistentRevision==revision&&t->undoAvailable==priorUndo);float value=0;assert(t->sampleClipCurve(*c,std::size_t(LockParameter::speed),300,true,value)&&value==2);
 send("point|0|0|"+std::to_string(id)+"|0|2|255|1|400|3|0");assert(t->sampleClipCurve(*c,std::size_t(LockParameter::speed),400,true,value)&&value==3);
 c->touchSpans[0].startMs=1800;c->touchSpans[0].durationMs=400;ClipPoint wrapped;wrapped.timeMs=100;wrapped.parameter=ThruInstance::clipPointDescriptor(2,1);assert(ThruInstance::composerPointInTake(*c,wrapped));
 // A refused source edit publishes its actual error alongside a failed receipt.
 const auto* source=midiParameterForKey("live_source");assert(source);setParam(t.get(),"algorithm","2");
 const int rejected=send("p|0|0|"+std::to_string(source->id)+"|2",false);assert(t->controls.sourceMotion.v[8]==0);
 std::string reason;for(const auto& ch:t->composerEditErrors[rejected%64]){const char c=ch.load();if(!c)break;reason+=c;}assert(reason=="Choose input or an available part without feedback");
 send("p|0|0|"+std::to_string(source->id)+"|1");assert(t->controls.sourceMotion.v[8]==1&&t->lastError[0]==0);
 t->composerInitWorker();t->composerStatus("ready",true,"test",false);const auto status=s3g_thru_pack::readFile(t->composerRoot+"/status.json",1024*1024);const auto published=s3g_thru_json::parse(std::string(status.begin(),status.end()));assert(!published.at("editErrors").arrayValue().empty());
 std::cout<<"Release stability: owned Sequence/source frames, inactive File reset, single-point boundaries/Undo, wrapped intervals and rejected browser edit receipts: PASS\n";
 t.reset();std::filesystem::remove_all(folder);
}
