void testSynthDrumAudition() {
  using namespace s3g_thru_synth;
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(nullptr));t->stopBackgroundWork();
  setParam(t.get(),"synth_open","1");
  t->previewLevel=t->previewLevelState=.7f;
  const auto reset = [&](Audio audio){
    assert(t->synthWorker->state.load()==Worker::idle);
    t->synthWorker->audio[0]=std::move(audio);t->synthRecipe=t->synthWorker->audio[0].recipe;
    t->synthActive=0;t->synthFade=-1;t->synthFadeRemaining=0;
    t->synthPreviewWanted=false;t->synthPreviewGain=0;t->synthPosition=0;
  };
  const auto capture = [&](std::size_t blocks){
    std::vector<float> left,right;std::size_t start=std::size_t(-1);
    for(std::size_t b=0;b<blocks;++b){
      const auto before=t->synthPosition;renderSplit(*t,1);
      if(before==0&&t->synthPosition>0)start=left.size()+kBlockFrames-t->synthPosition;
      left.insert(left.end(),t->directPreview[0].begin(),t->directPreview[0].begin()+kBlockFrames);
      right.insert(right.end(),t->directPreview[1].begin(),t->directPreview[1].begin()+kBlockFrames);
    }
    return std::make_tuple(std::move(left),std::move(right),start);
  };
  for(int model=0;model<drumModelCount+wtModelCount+1;++model){
    Recipe r;initializeGenerator(r,model<drumModelCount?drum:model<drumModelCount+wtModelCount?wtDrum:fmDrum);if(model<drumModelCount)initializeDrumModel(r,model);else if(model<drumModelCount+wtModelCount)initializeWtDrumModel(r,model-drumModelCount);
    r.v[length]=.18f;r.v[attack]=0;r.v[release]=2;r.v[width]=1;
    Audio source;assert(render(r,t->sampleRate,source,[]{return false;},[](unsigned){}));
    reset(source);const auto expectedFrames=source.left.size();
    setParam(t.get(),"synth_preview","1");
    auto [left,right,start]=capture((expectedFrames+std::size_t(t->sampleRate*.03f))/kBlockFrames+2);
    assert(start>=std::size_t(t->sampleRate*.008f)-2&&start<=std::size_t(t->sampleRate*.008f)+2);
    assert(start+expectedFrames<=left.size());
    for(std::size_t i=0;i<start;++i)assert(left[i]==0&&right[i]==0);
    double wantedPower=0,heardPower=0;
    for(std::size_t i=0;i<expectedFrames;++i){
      assert(std::abs(left[start+i]-source.left[i]*.7f)<1e-6f);
      assert(std::abs(right[start+i]-source.right[i]*.7f)<1e-6f);
      wantedPower+=double(source.left[i]*.7f)*(source.left[i]*.7f);
      heardPower+=double(left[start+i])*left[start+i];
    }
    assert(wantedPower>0&&std::abs(heardPower-wantedPower)<wantedPower*1e-5);
    assert(t->synthWorker->audio[0].left==source.left&&t->synthWorker->audio[0].right==source.right);
    // Cancelling inside the silent route ramp must not emit a partial hit.
    reset(source);setParam(t.get(),"synth_preview","1");
    capture(1);assert(t->synthPosition==0);
    setParam(t.get(),"synth_preview","0");
    auto [cancelL,cancelR,cancelStart]=capture(8);
    assert(cancelStart==std::size_t(-1)&&t->synthPosition==0&&t->synthPreviewGain==0);
    for(float value:cancelL)assert(value==0);for(float value:cancelR)assert(value==0);
    // Restarting before the old audition fades out retires its tail, then
    // starts the new attack at full route gain, without altering the WAV.
    reset(source);setParam(t.get(),"synth_preview","1");capture(8);
    setParam(t.get(),"synth_preview","0");capture(1);
    setParam(t.get(),"synth_preview","1");
    auto [againL,againR,againStart]=capture(25);
    assert(againStart>=std::size_t(t->sampleRate*.012f)-2&&againStart<=std::size_t(t->sampleRate*.012f)+2);
    for(std::size_t i=0;i<std::min(expectedFrames,againL.size()-againStart);++i){
      assert(std::abs(againL[againStart+i]-source.left[i]*.7f)<1e-6f);
      assert(std::abs(againR[againStart+i]-source.right[i]*.7f)<1e-6f);
    }
    setParam(t.get(),"synth_preview","0");capture(8);
  }
  // Continuous loops and other generators retain their gradual onset.
  for(int type:{wave,drum}){
    Audio a;initializeGenerator(a.recipe,type);a.recipe.v[loop]=1;
    a.left.assign(2048,.2f);a.right=a.left;reset(a);
    setParam(t.get(),"synth_preview","1");auto [l,rr,start]=capture(1);
    assert(start==0&&t->synthPosition==kBlockFrames&&l.front()>0&&l.front()<.001f&&l==rr);
    setParam(t.get(),"panic","1");capture(8);assert(!t->synthPreviewWanted&&t->synthPreviewGain==0);
  }
  std::puts("Synth Drum audition: eleven legacy Drum, six WT Drum and independent FM Drum starts retain exact file attack/body/stereo and peak through the real render route; silent pre-roll, early cancel, tail-safe restart, unchanged WAVs, loop onset and Panic: PASS");
}
