void testSynthHybridTransfer(){
  using namespace s3g_thru_synth;
  constexpr float rate=16000;constexpr int frames=16000;
  const auto no=[]{return false;};const auto progress=[](unsigned){};
  HybridInputs in;
  for(int j=0;j<4;++j){auto a=std::make_shared<SourceAudio>();a->rate=rate;
    for(int i=0;i<frames;++i){const float t=float(i)/rate;
      const float env=j==1?((i/3200)%2==0?1.f:0.f):1.f;
      a->left.push_back(env*(.25f*std::sin(Plan::tau*(j==1?440:220)*t)+.055f*std::sin(Plan::tau*(j==2?2800:660)*t)));
      a->right.push_back(env*.17f*std::sin(Plan::tau*(j==1?440:220)*t+.7f));}
    in[j]=a;
  }
  Recipe r;initializeGenerator(r,hybrid);r.v[length]=1;r.v[attack]=r.v[release]=0;
  r.transfer[htEnvelope]=r.transfer[htSpectrum]=r.transfer[htPitch]=0;
  r.transfer[htStart]=0;
  Audio a,b;assert(valid(r));
  const auto run=[&](const Recipe& recipe,Audio& output){assert(render(recipe,rate,output,no,progress,nullptr,nullptr,&in));
    assert(output.left.size()==frames);for(int i=0;i<frames;++i)assert(std::isfinite(output.left[i])&&std::isfinite(output.right[i])&&std::abs(output.left[i])<.892f&&std::abs(output.right[i])<.892f);};
  run(r,a);
  WorkshopSpan span(r,rate,b);b.left=in[0]->left;b.right=in[0]->right;
  assert(finishWorkshop(r,rate,b,span,no,true));
  float error=0;for(int i=0;i<frames;++i)error=std::max({error,std::abs(a.left[i]-b.left[i]),std::abs(a.right[i]-b.right[i])});
  assert(error<3e-5f&&a.left!=a.right);
  // No random placement or seed sensitivity unless RAND has been enabled.
  r.seed=5732;run(r,b);assert(a.left==b.left&&a.right==b.right);
  r.v[width]=0;run(r,b);assert(b.left==b.right);r.v[width]=1;
  ResynthAnalysis analysis{*in[0],0,in[0]->left.size(),{}};HybridProfile profile;
  assert(hybridAnalyze(analysis,rate,profile,no));
  assert(std::abs(profile.at(8).hz-220)<8&&profile.at(8).confidence>.6f);
  const auto energy=[](const Audio& audio,int begin,int end){double x=0;for(int i=begin;i<end;++i)x+=double(audio.left[i])*audio.left[i];return x/(end-begin);};
  r.transfer[htEnvelope]=1;r.transfer[htAttack]=0;run(r,b);
  assert(energy(b,700,2000)>energy(b,4000,5600)*8);
  assert(b.left!=a.left);
  r.transfer[htEnvelope]=0;r.transfer[htPitch]=1;r.transfer[htSources]=4;r.transfer[htPitchSource]=3;
  // A clean octave donor verifies pitch transfer independently of envelope.
  auto donor=std::make_shared<SourceAudio>();donor->rate=rate;
  for(int i=0;i<frames;++i){float v=.2f*std::sin(Plan::tau*440*i/rate);donor->left.push_back(v);donor->right.push_back(v);}in[3]=donor;
  run(r,b);
  SourceAudio pitchOutput;pitchOutput.rate=rate;pitchOutput.left=b.left;pitchOutput.right=b.right;
  ResynthAnalysis pa{pitchOutput,0,pitchOutput.left.size(),{}};HybridProfile pp;
  assert(hybridAnalyze(pa,rate,pp,no));
  std::fprintf(stderr,"Hybrid neutral error %.9f; pitch %.2f Hz\n",error,pp.at(8).hz);
  assert(std::abs(pp.at(8).hz-440)<20);
  r.transfer[htPitch]=0;r.transfer[htSpectrum]=1;r.transfer[htSpecSource]=2;run(r,b);assert(a.left!=b.left);
  const auto stable=b;r.seed=938;run(r,b);assert(stable.left==b.left);
  r.transfer[htRandom]=1;run(r,b);assert(stable.left!=b.left);
  for(int align=0;align<3;++align){r.transfer[htAlign]=float(align);r.transfer[htOffset]=-1700;run(r,b);r.transfer[htOffset]=7200;run(r,b);}
  r.transfer[htAlign]=0;r.transfer[htOffset]=0;
  auto bytes=encode(r);assert(bytes.size()==hybridRecipeBytes);Recipe restored;
  assert(decode(bytes.data(),bytes.size(),restored)&&restored.hybridStyle==1&&restored.transfer==r.transfer);
  run(restored,b);run(r,a);assert(a.left==b.left&&a.right==b.right);
  // A pre-redesign v10 Hybrid remains the legacy process, through re-saving.
  auto old=bytes;old.resize(recipeBytes-4);old[0]=10;put(old,checksum(old));
  assert(decode(old.data(),old.size(),restored)&&restored.hybridStyle==0);
  Audio legacy;assert(renderLegacyHybrid(restored,rate,legacy,in,no,progress));run(restored,a);
  assert(legacy.left==a.left&&legacy.right==a.right);
  bytes=encode(restored);assert(decode(bytes.data(),bytes.size(),r)&&r.hybridStyle==0);run(r,b);assert(a.left==b.left);
  r=Recipe{};r.type=hybrid;r.v[length]=1;
  for(int i=0;i<hybridTransferFields;++i)assert(!setHybridTransfer(r,i,NAN));
  assert(!setHybridTransfer(r,-1,1)&&!setHybridTransfer(r,hybridTransferFields,1));
  setHybridTransfer(r,htSources,4);setHybridTransfer(r,htSpecSource,3);setHybridTransfer(r,htSources,2);
  assert(r.transfer[htSpecSource]==1&&valid(r));
  for(int start=1;start<=4;++start){setHybridTransfer(r,htStart,float(start));assert(valid(r));run(r,b);}
  for(int count=2;count<=4;++count){setHybridTransfer(r,htSources,float(count));auto missing=in;missing[count-1].reset();
    assert(!render(r,rate,b,no,progress,nullptr,nullptr,&missing));}
  assert(!render(r,rate,b,[]{return true;},progress,nullptr,nullptr,&in));
  auto silent=std::make_shared<SourceAudio>();silent->rate=rate;silent->left.resize(frames);silent->right.resize(frames);in[0]=silent;
  run(r,b);for(float v:b.left)assert(v==0);
  // Neutral reconstruction at the production rate and higher source rates.
  for(float sr:{44100.f,48000.f,96000.f}){
    Recipe identity;initializeGenerator(identity,hybrid);identity.v[length]=.12f;
    identity.v[attack]=identity.v[release]=0;identity.transfer[htEnvelope]=0;
    const int size=int(std::llround(sr*identity.v[length]));auto source=std::make_shared<SourceAudio>();source->rate=sr;
    for(int i=0;i<size;++i){source->left.push_back(.3f*std::sin(Plan::tau*220*i/sr));source->right.push_back(-source->left.back());}
    HybridInputs sources{source,source,source,source};Audio output,reference;
    assert(render(identity,sr,output,no,progress,nullptr,nullptr,&sources));
    WorkshopSpan expected(identity,sr,reference);reference.left=source->left;reference.right=source->right;
    assert(finishWorkshop(identity,sr,reference,expected,no,true));
    for(int i=0;i<size;++i)assert(std::abs(output.left[i]-reference.left[i])<3e-5f&&std::abs(output.right[i]-reference.right[i])<3e-5f);
  }
  std::puts("Hybrid trait transfer: neutral/stereo, real envelope, pitch, colour, donor routing, alignment, seed opt-in, codec and legacy recall pass");
}
