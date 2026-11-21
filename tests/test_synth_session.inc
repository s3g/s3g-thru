static std::atomic<bool> synthSessionWrongThread{false};
static pthread_t synthSessionAudioThread{};
void testSynthSessionStore(){
 using namespace s3g_thru_synth;
 Session s;s.type=dust;
 for(int type=0;type<generatorCount;++type){auto&d=s.drafts[type];initializeGenerator(d.recipe,type);d.used=true;std::snprintf(d.editor.name.data(),d.editor.name.size(),"Draft-%d",type);}
 s.drafts[partialsType].editor.row=1;s.drafts[partialsType].editor.low=-31;s.drafts[partialsType].editor.high=31;
 s.drafts[dust].editor.page=6;s.drafts[dust].editor.amount=.37f;
 for(int i=0;i<7;++i){std::snprintf(s.sources[i].path.data(),s.sources[i].path.size(),"/tmp/source-%d.wav",i);std::snprintf(s.sources[i].label.data(),s.sources[i].label.size(),"Source %d",i);s.sources[i].embedded=i%2;}
 const auto packed=encodeSession(s);Session copy;assert(decodeSession(packed,copy)&&encodeSession(copy)==packed);
 auto bad=packed;bad[20]^=1;assert(!decodeSession(bad,copy));
 for(std::size_t size:{0u,10u,100u})assert(!decodeSession(std::vector<std::uint8_t>(packed.begin(),packed.begin()+size),copy));
 char directory[]="/tmp/thru-synth-session-store.XXXXXX";assert(mkdtemp(directory));const std::string file=std::string(directory)+"/session.bin";
 synthSessionAudioThread=pthread_self();synthSessionWrongThread.store(false);
 s3g_thru_storage::testHook=+[](s3g_thru_storage::Operation,const char*,const char*){if(pthread_equal(pthread_self(),synthSessionAudioThread))synthSessionWrongThread.store(true);return true;};
 {
  SessionStore store;assert(store.prepare(file.c_str()));
  for(int i=0;i<100;++i){s.drafts[dust].recipe.seed=unsigned(i);assert(store.post(s));}
  store.stop();assert(store.idle()&&store.saved.load()>0);
 }
 s3g_thru_storage::testHook=nullptr;assert(!synthSessionWrongThread.load());
 assert(SessionStore::load(file.c_str(),copy)==SessionStore::loaded&&copy.drafts[dust].recipe.seed==99);
 const auto retained=encodeSession(copy);
 s3g_thru_storage::testHook=+[](s3g_thru_storage::Operation op,const char*,const char*){return op!=s3g_thru_storage::Operation::fileSync;};
 {SessionStore store;assert(store.prepare(file.c_str()));s.drafts[dust].recipe.seed=123;assert(store.post(s));store.stop();assert(store.failed.load());}
 s3g_thru_storage::testHook=nullptr;assert(SessionStore::load(file.c_str(),copy)==SessionStore::loaded&&encodeSession(copy)==retained);
 {SessionStore store;assert(store.prepare(file.c_str()));s.drafts[dust].recipe.seed=124;assert(store.post(s));store.stop();}
 {std::ofstream corrupt(file,std::ios::binary|std::ios::trunc);corrupt<<"broken";}
 assert(SessionStore::load(file.c_str(),copy)==SessionStore::recovered&&encodeSession(copy)==retained);
 std::filesystem::remove_all(directory);
 std::puts("Synth draft store: all fifteen recipes, editor/source metadata, coalesced newest snapshot, non-audio-thread durable I/O, failed-save preservation, checksum/truncation rejection and backup recovery: PASS");
}
void testSynthManualSession(){
 using namespace s3g_thru_synth;using W=Worker;
 char directory[]="/tmp/thru-synth-manual.XXXXXX";assert(mkdtemp(directory));
 auto t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
 const auto pump=[&](int blocks=200){for(int i=0;i<blocks;++i){renderSplit(*t,1);usleep(500);}};
 setParam(t.get(),"synth_open","1");pump();assert(!t->synthReady()&&!t->synthWantsRender&&t->synthActive<0&&t->synthWorker->state.load()==W::idle);
 setParam(t.get(),"synth_preview","1");assert(!t->synthPreviewWanted&&!t->synthWantsRender);
 for(int type=0;type<generatorCount;++type){setParam(t.get(),"synth_type",std::to_string(type).c_str());setParam(t.get(),"synth_edit","1,55");pump(2);assert(!t->synthWantsRender&&t->synthWorker->state.load()==W::idle);}
 const std::string source=std::string(directory)+"/source.wav";std::vector<float> samples(2000);for(int i=0;i<2000;++i)samples[i]=std::sin(i*.13f)*.2f;
 assert(ThruInstance::writeStereoWavFile(samples.data(),samples.data(),samples.size(),44100,source.c_str(),(source+".tmp").c_str(),true));
 setParam(t.get(),"synth_type","7");setParam(t.get(),"synth_primary_file",source.c_str());setParam(t.get(),"synth_impulse_file",source.c_str());
 setParam(t.get(),"synth_type","11");setParam(t.get(),"synth_hybrid0_file",source.c_str());setParam(t.get(),"synth_hybrid1_file",source.c_str());
 std::snprintf(t->currentLaneSourcePaths[t->selectedLane].data(),kPersistencePathSize,"%s",source.c_str());setParam(t.get(),"synth_type","3");setParam(t.get(),"synth_source","1");
 setParam(t.get(),"synth_type","4");setParam(t.get(),"synth_select","1,0");setParam(t.get(),"synth_editor","4,3,0,3,4,0.5,-31,31,1");
 setParam(t.get(),"synth_type","8");setParam(t.get(),"synth_drum","0,9");setParam(t.get(),"synth_drum","1,50.5");setParam(t.get(),"synth_name","Concert draft");setParam(t.get(),"synth_editor","8,14,14,2,4,0.8,-2,2,1");
 setParam(t.get(),"synth_type","12");setParam(t.get(),"synth_edit","0,0.18");setParam(t.get(),"synth_edit","1,50");setParam(t.get(),"synth_dust","1,37");setParam(t.get(),"synth_name","Keep Dust");
 setParam(t.get(),"synth_editor","12,6,6,2,4,0.37,-1,1,0");
 setParam(t.get(),"synth_render","1");const auto requested=t->synthQueuedRender.recipe;
 setParam(t.get(),"synth_edit","1,51");setParam(t.get(),"synth_dust","1,18");
 for(int i=0;i<10000&&(t->synthWantsRender||t->synthWorker->state.load()!=W::idle||t->synthActive<0);++i){renderSplit(*t,1);usleep(500);}
 assert(t->synthActive>=0&&!t->synthReady()&&t->synthWorker->audio[t->synthActive].recipe.v[root]==50&&t->synthWorker->audio[t->synthActive].recipe.dustParams[uRate]==37);
 assert(encode(t->synthWorker->audio[t->synthActive].recipe)==encode(requested));
 const auto old=t->synthWorker->audio[t->synthActive].left;const auto active=t->synthActive;
 setParam(t.get(),"synth_preview","1");pump(10);assert(t->synthPreviewWanted);
 setParam(t.get(),"synth_dust","2,3");pump();assert(t->synthActive==active&&t->synthWorker->audio[active].left==old&&!t->synthWantsRender);
 setParam(t.get(),"synth_render","1");waitSynth(*t);assert(t->synthReady()&&t->synthWorker->audio[t->synthActive].recipe.v[root]==51);
 setParam(t.get(),"synth_edit","1,63");setParam(t.get(),"synth_dust","1,42");
 const auto draft=encode(t->synthRecipe);setParam(t.get(),"synth_close","1");t.reset();
 t=std::make_unique<ThruInstance>();assert(t->prepare(directory));t->stopBackgroundWork();
 setParam(t.get(),"synth_open","1");assert(t->synthRecipe.type==dust&&encode(t->synthRecipe)==draft&&!t->synthWantsRender&&!t->synthReady()&&std::string(t->synthName.data())=="Keep Dust");
 std::array<char,256> editor{};assert(getParamValue(t.get(),"synth_editor",editor.data(),editor.size())>0&&std::string(editor.data()).find("12,6,6,2,4,0.37,-1,1,0")==0);
 assert(t->synthSourcePath.data()==source&&t->synthConvPaths[0].data()==source&&t->synthConvPaths[1].data()==source&&t->synthHybridPaths[0].data()==source&&t->synthHybridPaths[1].data()==source);
 setParam(t.get(),"synth_type","4");assert(t->synthRow==1);assert(getParamValue(t.get(),"synth_editor",editor.data(),editor.size())>0&&std::string(editor.data()).find("4,3,0,3,4,0.5,-31,31,1")==0);
 setParam(t.get(),"synth_type","8");assert(t->synthRecipe.drums[dModel]==9&&t->synthRecipe.drums[dTune]==50.5f&&std::string(t->synthName.data())=="Concert draft");
 assert(getParamValue(t.get(),"synth_editor",editor.data(),editor.size())>0&&std::string(editor.data()).find("8,14,14,2,4,0.8")==0);
 setParam(t.get(),"synth_type","12");assert(encode(t->synthRecipe)==draft);pump();assert(!t->synthWantsRender&&t->synthActive<0);
 t.reset();std::filesystem::remove_all(directory);
 std::puts("Synth manual session: no render on open/type/pitch/parameter/source edits, immutable button snapshot, uninterrupted last audio, explicit rerender, close/restart restores unrendered drafts, per-type settings/names/pages and all source kinds: PASS");
}
