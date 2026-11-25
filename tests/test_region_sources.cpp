// Exercise authored-region selection through native controls, renderer and storage.
#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include "test_clip_format_helpers.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using Mode = Engine::RegionMode;
struct RegionFixture {
    static constexpr std::size_t frames = 4096;
    Engine engine;
    std::array<std::array<float,frames*2>,4> storage{};
    std::array<float,4> input{},output{};
    std::array<const float*,4> in{};
    std::array<float*,4> out{};
    RegionFixture() {
        for(int c=0;c<4;++c){in[c]=&input[c];out[c]=&output[c];}
        assert(engine.prepare(44100,storage[0].data(),storage[1].data(),storage[2].data(),storage[3].data(),frames*2));
        for(int c=0;c<2;++c)for(std::size_t i=0;i<frames;++i)
            engine.writableLane(0,c)[i]=.35f*std::sin(float(i)*(.02f+.1f*(i/1024)));
        assert(engine.adoptLane(0,frames,44100));
    }
    float tick(const Engine::Controls& c) {engine.process(in.data(),out.data(),1,c);assert(std::isfinite(output[0]));return output[0];}
};
static s3g_thru::SliceMap map(std::initializer_list<float> points) {
    s3g_thru::SliceMap m;m.count=points.size()-1;std::copy(points.begin(),points.end(),m.points.begin());assert(s3g_thru::validSliceMap(m));return m;
}
static std::vector<float> render(Engine::Controls c,const s3g_thru::SliceMap& m,bool authored) {
    auto f=std::make_unique<RegionFixture>();
    if(authored)c.eventRegions[0]={m.points.data(),m.count,true};
    else assert(f->engine.setLaneRegions(0,m.points.data(),m.count));
    std::vector<float> audio;audio.reserve(2048);
    for(int i=0;i<2048;++i)audio.push_back(f->tick(c));
    return audio;
}
int main() {
    const auto file=map({0,.13f,.7f,1}), edited=map({0,.4f,.92f,1});
    for(auto algorithm:{Engine::PlaybackAlgorithm::cutups,Engine::PlaybackAlgorithm::texture}) {
        Engine::Controls c;c.algorithm=algorithm;c.locus=.5f;c.field=1;c.join=.1f;c.manualLane=0;c.laneCycles=.125f;
        c.cutups=Engine::CutupsMode::reverse;c.cutRateHz=.25f;c.cutClock=Engine::CutClock::free;c.cutRegionMode=Mode::file;
        c.textureMode=2;c.textureRate=.25f;c.textureLengthMs=2000;c.textureRange=0;c.textureRegionMode=Mode::file;c.noteStartOffset=.8f;
        const auto marked=render(c,file,true);
        c.cutRegionMode=c.textureRegionMode=Mode::transients;
        const auto detected=render(c,file,false);
        float peak=0,difference=0;
        for(std::size_t i=0;i<marked.size();++i){peak=std::max(peak,std::abs(marked[i]));difference=std::max(difference,std::abs(marked[i]-detected[i]));}
        assert(peak>.01f&&difference<.00001f);
        c.cutRegionMode=c.textureRegionMode=Mode::edited;
        const auto alternate=render(c,edited,true);difference=0;
        for(std::size_t i=0;i<marked.size();++i)difference+=std::abs(marked[i]-alternate[i]);
        assert(difference>1.f);
    }
    // Authored markers remain in file coordinates under a narrow FIELD.
    auto f=std::make_unique<RegionFixture>();Engine::Controls c;c.algorithm=Engine::PlaybackAlgorithm::cutups;
    c.cutups=Engine::CutupsMode::reverse;c.cutClock=Engine::CutClock::free;c.cutRateHz=.25f;
    c.locus=.8f;c.field=.2f;c.cutRegionMode=Mode::file;c.eventRegions[0]={file.points.data(),file.count,true};
    for(int i=0;i<400;++i)f->tick(c);
    assert(f->engine.normalizedPosition()>.7f&&f->engine.normalizedPosition()<.9f);
    // Resolve sources independently of Slices' SSRC, per clip and lane.
    char folder[]="/tmp/thru-region-sources.XXXXXX";assert(mkdtemp(folder));
    auto t=std::make_unique<ThruInstance>();assert(t->prepare(folder));t->stopBackgroundWork();
    assert(t->engine.adoptLane(0,4096,44100));
    t->fileSliceMaps[0]=file;t->activeClipSlot()->sliceLanes[0].edited=edited;
    c.algorithm=Engine::PlaybackAlgorithm::cutups;c.cutRegionMode=Mode::file;
    t->prepareEventRegionViews(c);assert(c.eventRegions[0].points==t->fileSliceMaps[0].points.data());
    c.cutRegionMode=Mode::edited;t->prepareEventRegionViews(c);assert(c.eventRegions[0].points==t->activeClipSlot()->sliceLanes[0].edited.points.data());
    c.cutRegionMode=Mode::automatic;t->prepareEventRegionViews(c);assert(c.eventRegions[0].points==t->activeClipSlot()->sliceLanes[0].edited.points.data());
    t->activeClipSlot()->sliceLanes[0].edited={};t->prepareEventRegionViews(c);assert(c.eventRegions[0].points==t->fileSliceMaps[0].points.data());
    t->fileSliceMaps[0]={};t->prepareEventRegionViews(c);assert(!c.eventRegions[0].count);
    c.algorithm=Engine::PlaybackAlgorithm::texture;c.textureRegionMode=Mode::file;t->fileSliceMaps[0]=file;
    t->prepareEventRegionViews(c);assert(c.eventRegions[0].count==file.count);
    c.textureRegionMode=Mode::transients;t->prepareEventRegionViews(c);assert(!c.eventRegions[0].count);
    setParam(t.get(),"texture_region_mode","Edited");setParam(t.get(),"texture_regions","7");setParam(t.get(),"cut_region_mode","Auto");
    assert(t->controls.textureRegionMode==Mode::edited&&t->controls.textureRegionCount==7&&t->controls.cutRegionMode==Mode::automatic);
    applyNrpn(*t,5,1672,8192,MOVE_MIDI_SOURCE_EXTERNAL);assert(t->controls.textureRegionMode==Mode::file);
    applyNrpn(*t,5,1673,16383,MOVE_MIDI_SOURCE_EXTERNAL);assert(t->controls.textureRegionCount==64);
    const auto* descriptor=s3g_thru_sequence::soundDescriptor(1672);assert(descriptor&&descriptor->target==int(LockParameter::textureRegionMode));
    assert(t->savePersistentState());
    const std::string path=std::string(folder)+"/thru-clips.bin";
    std::ifstream input(path,std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});assert(!bytes.empty());
    auto reloaded=std::make_unique<ThruInstance>();assert(reloaded->prepare(folder));reloaded->stopBackgroundWork();
    assert(reloaded->controls.textureRegionMode==Mode::file&&reloaded->controls.textureRegionCount==64&&reloaded->controls.cutRegionMode==Mode::automatic);
    // A pre-feature bank retains its values and gets the old Texture defaults.
    bytes.resize(bytes.size()-4);stripEventRegions(bytes);
    std::uint32_t checksum=2166136261u;for(auto x:bytes)checksum=(checksum^x)*16777619u;
    for(int i=0;i<4;++i)bytes.push_back(static_cast<unsigned char>(checksum>>(8*i)));
    const auto legacy=std::string(folder)+"/legacy.bin";std::ofstream output(legacy,std::ios::binary);output.write(reinterpret_cast<char*>(bytes.data()),bytes.size());output.close();
    assert(reloaded->readClipFileFromPath(legacy.c_str()));
    assert(reloaded->clipSlots[0].baseValues[std::size_t(LockParameter::textureRegionMode)]==1);
    assert(reloaded->clipSlots[0].baseValues[std::size_t(LockParameter::textureRegionCount)]==16);
    // Both parts retain independent source choices, without moving edit focus.
    reloaded->setPerformanceMode(2);reloaded->selectSplitPart(0,false);
    applyNrpn(*reloaded,5,1672,8192,MOVE_MIDI_SOURCE_EXTERNAL);
    applyNrpn(*reloaded,6,1672,16383,MOVE_MIDI_SOURCE_EXTERNAL);
    assert(reloaded->splitFocus==0&&reloaded->liveControlsForPart(0).textureRegionMode==Mode::file
        &&reloaded->liveControlsForPart(1).textureRegionMode==Mode::automatic);
    reloaded->beginClipRecording();assert(reloaded->clipRecording);
    applyNrpn(*reloaded,5,1672,12287,MOVE_MIDI_SOURCE_EXTERNAL);
    std::array<int16_t,256> block{};for(int i=0;i<100;++i)reloaded->render(block.data(),128);
    reloaded->finishClipRecording();bool captured=false;
    const auto* clip=reloaded->activeClipSlot();for(std::size_t i=0;i<clip->clipPointCount;++i)
        if((clip->clipPoints[i].parameter & 0xffu)==std::size_t(LockParameter::textureRegionMode))captured=true;
    assert(captured);
    reloaded->setPerformanceMode(6);reloaded->selectSplitPart(0,false);reloaded->sequenceChild=0;
    setParam(reloaded.get(),"seq_hold","0");assert(reloaded->sequenceSoundEditing());
    setParam(reloaded.get(),"texture_region_mode","2");setParam(reloaded.get(),"texture_regions","9");
    const auto sound=reloaded->sequenceLive.soundLocks.resolve(0,0,0);
    assert(sound.find(1672)&&sound.find(1673));Engine::Controls locked;
    reloaded->applySequenceVoiceSound(locked,sound,0);
    assert(locked.textureRegionMode==Mode::file&&locked.textureRegionCount==9);
    assert(LayerPatch::size==1017&&LayerPatch::fx==968);
    t.reset();reloaded.reset();std::filesystem::remove_all(folder);
    std::cout<<"Cutups/Texture authored regions: audible source selection, file-coordinate FIELD, fallbacks, native/MIDI controls, sound locks, persistence and format-63 compatibility: PASS\n";
}
