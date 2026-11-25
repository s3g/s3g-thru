#define S3G_THRU_STORAGE_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <thread>
using J = s3g_thru_json::Value;
namespace fs = std::filesystem;
static int audioPeak = 0;
static void drive(ThruInstance &t) {
  t.serviceComposer();
  t.processComposerWorker();
  std::array<int16_t, 256> out{};
  t.render(out.data(), 128);
  for (const auto value : out)
    audioPeak = std::max(audioPeak, std::abs(int(value)));
  std::this_thread::sleep_for(std::chrono::microseconds(80));
}
int main() {
  char folder[] = "/tmp/thru-browser-samples.XXXXXX";
  assert(mkdtemp(folder));
  const std::string root = fs::canonical(folder).string();
  auto t = std::make_unique<ThruInstance>();
  assert(t->prepare(folder));
  t->stopBackgroundWork();
  fs::create_directories(root + "/library/Drums");
  fs::create_directories(root + "/process");
  std::snprintf(t->synthDirectory.data(), t->synthDirectory.size(),
                "%s/process", folder);
  std::array<float, 2048> left{}, right{};
  for (int i = 0; i < 2048; ++i) {
    left[i] = .6f * std::sin(i * .05f);
    right[i] = .25f * std::cos(i * .08f);
  }
  const auto wave = [&](const std::string &path, int n = 2048) {
    fs::remove(path);
    fs::remove(path + ".tmp");
    assert(ThruInstance::writeStereoWavFile(left.data(), right.data(), n, 44100,
                                            path.c_str(),
                                            (path + ".tmp").c_str(), true));
  };
  wave(root + "/library/Drums/Kick.WAV");
  wave(root + "/captures/take.wav", 1024);
  wave(root + "/process/tone.wav", 512);
  s3g_thru_pack::atomicFile(root + "/library/not-a-wave.txt", "ignore");
  s3g_thru_pack::atomicFile(root + "/library/broken.wav", std::string(64, 'x'));
  fs::create_symlink(root + "/captures/take.wav",
                     root + "/library/outside.wav");
  fs::create_directory_symlink(root + "/captures", root + "/library/outside");
  int serial = 0;
  const auto begin = [&](const std::string &op, const J &request) {
    const auto id = std::to_string(++serial), leaf = "request-" + id + ".json";
    if (op != "hello")
      s3g_thru_pack::atomicFile(t->composerRoot + "/uploads/" + leaf,
                                s3g_thru_json::stringify(request));
    const auto packet = "test|" + id + "|" + op + "|" +
                        (op == "hello" ? "" : leaf) + "|" +
                        std::to_string(t->persistentRevision);
    assert(t->setComposerParam("composer_command", packet.c_str()));
    return id;
  };
  const auto finish = [&](const std::string &id) {
    for (int i = 0; i < 12000; ++i) {
      drive(*t);
      const auto path = t->composerRoot + "/replies/test-" + id + ".json";
      if (fs::exists(path))
        return s3g_thru_json::parse(s3g_thru_pack::readFile(path, 1024 * 1024));
    }
    std::cerr << "Timeout " << id << " phase " << t->composerPhase.load()
              << " load " << t->sampleLoadState.load() << " err "
              << t->lastError.data() << "\n";
    std::abort();
  };
  const auto command = [&](const std::string &op, const J &request) {
    return finish(begin(op, request));
  };
  assert(command("hello", J()).at("ok").boolean());
  const auto browse = [&](int source, const std::string &path = "",
                          int offset = 0, const std::string &search = "") {
    return command("samples",
                   ThruInstance::composerObject({{"root", source},
                                                 {"path", path},
                                                 {"offset", offset},
                                                 {"search", search}}));
  };
  auto reply = browse(0);
  assert(reply.at("ok").boolean());
  auto listing = s3g_thru_json::parse(
      s3g_thru_pack::readFile(reply.at("result").string(), 65536));
  assert(listing.at("total").number() == 2);
  assert(listing.at("entries").arrayValue()[0].at("name").string() == "Drums");
  assert(browse(1).at("ok").boolean() && browse(2).at("ok").boolean());
  assert(!browse(0, "../captures").at("ok").boolean());
  assert(!browse(0, "outside").at("ok").boolean());
  assert(!browse(0, "/tmp").at("ok").boolean());
  assert(!browse(0, "Drums/./").at("ok").boolean());
  assert(!browse(9).at("ok").boolean());
  assert(!browse(0, "", -1).at("ok").boolean());
  assert(
      s3g_thru_json::parse(s3g_thru_pack::readFile(
                               t->composerRoot + "/status.json", 1024 * 1024))
          .at("ok")
          .boolean());
  for (int i = 0; i < 68; ++i)
    s3g_thru_pack::atomicFile(
        root + "/library/Drums/part-" + std::to_string(i) + ".wav", "");
  reply = browse(0, "Drums", 64);
  listing = s3g_thru_json::parse(
      s3g_thru_pack::readFile(reply.at("result").string(), 65536));
  assert(listing.at("total").number() == 69 &&
         listing.at("entries").arrayValue().size() == 5);
  reply = browse(0, "Drums", 0, "KICK");
  listing = s3g_thru_json::parse(
      s3g_thru_pack::readFile(reply.at("result").string(), 65536));
  assert(listing.at("entries").arrayValue().size() == 1);
  const auto request = [&](int lane, const std::string &path) {
    return ThruInstance::composerObject(
        {{"clip", t->activeClip},
         {"part", t->splitEnabled ? t->splitFocus : 0},
         {"lane", lane},
         {"mode", t->performanceMode()},
         {"file", false},
         {"root", 0},
         {"path", path}});
  };
  t->setPerformanceMode(6);
  setParam(t.get(), "algorithm", "4");
  t->sequenceLive.tracks[0].trigs[3].enabled = true;
  t->sequenceLive.tracks[0].trigs[3].locks.timeLocked = true;
  t->sequenceLive.tracks[0].trigs[3].locks.timeMs = 170;
  t->commitSequenceEdit(false);
  auto *clip = t->activeClipSlot();
  clip->clipNoteCount = 1;
  clip->clipNotes[0].note = 60;
  clip->clipNotes[0].durationMs = 100;
  const auto version = t->composerPanelWaveVersion.load();
  reply = command("sample", request(0, "Drums/Kick.WAV"));
  if (!reply.at("ok").boolean())
    std::cerr << reply.at("message").string() << "\n";
  assert(reply.at("ok").boolean());
  assert(t->engine.laneFrames(0) == 2048 &&
         t->engine.laneSampleRate(0) == 44100);
  assert(std::abs(t->engine.laneData(0, 0)[21] - left[21]) < .0001f &&
         std::abs(t->engine.laneData(0, 1)[21] - right[21]) < .0001f);
  assert(t->composerPanelWaveVersion.load() > version);
  assert(t->sequenceLive.tracks[0].trigs[3].enabled &&
         t->sequenceLive.tracks[0].trigs[3].locks.timeMs == 170 &&
         clip->clipNoteCount == 1);
  assert(!t->engine.laneLoaded(1));
  assert(fs::equivalent(t->currentLaneSourcePaths[0].data(),
                        root + "/library/Drums/Kick.WAV"));
  const auto prior = std::string(t->currentLaneSourcePaths[0].data());
  reply = command("sample", request(0, "broken.wav"));
  assert(!reply.at("ok").boolean());
  assert(t->engine.laneFrames(0) == 2048 &&
         prior == t->currentLaneSourcePaths[0].data());
  // Frozen context, lane ownership and active recording guards reject before
  // the old source changes.
  assert(command("sample", request(1, "Drums/Kick.WAV")).at("ok").boolean());
  assert(t->splitFocus == 0 && t->selectedLane == 0 &&
         t->engine.laneFrames(1) == 2048);
  auto invalid = request(0, "Drums/Kick.WAV");
  invalid["clip"] = 1;
  assert(!command("sample", invalid).at("ok").boolean());
  invalid = request(0, "Drums/Kick.WAV");
  invalid["mode"] = 0;
  assert(!command("sample", invalid).at("ok").boolean());
  auto id = begin("sample", request(0, "Drums/Kick.WAV"));
  t->processComposerWorker();
  assert(t->composerSampleJob.load() == 1);
  t->markPersistentStateDirty();
  assert(!finish(id).at("ok").boolean());
  t->clipRecording = true;
  assert(!command("sample", request(0, "Drums/Kick.WAV")).at("ok").boolean());
  t->clipRecording = false;
  t->setPerformanceMode(1);
  t->selectSplitPart(1, false);
  t->selectedLane = 2;
  assert(command("sample", request(0, "Drums/Kick.WAV")).at("ok").boolean());
  assert(t->splitFocus == 1 && t->selectedLane == 2);
  assert(command("sample", request(2, "Drums/Kick.WAV")).at("ok").boolean());
  assert(t->splitFocus == 1 && t->engine.laneFrames(2) == 2048);
  // Uploaded files survive outside the temporary upload folder and reuse
  // content-addressed destinations.
  wave(t->composerRoot + "/uploads/new.wav", 1024);
  auto upload = ThruInstance::composerObject({{"clip", 0},
                                              {"part", 1},
                                              {"lane", 3},
                                              {"mode", 1},
                                              {"file", false},
                                              {"upload", "new.wav"},
                                              {"name", "Studio take.wav"}});
  reply = command("sample", upload);
  assert(reply.at("ok").boolean());
  const std::string saved = t->currentLaneSourcePaths[3].data();
  assert(saved == root + "/library/Browser Samples/Studio take.wav" &&
         fs::exists(saved));
  fs::remove(t->composerRoot + "/uploads/new.wav");
  assert(t->engine.laneFrames(3) == 1024 && fs::exists(saved));
  wave(t->composerRoot + "/uploads/new.wav", 1024);
  assert(command("sample", upload).at("ok").boolean() &&
         saved == t->currentLaneSourcePaths[3].data());
  // Upload into a selected existing library folder, without changing edit
  // focus.
  wave(t->composerRoot + "/uploads/new.wav", 1024);
  upload["folder"] = "Drums";
  assert(command("sample", upload).at("ok").boolean());
  const std::string chosen = t->currentLaneSourcePaths[3].data();
  assert(chosen == root + "/library/Drums/Studio take.wav" &&
         fs::exists(chosen) && fs::exists(saved));
  const auto rejectedSource = chosen;
  for (const auto *invalidFolder :
       {"../captures", "outside", "/tmp", "missing", "Drums/Kick.WAV"}) {
    wave(t->composerRoot + "/uploads/new.wav", 1024);
    upload["folder"] = invalidFolder;
    assert(!command("sample", upload).at("ok").boolean());
    assert(rejectedSource == t->currentLaneSourcePaths[3].data());
  }
  wave(t->composerRoot + "/uploads/new.wav", 1024);
  upload["folder"] = "";
  assert(command("sample", upload).at("ok").boolean());
  assert(fs::path(t->currentLaneSourcePaths[3].data()).parent_path() ==
         fs::path(root + "/library"));
  upload.objectValue().erase("folder");
  // Audition uses the same preview bus as Move and never installs a sample.
  auto audition = request(0, "Drums/Kick.WAV");
  audition["preview"] = true;
  const auto oldSource = t->currentLaneSourcePaths[0];
  const auto oldRevision = t->persistentRevision;
  const auto oldUndo = t->undoAvailable;
  audioPeak = 0;
  assert(command("sample", audition).at("ok").boolean());
  for (int i = 0; i < 60; ++i)
    drive(*t);
  assert(audioPeak > 20 && t->currentLaneSourcePaths[0] == oldSource &&
         t->persistentRevision == oldRevision && t->undoAvailable == oldUndo &&
         t->splitFocus == 1);
  wave(t->composerRoot + "/uploads/audition.wav", 1024);
  auto uploadedPreview =
      ThruInstance::composerObject({{"clip", 0},
                                    {"part", 1},
                                    {"lane", 3},
                                    {"mode", 1},
                                    {"file", true},
                                    {"preview", true},
                                    {"upload", "audition.wav"},
                                    {"name", "Only audition.wav"}});
  const auto oldLane3 = t->currentLaneSourcePaths[3];
  audioPeak = 0;
  assert(command("sample", uploadedPreview).at("ok").boolean());
  for (int i = 0; i < 60; ++i)
    drive(*t);
  assert(audioPeak > 20 && t->currentLaneSourcePaths[3] == oldLane3 &&
         !fs::exists(t->composerRoot + "/uploads/audition.wav"));
  for (const auto &file :
       fs::directory_iterator(root + "/library/Browser Samples"))
    assert(file.path().filename().string().rfind("Only audition-", 0) != 0);
  // Readable original names: reuse identical content; only collisions get
  // numbers.
  wave(t->composerRoot + "/uploads/new.wav", 512);
  auto collision = upload;
  collision["name"] = "Studio take.wav";
  assert(command("sample", collision).at("ok").boolean());
  assert(std::string(t->currentLaneSourcePaths[3].data()) ==
         root + "/library/Browser Samples/Studio take-2.wav");
  wave(t->composerRoot + "/uploads/new.wav", 512);
  assert(command("sample", collision).at("ok").boolean());
  assert(std::string(t->currentLaneSourcePaths[3].data()) ==
         root + "/library/Browser Samples/Studio take-2.wav");
  const auto markerRequest = [&]() {
    return ThruInstance::composerObject(
        {{"clip", t->activeClip},
         {"part", t->splitFocus},
         {"lane", 3},
         {"mode", t->performanceMode()},
         {"asset", s3g_thru_pack::sha256(
                       std::string(t->currentLaneSourcePaths[3].data()))
                       .substr(0, 24)},
         {"wave", double(t->composerPanelWaveVersion.load())},
         {"name", "Marked take.wav"},
         {"points", J::Array{ThruInstance::composerObject(
                                 {{"position", 0}, {"label", "Start"}}),
                             ThruInstance::composerObject(
                                 {{"position", 0.5}, {"label", "Middle"}})}}});
  };
  // Preview the draft range without saving markers or changing the clip/lane.
  auto draftPreview = markerRequest();
  draftPreview.objectValue().erase("name");
  draftPreview.objectValue().erase("points");
  draftPreview["audition"] = true;
  draftPreview["start"] = .25;
  draftPreview["end"] = .75;
  const auto auditionSource = t->currentLaneSourcePaths[3];
  const auto auditionRevision = t->persistentRevision;
  const auto auditionUndo = t->undoAvailable;
  const auto auditionNotes = t->activeClipSlot()->clipNoteCount;
  audioPeak = 0;
  assert(command("markers", draftPreview).at("ok").boolean());
  const auto start = std::size_t(std::llround(t->sampleStageFrames * .25)),
             end = std::size_t(std::llround(t->sampleStageFrames * .75));
  assert(t->previewFrame >= start && t->previewFrame <= end &&
         t->previewEndFrame == end);
  for (int i = 0; i < 60; ++i)
    drive(*t);
  assert(!t->previewActive && t->previewFrame == end && audioPeak > 20);
  assert(t->currentLaneSourcePaths[3] == auditionSource &&
         t->persistentRevision == auditionRevision &&
         t->undoAvailable == auditionUndo &&
         t->activeClipSlot()->clipNoteCount == auditionNotes);
  // Last slice ends at the decoded file end; Stop preserves all clip data.
  draftPreview["start"] = .75;
  draftPreview["end"] = 1;
  assert(command("markers", draftPreview).at("ok").boolean());
  setParam(t.get(), "preview_stop", "1");
  for (int i = 0; i < 60; ++i)
    drive(*t);
  assert(!t->previewActive && t->currentLaneSourcePaths[3] == auditionSource);
  assert(t->persistentRevision == auditionRevision &&
         t->undoAvailable == auditionUndo);
  setParam(t.get(), "preview_stop", "1");
  assert(t->persistentRevision == auditionRevision);
  // Very short slices never read past their boundary; both endpoints reach
  // zero.
  draftPreview["start"] = 0;
  draftPreview["end"] = 2. / 512;
  assert(command("markers", draftPreview).at("ok").boolean());
  for (int i = 0; i < 10; ++i)
    drive(*t);
  assert(!t->previewActive && t->previewFrame == 2);
  auto badPreview = draftPreview;
  badPreview["end"] = 0;
  assert(!command("markers", badPreview).at("ok").boolean());
  badPreview = draftPreview;
  badPreview["start"] = -.1;
  assert(!command("markers", badPreview).at("ok").boolean());
  badPreview = draftPreview;
  badPreview["end"] = 1.1;
  assert(!command("markers", badPreview).at("ok").boolean());
  badPreview = draftPreview;
  badPreview["wave"] = double(t->composerPanelWaveVersion.load() + 1);
  assert(!command("markers", badPreview).at("ok").boolean());
  // Ordinary sample audition returns to the full file after a ranged preview.
  assert(command("sample", audition).at("ok").boolean());
  assert(t->samplePreviewLow == 0 && t->samplePreviewHigh == 1 &&
         t->previewEndFrame == 0);
  for (int i = 0; i < 60; ++i)
    drive(*t);
  const auto transientRevision = t->persistentRevision;
  setParam(t.get(), "preview_file", (root + "/library/Drums/Kick.WAV").c_str());
  for (int i = 0; i < 100; ++i)
    drive(*t);
  setParam(t.get(), "preview_stop", "1");
  for (int i = 0; i < 10; ++i)
    drive(*t);
  assert(t->persistentRevision == transientRevision &&
         t->undoAvailable == auditionUndo);
  const std::string markerOriginal = t->currentLaneSourcePaths[3].data();
  const auto originalBytes =
      s3g_thru_pack::readFile(markerOriginal, 1024 * 1024);
  const auto originalCount = t->activeClipSlot()->clipNoteCount;
  const auto originalFocus = t->splitFocus;
  t->activeClipSlot()->sliceLanes[3].preRollMs = 7;
  auto marked = markerRequest();
  assert(command("markers", marked).at("ok").boolean());
  assert(std::string(t->currentLaneSourcePaths[3].data()) ==
         root + "/library/Browser Samples/Marked take.wav");
  assert(t->activeClipSlot()->sliceLanes[3].preRollMs == 7);
  struct stat markedMode {};
  assert(::stat(t->currentLaneSourcePaths[3].data(), &markedMode) == 0 &&
         (markedMode.st_mode & 0777) == 0644);
  assert(t->fileSliceMaps[3].count == 2 &&
         std::abs(t->fileSliceMaps[3].points[1] - .5f) < .00001f &&
         std::string(t->fileSliceMaps[3].labels[1].data()) == "Middle");
  assert(s3g_thru_pack::readFile(markerOriginal, 1024 * 1024) ==
             originalBytes &&
         t->splitFocus == originalFocus &&
         t->activeClipSlot()->clipNoteCount == originalCount);
  assert(std::abs(t->engine.laneData(3, 0)[21] - left[21]) < .0001f);
  auto stale = markerRequest();
  stale["wave"] = double(t->composerPanelWaveVersion.load() + 1);
  assert(!command("markers", stale).at("ok").boolean());
  stale = markerRequest();
  stale["asset"] = std::string(24, 'a');
  assert(!command("markers", stale).at("ok").boolean());
  stale = markerRequest();
  stale["points"].arrayValue()[1]["position"] = 0;
  assert(!command("markers", stale).at("ok").boolean());
  stale = markerRequest();
  stale["name"] = "../replace.wav";
  assert(!command("markers", stale).at("ok").boolean());
  t->composerPanelJob.store(1);
  t->captureComposerPanel();
  t->publishComposerPanel();
  const auto markerPanel = s3g_thru_json::parse(
      s3g_thru_pack::readFile(t->composerPanelPath, 4 * 1024 * 1024));
  assert(markerPanel.at("sampleMarkers")
             .arrayValue()[3]
             .at("file")
             .arrayValue()
             .size() == 2);
  // Restoring the cached lane audio must also restore its file marker map.
  assert(t->savePersistentState());
  auto restored = std::make_unique<ThruInstance>();
  assert(restored->prepare(folder));restored->stopBackgroundWork();
  assert(restored->engine.laneLoaded(3));
  assert(restored->activeClipSlot()->sliceLanes[3].file.count == 2);
  assert(restored->fileSliceMaps[3].count == 2);
  assert(std::string(restored->fileSliceMaps[3].labels[1].data()) == "Middle");
  restored->composerInitWorker();
  restored->composerPanelClip = std::make_unique<ClipSlot>();
  restored->composerPanelJob.store(1);restored->captureComposerPanel();restored->publishComposerPanel();
  const auto restoredPanel = s3g_thru_json::parse(s3g_thru_pack::readFile(restored->composerPanelPath, 4 * 1024 * 1024));
  assert(restoredPanel.at("sampleMarkers").arrayValue()[3].at("file").arrayValue().size() == 2);
  restored.reset();
  std::cout << "Saved browser WAV markers survive session restart and publish in the marker popup: PASS\n";
  upload["name"] = "../bad.wav";
  assert(!command("sample", upload).at("ok").boolean());
  upload["name"] = "Studio take.wav";
  upload["upload"] = "../new.wav";
  assert(!command("sample", upload).at("ok").boolean());
  // Live Memory is changed only after a valid file has committed; other part
  // focus is retained.
  t->selectSplitPart(0, false);
  setParam(t.get(), "algorithm", "2");
  setParam(t.get(), "live_source", "1");
  t->selectSplitPart(1, false);
  setParam(t.get(), "algorithm", "2");
  setParam(t.get(), "live_source", "1");
  auto crossPart = request(0, "Drums/Kick.WAV");
  crossPart["file"] = true;
  assert(command("sample", crossPart).at("ok").boolean() &&
         t->splitFocus == 1 && t->controls.sourceMotion.v[8] == 1 &&
         t->splitParts[0].controls.sourceMotion.v[8] == 0);
  t->noteOn(60, 100);
  for (int i = 0; i < 24; ++i)
    drive(*t);
  assert(t->controls.sourceMotion.v[8] == 1);
  auto live = request(2, "broken.wav");
  live["file"] = true;
  assert(!command("sample", live).at("ok").boolean() &&
         t->controls.sourceMotion.v[8] == 1);
  live["path"] = "Drums/Kick.WAV";
  assert(command("sample", live).at("ok").boolean() &&
         t->controls.sourceMotion.v[8] == 0 && t->splitFocus == 1);
  t->noteOff(60);
  // Move's own loading path also invalidates browser waveforms without reading
  // OLED wave text.
  const auto before = t->composerPanelWaveVersion.load();
  setParam(t.get(), "load_lane_file",
           ("3," + root + "/captures/take.wav").c_str());
  for (int n = 0;
       n < 10000 && t->sampleLoadState.load() != int(SampleLoadState::complete);
       ++n)
    drive(*t);
  assert(t->composerPanelWaveVersion.load() > before &&
         t->engine.laneFrames(2) == 1024);
  t->composerPanelJob.store(1);
  t->captureComposerPanel();
  t->publishComposerPanel();
  auto panel = s3g_thru_json::parse(
      s3g_thru_pack::readFile(t->composerPanelPath, 4 * 1024 * 1024));
  bool found = false;
  for (const auto &asset : panel.at("document").at("assets").arrayValue())
    if (asset.at("name").string() == "take.wav")
      found = true;
  assert(found);
  // Bank's right-hand 16 pads are deliberately outside this lane loader.
  t->setPerformanceMode(4);
  t->selectSplitPart(1, false);
  assert(!command("sample", request(3, "Drums/Kick.WAV")).at("ok").boolean());
  t.reset();
  fs::remove_all(root);
  std::cout << "Browser samples: library/upload, stereo audio, guards, "
               "retained sequence/notes, invalid files, durable dedup, Live "
               "Memory, chosen upload folders, cross-part lanes, "
               "nondestructive audition and Move waveform refresh: PASS\n";
}
