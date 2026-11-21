void testSynthFmTypeCore() {
  using namespace s3g_thru_synth;
  Recipe old;
  initializeGenerator(old, drum);
  initializeDrumModel(old, 10);
  old.v[length] = .2f;
  old.v[width] = 1;
  old.v[attack] = 0;
  old.v[release] = 2;
  const auto legacy = encode(old);
  Recipe migrated;
  assert(legacy[0] == 10 && decode(legacy.data(), legacy.size(), migrated) &&
         encode(migrated) == legacy);
  Recipe modern = old;
  modern.type = fmDrum;
  const auto bytes = encode(modern);
  assert(bytes.size() == recipeBytes && bytes[0] == 12 &&
         decode(bytes.data(), bytes.size(), migrated) &&
         encode(migrated) == bytes);
  const auto no = [] { return false; };
  const auto progress = [](unsigned) {};
  Audio a, b;
  assert(render(old, 48000, a, no, progress) &&
         render(modern, 48000, b, no, progress) && a.left == b.left &&
         a.right == b.right);
  old.events[eMode] = modern.events[eMode] = 1;
  old.v[length] = modern.v[length] = .3f;
  old.events[eCount] = modern.events[eCount] = 3;
  old.timing[eBpm] = modern.timing[eBpm] = 240;
  assert(render(old, 16000, a, no, progress) &&
         render(modern, 16000, b, no, progress) && a.left == b.left &&
         a.right == b.right);
  assert(!setDrum(modern, dModel, 0));
  for (int i = 0; i < 20; ++i) {
    mutate(modern, .5f);
    assert(modern.drums[dModel] == 10 && valid(modern));
  }
  std::puts("FM Drum type: independent defaults/recipe12, exact legacy FM "
            "audio and Events identity, protected model and mutation: PASS");
}
void testSynthFmTypeWorkflow() {
  using namespace s3g_thru_synth;
  char dir[] = "/tmp/thru-fmtype.XXXXXX";
  assert(mkdtemp(dir));
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(dir));
  t->stopBackgroundWork();
  assert(t->persistencePath("captures/Synths", t->synthDirectory.data(),
                            t->synthDirectory.size()));
  setParam(t.get(), "synth_open", "1");
  setParam(t.get(), "synth_type", "14");
  assert(t->synthRecipe.type == fmDrum && t->synthRecipe.drums[dModel] == 10 &&
         !t->synthWantsRender);
  setParam(t.get(), "synth_edit", "0,0.18");
  setParam(t.get(), "synth_drum", "13,0.7");
  waitSynth(*t);
  if(const char *file=std::getenv("S3G_THRU_SYNTH_TYPES_FIXTURE")) {
    std::array<char,4096> frame{};assert(t->formatSynth(frame.data(),frame.size())>0);
    std::ifstream in(file);std::string all((std::istreambuf_iterator<char>(in)),{});assert(!all.empty()&&all.back()==']');all.pop_back();
    all+=std::string(",{\"label\":\"fm-drum-separate\",\"frame\":")+frame.data()+"}]";std::ofstream out(file);out<<all;
  }
  const auto recipe = encode(t->synthRecipe);
  const auto audio = t->synthWorker->audio[t->synthActive];
  setParam(t.get(), "synth_name", "FM Drum independent");
  setParam(t.get(), "synth_save", "1");
  waitSynth(*t, false);
  const std::string path = t->synthLastSaved.data();
  for (int i = 0;
       i < 15000 && (t->sampleLoadBusy() || t->synthLoadPath[0] ||
                     path != t->currentLaneSourcePaths[t->selectedLane].data());
       ++i) {
    renderSplit(*t, 1);
    usleep(500);
  }
  assert(path == t->currentLaneSourcePaths[t->selectedLane].data());
  setParam(t.get(), "synth_type", "8");
  assert(t->synthRecipe.drums[dModel] == 0);
  setParam(t.get(), "synth_restore", "1");
  waitSynth(*t);
  assert(encode(t->synthRecipe) == recipe &&
         t->synthWorker->audio[t->synthActive].left == audio.left &&
         t->synthWorker->audio[t->synthActive].right == audio.right);
  // Explicit randomize + Render updates actual audio, not just the recipe.
  setParam(t.get(), "synth_type", "13");
  waitSynth(*t);
  const auto old = t->synthWorker->audio[t->synthActive].left;
  const auto controls = t->synthRecipe.wt;
  setParam(t.get(), "synth_mutate", "0.25");
  assert(t->synthRecipe.wt != controls && !t->synthWantsRender &&
         t->synthRecipe.wt[wtModel] == controls[wtModel] &&
         t->synthRecipe.wt[wtTune] == controls[wtTune]);
  setParam(t.get(), "synth_render", "1");
  waitSynth(*t, false);
  assert(t->synthReady() && t->synthWorker->audio[t->synthActive].left != old);
  t.reset();
  std::filesystem::remove_all(dir);
  std::puts(
      "FM Drum workflow: separate workspace, manual controls, portable Save & "
      "Load/restore and actual WT randomize+render audio replacement: PASS");
}
