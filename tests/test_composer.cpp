#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <thread>
#include <tuple>
using J = s3g_thru_json::Value;
static void drive(ThruInstance &t) {
  t.serviceComposer();
  t.processComposerWorker();
  std::array<int16_t, 256> output{};
  t.render(output.data(), 128);
  std::this_thread::sleep_for(std::chrono::microseconds(50));
}
static J command(ThruInstance &t, const std::string &request,
                 const std::string &op, const std::string &file = "") {
  const auto text = "test|" + request + "|" + op + "|" + file + "|" +
                    std::to_string(t.persistentRevision);
  assert(t.setComposerParam("composer_command", text.c_str()));
  for (int i = 0; i < 6000; ++i) {
    drive(t);
    const auto phase = t.composerPhase.load();
    if (phase == ThruInstance::composerIdle ||
        phase == ThruInstance::composerActive) {
      const auto path = t.composerRoot + "/replies/test-" + request + ".json";
      if (std::filesystem::exists(path)) {
        const auto reply =
            s3g_thru_json::parse(s3g_thru_pack::readFile(path, 65536));
        if (!reply.at("ok").boolean())
          std::cerr << reply.at("message").string() << "\n";
        return reply;
      }
    }
  }
  std::cerr << "Timeout phase " << t.composerPhase.load() << " prep "
            << t.clipPrepareState.load() << " queued " << t.queuedClip
            << " dirty " << t.persistentStateDirty << " error "
            << t.lastError.data() << "\n";
  std::abort();
}
int main() {
  {
    auto array = J::array();
    array.push("name");
    array.push(std::string("other"));
    array.push(true);
    array.push(2);
    array.push(1.25);
    array.push(J());
    assert(s3g_thru_json::stringify(array) ==
           "[\"name\",\"other\",true,2,1.25,null]");
  }
  char folder[] = "/tmp/thru-composer-test.XXXXXX";
  assert(mkdtemp(folder));
  const std::string root = folder;
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  const auto originalAudio = root + "/thru-lane-1.wav";
  std::array<float, 128> left{}, right{};
  for (int n = 0; n < 128; ++n) {
    left[n] = .25f * std::sin(n * .1f);
    right[n] = left[n] * .7f;
  }
  assert(ThruInstance::writeStereoWavFile(
      left.data(), right.data(), 128, 44100, originalAudio.c_str(),
      (originalAudio + ".tmp").c_str(), true));
  const auto bankAudio = root + "/captures/bank.wav";
  std::filesystem::copy_file(originalAudio, bankAudio);
  t->currentLaneSourceKinds[0] = LaneSourceKind::capture;
  t->clipSlots[0].laneSourceKinds[0] = LaneSourceKind::capture;
  std::snprintf(t->clipSlots[0].bankSlots[5].path.data(), kPersistencePathSize,
                "%s", bankAudio.c_str());
  std::snprintf(t->clipSlots[0].bankSlots[5].name.data(), 41,
                "Hidden Bank Sample");
  const auto originalBytes = s3g_thru_pack::readFile(originalAudio, 100000);
  t->setPerformanceMode(6);
  setParam(t.get(), "algorithm", "17");
  setParam(t.get(), "seq_enabled", "1");
  setParam(t.get(), "seq_hold", "0");
  setParam(t.get(), "seq_on", "1");
  setParam(t.get(), "seq_hold", "-1");
  t->sequenceLive.tracks[0].trigs[0].enabled = true;
  std::snprintf(t->clipSlots[0].name.data(), 41, "Original");
  t->captureActiveClip(true);
  std::snprintf(t->clipSlots[0].bankSlots[5].path.data(), kPersistencePathSize,
                "%s", bankAudio.c_str());
  std::snprintf(t->clipSlots[0].bankSlots[5].name.data(), 41,
                "Hidden Bank Sample");
  t->bankSlots = t->clipSlots[0].bankSlots;
  assert(t->writeMetadataFiles());
  t->persistentStateDirty = false;
  assert(command(*t, "hello", "hello").at("ok").boolean());
  // Direct panel edits use the live project, not Preview ownership or audio transfers.
  {
    const auto start=t->persistentRevision;const auto beat=t->sequenceBeat;
    const auto send=[&](int serial,const char* payload){const auto value="test|"+std::to_string(serial)+"|"+payload;setParam(t.get(),"composer_edit",value.c_str());return t->composerEditReceipts[serial%64].load();};
    const auto* speed=midiParameterForKey("speed");assert(speed);
    assert(send(1,("p|0|2|"+std::to_string(speed->id)+"|0.75").c_str())==3);
    assert(t->splitFocus==0&&t->liveControlsForPart(2).speed==.75f);
    assert(!t->composerOwned.load()&&t->persistentRevision>start&&t->sequenceBeat==beat);
    assert(send(2,("p|1|2|"+std::to_string(speed->id)+"|0.5").c_str())==4); // stale clip rejected
    assert(t->liveControlsForPart(2).speed==.75f);
    assert(send(3,"t|0|0|3|1|0.01|0.7|1|2|3|0.75|7|100|1|400|2|0|0.5")==7);
    const auto& trig=t->sequenceLive.tracks[0].trigs[3];
    assert(trig.enabled&&trig.count==2&&trig.locks.values[0]==.75f&&trig.locks.values[1]==7&&trig.locks.timeLocked&&trig.locks.timeMs==400);
    assert(t->sequencePlayback.tracks[0].trigs[3].enabled&&t->clipSlots[0].sequence.tracks[0].trigs[3].enabled);
    assert(send(3,"t|0|0|3|0|0.01|0.7|1|2|3|0.75|7|100|1|400|2|0|0.5")==7); // duplicate cannot toggle twice
    assert(trig.enabled);
    assert(send(4,"l|0|0|12|6|0|0|0|1|100|1|300|0.25|60|0")==9);
    assert(t->sequenceLive.tracks[0].length==12&&t->sequenceRequestedRate[0].load()==6&&t->controls.locus==.25f);
    assert(send(5,"l|0|0|0|6|0|0|0|1|100|1|300|0.25|60|0")==10);
    assert(send(6,"play|0|0|1")==13&&t->clipPlaying);
    assert(send(7,"play|0|0|0")==15&&!t->clipPlaying);
    assert(!t->composerOwned.load());
    assert(send(8,"f|0|2|0")==17&&t->splitFocus==2);
    assert(send(9,"root|0|2|65")==19&&t->rootNote==65);
    const auto* length=midiParameterForKey("seq_length");assert(length);
    assert(send(10,("p|0|2|"+std::to_string(length->id)+"|15").c_str())==21&&t->sequenceLive.tracks[2].length==15);
    assert(send(11,"root|0|2|128")==22&&t->rootNote==65);
    assert(send(12,"f|0|0|0")==25&&t->splitFocus==0);
    for(int n=0;n<10;++n)drive(*t);
    assert(!t->composerPanelPath.empty());
    const auto frame=s3g_thru_json::parse(s3g_thru_pack::readFile(t->composerPanelPath,4*1024*1024));
    s3g_thru_pack::validate(frame.at("document"));
    assert(frame.at("editSequence").number()==12&&frame.at("focus").number()==0);
    assert(frame.at("activeClip").number()==0&&frame.at("client").string()=="test");
    const auto reconnect=std::string("next-browser|connect|hello||")+std::to_string(t->persistentRevision);
    t->setComposerParam("composer_command",reconnect.c_str());
    t->composerPanelAt=0;
    for(int n=0;n<12;++n)drive(*t);
    const auto refreshed=s3g_thru_json::parse(s3g_thru_pack::readFile(t->composerPanelPath,4*1024*1024));
    assert(refreshed.at("client").string()=="next-browser");
    assert(command(*t,"hello-again","hello").at("ok").boolean());

    t->sequenceLive.tracks[0].trigs[3]={};t->sequenceLive.tracks[0].length=16;t->sequenceLive.tracks[0].rate=4;t->sequenceLive.tracks[0].timeMs=1000;t->controls.locus=.5f;
    t->splitParts[2].controls.speed=1;t->sequencePlayback=t->sequenceLive;t->publishSequenceRates();t->captureActiveClip(true);
    assert(t->writeMetadataFiles());t->persistentStateDirty=false;
  }
  {
    const auto directory=root+"/patch-scope";std::filesystem::create_directory(directory);
    auto single=std::make_unique<ThruInstance>();assert(single->prepare(directory.c_str()));single->stopBackgroundWork();
    setParam(single.get(),"algorithm","17");single->selectedLane=2;single->controls.fm.patches[2].v[s3g_thru_fm::tune]=12;
    single->captureActiveClip(true);assert(single->writeMetadataFiles());single->persistentStateDirty=false;
    assert(command(*single,"scope-hello","hello").at("ok").boolean());
    for(int n=0;n<10;++n)drive(*single);
    auto frame=s3g_thru_json::parse(s3g_thru_pack::readFile(single->composerPanelPath,4*1024*1024));
    assert(frame.at("patches").arrayValue()[0].number()==2);
    assert(frame.at("document").at("clips").arrayValue()[0].at("parts").arrayValue()[0].at("parameters").at("fm_tune").number()==12);
    setParam(single.get(),"composer_edit","test|1|p|0|0|4401|24|2");
    assert(single->composerEditReceipts[1].load()==3&&single->selectedLane==2&&single->controls.fm.patches[2].v[s3g_thru_fm::tune]==24&&single->controls.fm.patches[0].v[s3g_thru_fm::tune]==0);
    setParam(single.get(),"composer_edit","test|2|p|0|0|4401|10|1");assert(single->composerEditReceipts[2].load()==4&&single->controls.fm.patches[2].v[s3g_thru_fm::tune]==24);
    single->engine.adoptLane(2,128,44100);single->waveMinimum[2].fill(-3);single->waveMaximum[2].fill(4);
    std::snprintf(single->currentLaneSourcePaths[2].data(),kPersistencePathSize,"%s/not-opened.wav",directory.c_str());
    single->composerPanelJob.store(1);single->captureComposerPanel();single->publishComposerPanel();
    const auto cached=s3g_thru_json::parse(s3g_thru_pack::readFile(single->composerPanelPath,4*1024*1024));
    s3g_thru_pack::validate(cached.at("document"));
    if(const char* fixture=std::getenv("THRU_COMPOSER_FIXTURE"))s3g_thru_pack::atomicFile(std::string(fixture)+".panel.json",s3g_thru_json::stringify(cached));
    const auto& asset=cached.at("document").at("assets").arrayValue()[0];
    assert(asset.at("frames").number()==128&&asset.at("waveform").arrayValue().size()==224);
    assert(std::fabs(asset.at("waveform").arrayValue()[0].number()+3.0/7)<.00001);
    single->currentLaneSourcePaths[2][0]=0;single->engine.clear();
    single.reset();
  }
  // Browser step edits borrow the native authoring path without leaving Move
  // on a different held step, child, part or recording lane.
  {
    const auto directory=root+"/browser-steps";std::filesystem::create_directory(directory);
    auto b=std::make_unique<ThruInstance>();assert(b->prepare(directory.c_str()));b->stopBackgroundWork();
    b->setPerformanceMode(6);b->selectSplitPart(2);setParam(b.get(),"algorithm","17");b->selectSplitPart(0);
    setParam(b.get(),"seq_step","8");setParam(b.get(),"seq_burst","2");setParam(b.get(),"seq_child","1");setParam(b.get(),"seq_hold","7");
    b->selectedLane=1;b->captureActiveClip(true);assert(b->writeMetadataFiles());b->persistentStateDirty=false;
    assert(command(*b,"steps-hello","hello").at("ok").boolean());int serial=0;
    const auto context=[&](){return std::make_tuple(b->splitFocus,b->sequenceStep,b->sequenceChild,b->sequenceEditStep,b->sequenceSoundEditPart,b->sequenceSoundEditClip,b->selectedLane,b->sequenceSteps,b->sequenceChildren);};
    const auto send=[&](const std::string& payload,bool ok=true){const auto before=context();const int id=++serial;setParam(b.get(),"composer_edit",("test|"+std::to_string(id)+"|"+payload).c_str());assert(b->composerEditReceipts[id%64].load()==std::uint64_t(id*2+(ok?1:0)));assert(context()==before);};
    const auto base=b->liveControlsForPart(2).fm.patches[2].v[s3g_thru_fm::tune];
    send("q|0|2|5|0|0|1|0");assert(b->sequenceLive.tracks[2].trigs[5].enabled);
    send("s|0|2|5|4401|12|0|0");assert(b->sequenceLive.soundLocks.find(2,5,0,4401)->value==12);
    assert(b->liveControlsForPart(2).fm.patches[2].v[s3g_thru_fm::tune]==base);
    send("q|0|2|5|0|1|0.75|0");assert(b->sequenceLive.tracks[2].trigs[5].locks.values[0]==.75f);
    send("q|0|2|5|0|4|2500|0");assert(b->sequenceLive.tracks[2].trigs[5].locks.timeLocked&&b->sequenceLive.tracks[2].trigs[5].locks.timeMs==2500);
    send("q|0|2|5|0|10|3|0");assert(b->sequenceLive.tracks[2].trigs[5].count==3);
    send("s|0|2|5|4401|-7|0|2");assert(b->sequenceLive.soundLocks.resolve(2,5,2).find(4401)->value==-7);
    send("q|0|2|5|2|3|40|0");assert(b->sequenceLive.tracks[2].trigs[5].events[1].locks.values[2]==40);
    send("q|0|2|5|2|12|0|0");assert(!b->sequenceLive.soundLocks.find(2,5,2,4401)&&b->sequenceLive.soundLocks.resolve(2,5,2).find(4401)->value==12);
    assert(b->sequenceLive.tracks[2].trigs[5].events[1].locks.mask==0&&b->sequenceLive.tracks[2].trigs[5].count==3);
    send("s|0|2|5|4401|1|0|1e30",false);send("q|0|1e30|5|0|0|1|0",false);
    send("s|0|2|5|4|17|0|0",false); // Engine choice remains lane-level.
    send("q|0|2|31|0|0|1|0",false);send("q|1|2|5|0|0|1|0",false);send("s|0|2|5|4401|2|0|8",false);
    send("q|0|2|5|0|5|1|0",false); // TIME lane rejects a stale STEP duration editor.
    send("q|0|2|5|2|11|0|0"); // Ordered event starts remain a valid portable trig.
    send("q|0|2|5|1|11|0.9|0",false);
    send("s|0|2|5|4401|0|1|0");assert(!b->sequenceLive.soundLocks.find(2,5,0,4401));
    send("s|0|2|5|4401|9|0|0");send("q|0|2|5|0|0|0|0");assert(!b->sequenceLive.tracks[2].trigs[5].enabled&&b->sequenceLive.soundLocks.find(2,5,0,4401));
    // A regular base edit must not become a lock on Move's concurrently held pad.
    send("p|0|0|2|0.8|0");assert(b->controls.speed==.8f&&!b->sequenceLive.soundLocks.find(0,7,1,2));
    b->setClipPlaying(true);const auto beat=b->sequenceBeat;
    send("q|0|2|5|0|0|1|0");send("q|0|2|5|0|7|0.6|0");assert(b->sequenceBeat==beat&&b->sequencePlayback.tracks[2].trigs[5].probability==.6f);
    b->setClipPlaying(false);b->stopSequenceAudition();setParam(b.get(),"seq_hold","-1");
    b->selectSplitPart(2);setParam(b.get(),"seq_type","1");b->selectSplitPart(0);
    send("q|0|2|5|0|1|0.42|0");assert(b->sequenceLive.tracks[2].generator.base[5].locks.values[0]==.42f);
    b->selectSplitPart(2);setParam(b.get(),"seq_recipe_hits","8");b->selectSplitPart(0);assert(b->sequenceLive.tracks[2].trigs[5].locks.values[0]==.42f);
    b->captureActiveClip(true);assert(b->writeMetadataFiles());b->persistentStateDirty=false;b.reset();
    auto reopened=std::make_unique<ThruInstance>();assert(reopened->prepare(directory.c_str()));reopened->stopBackgroundWork();
    assert(reopened->sequenceLive.tracks[2].trigs[5].locks.values[0]==.42f&&reopened->sequenceLive.soundLocks.find(2,5,0,4401)->value==9);reopened->persistentStateDirty=false;
    std::cout<<"Browser step locks: scoped native setters, bases/held context, Burst inheritance/clear, stale guards, live edits, recipe overrides and saved restart: PASS\n";
  }
  // Returning to Single intentionally keeps inactive layout preferences. Live
  // export and portable core overlays must still describe exactly one part.
  {
    const auto directory=root+"/single-layout";std::filesystem::create_directory(directory);
    auto b=std::make_unique<ThruInstance>();assert(b->prepare(directory.c_str()));b->stopBackgroundWork();
    b->composerPanelClip=std::make_unique<ClipSlot>();
    for(int prior:{1,2,3,4,6}){
      b->setPerformanceMode(prior);b->setPerformanceMode(0);assert(b->performanceMode()==0);
      b->captureActiveClip(true);assert(command(*b,"layout-"+std::to_string(prior),"hello").at("ok").boolean());
      b->composerPanelJob.store(1);b->captureComposerPanel();b->publishComposerPanel();
      auto f=s3g_thru_json::parse(s3g_thru_pack::readFile(b->composerPanelPath,4*1024*1024));s3g_thru_pack::validate(f.at("document"));
      const auto& c=f.at("document").at("clips").arrayValue()[0];assert(c.at("mode").string()=="single"&&c.at("parts").arrayValue().size()==1&&f.at("settings").arrayValue().size()==1&&f.at("patches").arrayValue().size()==1);
      const bool quad=b->clipSlots[0].quadMode,shared=b->clipSlots[0].sharedDual;
      auto copy=std::make_unique<ClipSlot>(b->clipSlots[0]);ThruInstance::composerReadClip(*copy,c,120);
      assert(!copy->splitEnabled&&copy->quadMode==quad&&copy->sharedDual==shared);
    }
    int serial=0;const auto edit=[&](const char* key,float value,int token,bool ok=true){const auto* p=midiParameterForKey(key);assert(p);const int id=++serial;const std::string text="test|"+std::to_string(id)+"|p|0|0|"+std::to_string(p->id)+"|"+std::to_string(value)+"|-1|"+std::to_string(token);setParam(b.get(),"composer_edit",text.c_str());assert(b->composerEditReceipts[id%64].load()==std::uint64_t(id*2+(ok?1:0)));};
    edit("algorithm",17,-1);edit("voice_engine_mode",1,1);assert(b->voiceEngineSettings.enabled);
    edit("voice_engine_2",15,17);edit("voice_engine_edit",2,17);assert(b->voiceEngineSettings.edit==1&&int(b->controls.algorithm)==15);
    edit("table_tune",24,17,false); // Delayed data from E1 cannot target E2.
    edit("voice_engine_pan",45,18);edit("voice_engine_octave",1,18);edit("voice_engine_assign",2,18);edit("voice_engine_depth",50,18);edit("pressure_mode",3,18);edit("polyphony_comp",0,18);
    b->composerPanelJob.store(1);b->captureComposerPanel();b->publishComposerPanel();
    auto f=s3g_thru_json::parse(s3g_thru_pack::readFile(b->composerPanelPath,4*1024*1024));const auto& settings=f.at("settings").arrayValue()[0];
    assert(settings.at("voice_engine_mode").number()==1&&settings.at("voice_engine_edit").number()==2&&settings.at("voice_engine_2").number()==15&&settings.at("voice_engine_1").number()==17);
    assert(settings.at("voice_engine_assign").number()==2&&settings.at("voice_engine_depth").number()==50&&settings.at("pressure_mode").number()==3&&settings.at("polyphony_comp").number()==0);
    assert(std::fabs(settings.at("voice_engine_pan").number()-45)<.00001&&settings.at("voice_engine_octave").number()==1);
    b->setPerformanceMode(3);edit("voice_engine_8",17,-1,false);b->persistentStateDirty=false;
    std::cout<<"Single live frames: dormant Split/Shared/Quad/Bank/Sequence flags, exact part counts and native preference preservation; LIST settings/readback and stale-entry rejection: PASS\n";
  }
  // Decimal endpoints of float metadata remain legal in portable JSON.
  for (const auto &p : s3g_thru_midi::parameters)
    if (s3g_thru_pack::soundParameter(p)) {
      auto values = J::object();
      values[p.key] = s3g_thru_pack::parameterBound(p.minimum);
      s3g_thru_pack::parameters(values, "minimum");
      values[p.key] = s3g_thru_pack::parameterBound(p.maximum);
      s3g_thru_pack::parameters(values, "maximum");
    }
  // Core conversions validate for every performance layout and synth patch
  // family.
  for (int mode = 0; mode < 7; ++mode) {
    auto c = std::make_unique<ClipSlot>();
    ThruInstance::composerDefaultClip(*c);
    c->splitEnabled = mode != 0 && mode != 5;
    c->quadMode = mode == 3 || mode == 6;
    c->sharedDual = mode == 2 || mode == 4;
    c->bankMode = mode == 4;
    c->layers.active = mode == 5;
    c->sequence.active = mode == 6;
    c->sequence.initialized = true;
    c->recordedTempoBpm = 91.0f;
    c->occupied = true;
    c->splitInitialized = c->splitEnabled;
    c->quadInitialized = c->quadMode;
    c->tablePrograms[0].patches[2].v[s3g_thru_table::tune] = 11;
    if (mode == 6) {
      c->clipPointCount = 2;
      c->takeCount = 1;
      c->clipPoints[0] = {0, 0, 4, 0, ClipPointType::sequenceLane};
      c->clipPoints[1] = {100, 100, 5, 0, ClipPointType::sequenceLane};
    }
    auto doc = ThruInstance::composerObject({{"format", "s3g-thrupack"},
                                             {"version", 1},
                                             {"title", "Test"},
                                             {"tempo", 120},
                                             {"assets", J::array()},
                                             {"clips", J::array()}});
    doc["clips"].push(ThruInstance::composerClip(*c, 0));
    try {
      s3g_thru_pack::validate(doc);
    } catch (const std::exception &e) {
      std::cerr << "Mode " << mode << " " << e.what() << "\n";
      throw;
    }
    auto next = std::make_unique<ClipSlot>(*c);
    ThruInstance::composerReadClip(*next, doc.at("clips").arrayValue()[0], 120);
    assert(next->tablePrograms[0].patches[2].v[s3g_thru_table::tune] == 11);
    assert(ThruInstance::composerMode(*next) == mode);
    assert(next->recordedTempoBpm == 91.0f);
    assert(c->splitEnabled == next->splitEnabled &&
           c->sharedDual == next->sharedDual);
    assert(c->splitInitialized == next->splitInitialized &&
           c->quadInitialized == next->quadInitialized);
  }
  {
    const auto revision = t->persistentRevision;
    const auto request =
        std::string("test|read-race|read|0|") + std::to_string(revision);
    t->setComposerParam("composer_command", request.c_str());
    t->processComposerWorker();
    for (int n = 0;
         n < 100 && t->composerPhase.load() != ThruInstance::composerCaptured;
         ++n)
      t->serviceComposer();
    assert(t->composerPhase.load() == ThruInstance::composerCaptured);
    t->markPersistentStateDirty();
    t->serviceComposer();
    t->processComposerWorker();
    const auto response = s3g_thru_json::parse(s3g_thru_pack::readFile(
        t->composerRoot + "/replies/test-read-race.json", 65536));
    assert(response.at("revision").string() == std::to_string(revision));
    assert(t->writeMetadataFiles());
    t->persistentStateDirty = false;
  }
  const auto reply = command(*t, "read", "read", "0");
  assert(reply.at("ok").boolean());
  s3g_thru_pack::Archive pack(reply.at("result").string());
  auto doc = pack.document;
  assert(doc.at("clips").arrayValue().size() == 1);
  assert(doc.at("assets").arrayValue().size() == 1);

  doc["clips"].arrayValue()[0]["name"] = "Edited";
  doc["clips"].arrayValue()[0]["parts"].arrayValue()[0]["engine"] = "fm";
  // The JS pack test repackages this fixture; retain it outside the module.
  const auto fixture = std::getenv("THRU_COMPOSER_FIXTURE");
  if (fixture) {
    std::filesystem::copy_file(
        reply.at("result").string(), fixture,
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::create_directories(std::string(fixture) + ".assets");
    for (const auto &a : doc.at("assets").arrayValue())
      std::filesystem::copy_file(
          t->composerRoot + "/cache/" + a.at("sha256").string() + ".wav",
          std::string(fixture) + ".assets/" + a.at("sha256").string() + ".wav",
          std::filesystem::copy_options::overwrite_existing);
  }
  const auto writeDraft = [&](const std::string &name) {
    const auto text = s3g_thru_json::stringify(doc);
    auto manifest = pack.manifest;
    manifest["sha256"] = s3g_thru_pack::sha256(text);
    const auto m = s3g_thru_json::stringify(manifest);
    mz_zip_archive zip{};
    s3g_thru_project::ZipBudget budget;
    s3g_thru_project::setupZip(zip, budget);
    assert(mz_zip_writer_init_file(
        &zip, (t->composerRoot + "/uploads/" + name + ".thrupack").c_str(), 0));
    assert(mz_zip_writer_add_mem(&zip, "composition.json", text.data(),
                                 text.size(), 0));
    assert(mz_zip_writer_add_mem(&zip, "manifest.json", m.data(), m.size(), 0));
    for (const auto &[path, index] : pack.entries)
      if (path != "composition.json" && path != "manifest.json") {
        const auto bytes = pack.extract(path);
        assert(mz_zip_writer_add_mem(&zip, path.c_str(), bytes.data(),
                                     bytes.size(), MZ_BEST_SPEED));
        (void)index;
      }
    assert(mz_zip_writer_finalize_archive(&zip));
    assert(mz_zip_writer_end(&zip));
    auto request = ThruInstance::composerObject(
        {{"pack", name + ".thrupack"},
         {"mapping", J::Array{ThruInstance::composerObject(
                         {{"clip", "clip-1"}, {"slot", 0}})}},
         {"replace", true},
         {"chains", false}});
    s3g_thru_pack::atomicFile(t->composerRoot + "/uploads/" + name + ".json",
                              s3g_thru_json::stringify(request));
  };
  writeDraft("draft");
  const auto revision = t->persistentRevision;
  const auto original = t->clipSlots[0].name;
  assert(command(*t, "preview", "preview", "draft.json").at("ok").boolean());
  assert(t->composerOwned.load() && t->clipPlaying &&
         std::string(t->clipSlots[0].name.data()) == "Edited" &&
         t->persistentRevision == revision);
  auto state = std::make_unique<std::array<char, kStateTextSize>>();
  assert(t->serializeState(state->data(), int(state->size())) > 0);
  assert(std::string(state->data()).find("Edited") == std::string::npos);
  setParam(t.get(), "algorithm", "19");
  assert(t->controls.algorithm == Engine::PlaybackAlgorithm::fm);
  assert(command(*t, "preview-repeat", "preview", "draft.json")
             .at("ok")
             .boolean());
  assert(command(*t, "cancel", "cancel").at("ok").boolean());
  assert(!t->composerOwned.load() && t->clipSlots[0].name == original &&
         !t->clipPlaying && t->persistentRevision == revision);
  assert(command(*t, "apply", "apply", "draft.json").at("ok").boolean());
  assert(t->composerUndoReady.load() && !t->composerOwned.load() &&
         std::string(t->clipSlots[0].name.data()) == "Edited");
  // A late/disconnect Cancel must never undo an already committed Apply.
  assert(command(*t, "late-cancel", "cancel").at("ok").boolean());
  assert(std::string(t->clipSlots[0].name.data()) == "Edited");
  // Keep the manual harness independent of the normal autosave worker.
  assert(t->writeMetadataFiles());
  t->persistentStateDirty = false;
  const std::uint8_t undoPress[]{0xb0, 56, 127};
  onMidi(t.get(), undoPress, 3, MOVE_MIDI_SOURCE_INTERNAL);
  // The Move UI routes its curved-arrow control to this contextual command.
  setParam(t.get(), "undo", "1");
  for (int n = 0;
       n < 3000 && (t->composerOwned.load() ||
                    t->composerPhase.load() != ThruInstance::composerIdle);
       ++n)
    drive(*t);
  assert(!t->composerOwned.load() &&
         t->composerPhase.load() == ThruInstance::composerIdle);
  assert(t->clipSlots[0].name == original);
  assert(t->writeMetadataFiles());
  t->persistentStateDirty = false;
  assert(
      command(*t, "lease-preview", "preview", "draft.json").at("ok").boolean());
  t->composerLease.store(1);
  for (int n = 0; n < 2000 && t->composerOwned.load(); ++n)
    drive(*t);
  for (int n = 0; n < 20; ++n)
    drive(*t);
  assert(!t->composerOwned.load() && t->clipSlots[0].name == original);
  {
    const auto example = std::filesystem::path(__FILE__).parent_path() /
                         "../docs/assets/formats/four-small-orbits.thrupack";
    std::filesystem::copy_file(
        example, t->composerRoot + "/uploads/orbits.thrupack",
        std::filesystem::copy_options::overwrite_existing);
    auto request = ThruInstance::composerObject(
        {{"pack", "orbits.thrupack"},
         {"mapping", J::Array{ThruInstance::composerObject(
                         {{"clip", "orbits"}, {"slot", 63}})}},
         {"replace", false},
         {"chains", false}});
    s3g_thru_pack::atomicFile(t->composerRoot + "/uploads/orbits.json",
                              s3g_thru_json::stringify(request));
    assert(command(*t, "ai-apply", "apply", "orbits.json").at("ok").boolean());
    assert(t->activeClip == 63 && t->sequenceMode() && t->tempoBpm == 96);
    t->setClipPlaying(true);
    long energy = 0;
    for (int i = 0; i < 150; ++i) {
      std::array<int16_t, 256> output{};
      t->render(output.data(), 128);
      for (auto x : output)
        energy += std::abs(int(x));
    }
    assert(energy > 10000);
    t->setClipPlaying(false);
    assert(t->writeMetadataFiles());
    t->persistentStateDirty = false;
    assert(command(*t, "ai-undo", "undo").at("ok").boolean());
    assert(t->activeClip == 0 && !t->clipSlots[63].occupied &&
           t->clipSlots[0].name == original);
    assert(t->writeMetadataFiles());
    t->persistentStateDirty = false;
  }
  assert(command(*t, "preview2", "preview", "draft.json").at("ok").boolean());
  assert(t->composerOwned.load());
  // Teardown must not write temporary audition audio over legacy backups.
  t->engine.adoptLane(0, 128, 44100);
  float *audio = const_cast<float *>(t->engine.laneData(0, 0));
  std::fill_n(audio, 128, .9f);
  assert(t->savePersistentState());
  assert(s3g_thru_pack::readFile(originalAudio, 100000) == originalBytes);
  assert(!t->composerOwned.load() && t->clipSlots[0].name == original);
  t.reset();
  auto restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(folder));
  restored->stopBackgroundWork();
  assert(restored->clipSlots[0].name == original);
  restored.reset();
  std::filesystem::remove_all(root);
  std::puts("Composer core, package, reversible Preview, state isolation, "
            "Apply/Undo and exit/restart: PASS");
}
