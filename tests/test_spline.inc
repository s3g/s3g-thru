void testSplineCore() {
    using namespace s3g_thru_synth;
    Recipe r;
    r.v[length] = .2f;
    r.v[drift] = 0;
    r.v[evolution] = 8;
    Audio a, b;
    const auto draw = [&](const Recipe &recipe, Audio &out, float rate = 16000) {
        return render(
            recipe, rate, out, [] { return false; }, [](unsigned) {});
    };
    assert(draw(r, a) && draw(r, b) && a.left == b.left && a.right == b.right && a.left == a.right);
    assert(a.left.size() == 3200 && a.left.front() == 0 && a.left.back() == 0);
    float peak = 0;
    double dc = 0;
    for (float x : a.left) {
        assert(std::isfinite(x));
        peak = std::max(peak, std::abs(x));
        dc += x;
    }
    assert(peak > .5f && peak <= .891251f * r.v[level] && std::abs(dc / a.left.size()) < .003);
    // Root pitch is the period, independent of spline complexity/evolution.
    int crossings = 0;
    for (std::size_t i = 400; i + 1 < a.left.size() - 400; ++i)
        crossings += a.left[i] <= 0 && a.left[i + 1] > 0;
    const float expected = 440.f * std::exp2((r.v[root] - 69) / 12) * (a.left.size() - 800) / 16000;
    assert(std::abs(crossings - expected) < 3);
    r.v[width] = 1;
    assert(draw(r, b) && b.left != b.right);
    const auto base = encode(r);
    assert(base.size() == recipeBytes);
    Recipe restored;
    assert(decode(base.data(), base.size(), restored) && encode(restored) == base);
    for (std::size_t i = 0; i < base.size(); ++i) {
        auto damaged = base;
        damaged[i] ^= 0x40;
        const auto before = encode(restored);
        assert(!decode(damaged.data(), damaged.size(), restored) && before == encode(restored));
    }
    for (std::size_t n = 0; n < base.size(); ++n)
        assert(!decode(base.data(), n, restored));
    for (int f = 0; f < fields; ++f) {
        assert(!set(r, f, NAN));
        assert(set(r, f, -1e9f) && r.v[f] == minimum[f]);
        assert(set(r, f, 1e9f) && r.v[f] == maximum[f]);
    }
    r = {};
    const auto before = encode(r);
    mutate(r, 0);
    randomCurve(r, 0, -12, 12, 0, true);
    assert(encode(r) == before);
    mutate(r, .7f);
    for (int i = 0; i < fields; ++i)
        if (r.locks & (1 << i))
            assert(r.v[i] == defaults[i]);
    assert(valid(r));
    r.scale = (1 << 0) | (1 << 3) | (1 << 7);
    randomCurve(r, 0, -24, 24, 1, true);
    assert(r.curves[0].p[0].value == 0 && r.curves[0].p[7].value == 0);
    for (int i = 1; i < 7; ++i)
        assert(r.scale & (1 << ((int(r.curves[0].p[i].value) + 120) % 12)));
    assert(editPoint(r, 0, 3, 0, 2) && r.curves[0].p[3].time < r.curves[0].p[4].time);
    r.curves[1].p[2].time = .5f;
    r.curves[1].p[3].time = std::nextafter(.5f, 1.f);
    r.curves[1].p[4].time = std::nextafter(r.curves[1].p[3].time, 1.f);
    assert(editPoint(r, 1, 3, 0, 0) &&
           valid(r)); // Malformed/very tightly spaced recipe: bounded, no clamp UB.
    r = {};
    r.v[length] = .1f;
    int progress = 0;
    assert(!render(
               r, 16000, a, [] { return true; }, [&](unsigned) { ++progress; }) &&
           progress == 0);
    r.v[loop] = 1;
    r.v[drift] = 0;
    r.v[evolution] = 4;
    assert(draw(r, a));
    const float seam = std::abs(a.left.front() - a.left.back());
    float maximumDelta = 0;
    for (std::size_t i = 1; i < a.left.size(); ++i)
        maximumDelta = std::max(maximumDelta, std::abs(a.left[i] - a.left[i - 1]));
    assert(seam < maximumDelta + .001f && a.left.size() == 1600);
    for (int seed = 0; seed < 12; ++seed) {
        r.seed = seed;
        r.v[width] = seed % 2;
        r.v[points] = 1 + seed * 2;
        r.v[evolution] = 1 + seed;
        r.v[root] = 24 + seed * 6;
        r.v[drift] = 4;
        assert(draw(r, b));
        for (float x : b.left)
            assert(std::isfinite(x) && std::abs(x) <= .892f);
    }
    // High notes retain no partials above the selected band limit.
    Table wave;
    makeTable(wave, 2, 1, 32, 1, 3);
    std::array<Plan::Complex, Plan::size> bins{};
    for (std::size_t i = 0; i < wave.size(); ++i)
        bins[i].real = wave[i];
    Plan::get().fft(bins);
    float rejected = 0, allowed = 0;
    for (std::size_t i = 1; i < Plan::size / 2; ++i)
        (i <= 3 ? allowed : rejected) += bins[i].real * bins[i].real + bins[i].imag * bins[i].imag;
    assert(rejected < allowed * 1e-10f);
    std::puts("Spline core: reproducible stereo, root pitch, bounded mutation/scale curves, "
              "metadata corruption, band limiting, cancellation and loop/one-shot edges: PASS");
}
void waitSynth(ThruInstance &t, bool ready = true) {
    // This waits for cancellable offline rendering and durable source I/O,
    // not a real-time processing deadline. Allow a busy host to finish.
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    do {
        renderSplit(t, 1);
        // Legacy codec regressions still compare exact old WAV rebuilds.
        // First verify the public restore migrated to TABLE, then explicitly
        // reopen only the retained internal renderer for that comparison.
        if(ready && !t.synthOpened && t.tableRecipeType(t.synthRecipe.type)
            && !std::strcmp(t.synthMessage.data(),"LOADED TO TABLE")
            && t.synthWorker->state.load()==s3g_thru_synth::Worker::idle){
            assert(t.controls.algorithm==Engine::PlaybackAlgorithm::table);
            t.synthOpened=true;t.synthEdited();
        }
        if(ready && !t.synthOpened && t.fmRecipeType(t.synthRecipe.type)
            && !std::strcmp(t.synthMessage.data(),"LOADED TO FM")
            && t.synthWorker->state.load()==s3g_thru_synth::Worker::idle){
            assert(t.controls.algorithm==Engine::PlaybackAlgorithm::fm);
            t.synthOpened=true;t.synthEdited();
        }
        if(ready && !t.synthOpened && t.noiseRecipeType(t.synthRecipe.type)
            && !std::strcmp(t.synthMessage.data(),"LOADED TO NOISE")
            && t.synthWorker->state.load()==s3g_thru_synth::Worker::idle){
            assert(t.controls.algorithm==Engine::PlaybackAlgorithm::noise);
            t.synthOpened=true;t.synthEdited();
        }
        if(ready && !t.synthOpened && t.resonRecipeType(t.synthRecipe.type,int(t.synthRecipe.drums[s3g_thru_synth::dModel]))
            && !std::strcmp(t.synthMessage.data(),"LOADED TO RESON")
            && t.synthWorker->state.load()==s3g_thru_synth::Worker::idle){
            assert(t.controls.algorithm==Engine::PlaybackAlgorithm::reson);
            t.synthOpened=true;t.synthEdited();
        }
        if(ready && !t.synthOpened && t.partialsRecipeType(t.synthRecipe.type)
            && !std::strcmp(t.synthMessage.data(),"LOADED TO PARTIALS")
            && t.synthWorker->state.load()==s3g_thru_synth::Worker::idle){
            assert(t.controls.algorithm==Engine::PlaybackAlgorithm::partials);
            t.synthOpened=true;t.synthEdited();
        }
        usleep(500);
        if(ready && t.synthOpened && !t.synthReady() && !t.synthWantsRender && t.synthWorker->state.load()==s3g_thru_synth::Worker::idle)
            setParam(&t,"synth_render","1"); // Simulate the explicit Render button.
        if (t.synthWorker->state.load() == s3g_thru_synth::Worker::idle &&
            (!ready || t.synthReady()) && !t.synthWantsRender &&
            (!t.synthSessionStore || t.synthSessionStore->idle()))
            return;
    } while (std::chrono::steady_clock::now() < limit);
    std::fprintf(stderr, "Synth timeout ready=%d state=%d msg=%s\n", t.synthReady(),
                 t.synthWorker->state.load(), t.synthMessage.data());
    std::fprintf(stderr,"Synth job type=%d op=%d progress=%u rate=%f source=%s impulse=%s\n",t.synthWorker->request.recipe.type,int(t.synthWorker->request.op),t.synthWorker->progress.load(),t.synthWorker->request.rate,t.synthWorker->request.source.data(),t.synthWorker->request.impulse.data());
    std::fprintf(stderr,"Synth recipe model=%g length=%g eventMode=%g eventCount=%g\n",t.synthWorker->request.recipe.drums[s3g_thru_synth::dModel],t.synthWorker->request.recipe.v[s3g_thru_synth::length],t.synthWorker->request.recipe.events[s3g_thru_synth::eMode],t.synthWorker->request.recipe.events[s3g_thru_synth::eCount]);
    assert(false);
}
void testSplineWorkflow() {
    using namespace s3g_thru_synth;
    char folder[] = "/tmp/thru-spline.XXXXXX";
    assert(mkdtemp(folder));
    auto t = std::make_unique<ThruInstance>();
    assert(t->prepare(folder));
    t->stopBackgroundWork();
    std::snprintf(t->synthDirectory.data(), t->synthDirectory.size(), "%s/captures/Synths", folder);
    const auto dirty = t->persistentRevision;
    setParam(t.get(), "synth_open", "1");
    setParam(t.get(), "synth_type", "0");
    assert(t->synthOpened);
    setParam(t.get(), "synth_edit", "0,0.12");
    setParam(t.get(), "synth_edit", "8,0");
    waitSynth(*t);
    assert(t->persistentRevision == dirty && !t->engine.laneLoaded(0));
    const auto noChange = t->synthRevision;
    setParam(t.get(), "synth_edit", "6,4");
    setParam(t.get(), "synth_curve_random", "0,-12,12,0,1");
    assert(t->synthRevision == noChange && !t->synthWantsRender);

    std::array<char, 8192> json{};
    const int count = getParamValue(t.get(), "synth_state", json.data(), json.size());
    assert(count > 0 && count < 3800);
    if (const char *fixture = std::getenv("S3G_THRU_SYNTH_FIXTURE")) {
        std::ofstream file(fixture);
        file.write(json.data(), count);
    }
    // Editing pads cannot trigger normal notes, Sequence trigs or Bank recording.
    for (int mode = 0; mode < 7; ++mode) {
        t->setPerformanceMode(mode);
        const std::uint8_t on[]{0x90, 92, 100}, off[]{0x80, 92, 0}, pressure[]{0xa0, 92, 80};
        dispatchMidi(t.get(), on, 3, MOVE_MIDI_SOURCE_INTERNAL, false);
        dispatchMidi(t.get(), pressure, 3, MOVE_MIDI_SOURCE_INTERNAL, false);
        dispatchMidi(t.get(), off, 3, MOVE_MIDI_SOURCE_INTERNAL, false);
        for (const auto &v : t->voices)
            assert(!v.held);
        assert(!t->sequencePads && t->bankRecordSlot < 0);
    }
    t->setPerformanceMode(0);
    setParam(t.get(), "synth_preview", "1");
    renderSplit(*t, 8);
    float audible = 0;
    for (float x : t->directPreview[0])
        audible = std::max(audible, std::abs(x));
    assert(audible > .01);
    setParam(t.get(), "panic", "1");
    renderSplit(*t, 16);
    assert(!t->synthPreviewWanted && t->synthPreviewGain == 0);
    const auto position = t->synthPosition;
    renderSplit(*t, 2);
    assert(position == t->synthPosition);
    setParam(t.get(), "synth_preview", "1");
    renderSplit(*t, 3);
    const auto old = t->synthActive;
    const auto original = t->synthWorker->audio[old].left;
    setParam(t.get(), "synth_edit", "6,14");
    setParam(t.get(), "synth_edit", "6,19");
    setParam(t.get(), "synth_render", "1");
    waitSynth(*t);
    assert(t->synthWorker->audio[old].left == original &&
           t->synthWorker->audio[t->synthActive].recipe.v[points] == 19);
    setParam(t.get(), "synth_edit", "0,12");
    setParam(t.get(), "synth_edit", "7,1");
    setParam(t.get(), "synth_render", "1");
    renderSplit(*t, 1);
    setParam(t.get(), "synth_edit", "0,0.12");
    setParam(t.get(), "synth_edit", "7,8");
    waitSynth(*t);
    assert(t->synthWorker->audio[t->synthActive].left.size() ==
           std::size_t(std::llround(.12f * t->sampleRate)));
    setParam(t.get(), "synth_edit", "6,7");
    setParam(t.get(), "synth_save", "1");
    assert(std::strstr(t->lastError.data(), "Render current"));
    waitSynth(*t);
    setParam(t.get(), "record_lane", "2");
    setParam(t.get(), "synth_name", "My Spline");
    storageRenderThread = pthread_self();
    storageTrace.clear();
    storageFailAt = 0;
    s3g_thru_storage::testHook = storageHook;
    setParam(t.get(), "synth_save", "1");
    setParam(t.get(), "record_lane", "3");
    waitSynth(*t, false);
    for (int i = 0; i < 5000 && !t->engine.laneLoaded(1); ++i) {
        renderSplit(*t, 1);
        usleep(500);
    }
    t->stopSampleWorker();
    s3g_thru_storage::testHook = nullptr;
    assert(t->engine.laneLoaded(1) && !t->engine.laneLoaded(0) && !t->engine.laneLoaded(2));
    assert(!storageTrace.empty());
    for (const auto &event : storageTrace)
        assert(!event.renderThread);
    const std::string saved = t->synthLastSaved.data();
    assert(saved.find("/My Spline.wav") != std::string::npos && std::filesystem::exists(saved));
    assert(t->engine.laneFrames(1) == std::size_t(std::llround(.12f * t->sampleRate)));
    setParam(t.get(), "synth_save", "0");
    waitSynth(*t, false);
    assert(saved != t->synthLastSaved.data() && std::filesystem::exists(saved) &&
           std::filesystem::exists(t->synthLastSaved.data()));
    setParam(t.get(), "record_lane", "2");
    const auto encoded = encode(t->synthRecipe);
    setParam(t.get(), "synth_edit", "6,3");
    waitSynth(*t);
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    assert(encode(t->synthRecipe) == encoded && std::string(t->synthName.data()) == "My Spline");
    // Every durable publication failure leaves no final or temporary file.
    for (int failed = 1; failed <= 3; ++failed) {
        const std::string name = "Fault" + std::to_string(failed);
        setParam(t.get(), "synth_name", name.c_str());
        storageFailAt = failed;
        storageTrace.clear();
        s3g_thru_storage::testHook = storageHook;
        setParam(t.get(), "synth_save", "0");
        waitSynth(*t, false);
        s3g_thru_storage::testHook = nullptr;
        assert(!std::filesystem::exists(std::string(folder) + "/captures/Synths/" + name + ".wav"));
    }
    storageFailAt = 0;
    const auto originalClip = t->activeClip;
    t->initializeClipIfNeeded(1);
    setParam(t.get(), "synth_save", "1");
    t->activeClip = 1;
    waitSynth(*t, false);
    assert(std::strstr(t->synthMessage.data(), "CLIP CHANGED") && !t->synthLoadPath[0]);
    t->activeClip = originalClip;
    for (const auto &entry :
         std::filesystem::directory_iterator(std::string(folder) + "/captures/Synths"))
        assert(entry.path().filename().string()[0] != '.');
    setParam(t.get(), "synth_edit", "0,12");
    setParam(t.get(), "synth_edit", "7,1");
    setParam(t.get(), "synth_render", "1");
    renderSplit(*t, 1);
    setParam(t.get(), "synth_cancel", "1");
    waitSynth(*t, false);
    assert(!t->synthReady());
    setParam(t.get(), "synth_close", "1");
    assert(!t->synthOpened && !t->synthPreviewWanted);
    // Project export keeps the complete WAV, including its editable recipe.
    t->captureActiveClip(true);
    assert(t->savePersistentState() && t->exportProjectFile());
    mz_zip_archive zip{};
    assert(mz_zip_reader_init_file(&zip, t->projectOutputPath.data(), 0));
    std::size_t bytes = 0;
    auto *asset = mz_zip_reader_extract_file_to_heap(&zip, "assets/a000000.wav", &bytes, 0);
    assert(asset && bytes > recipeBytes);
    const auto *data = static_cast<const unsigned char *>(asset);
    const unsigned char *chunk = nullptr;
    for (std::size_t i = 12; i + 8 + recipeBytes <= bytes; ++i)
        if (!std::memcmp(data + i, "sgSy", 4)) {
            chunk = data + i + 8;
            break;
        }
    Recipe portable;
    assert(chunk && decode(chunk, recipeBytes, portable) && encode(portable) == encoded);
    mz_free(asset);
    mz_zip_reader_end(&zip);
    const std::string archive = t->projectOutputPath.data();
    t.reset();
    std::filesystem::remove_all(std::string(folder) +
                                "/captures"); // Original generated WAVs are gone.
    const std::string destination = std::string(folder) + "/restored";
    std::filesystem::create_directory(destination);
    t = std::make_unique<ThruInstance>();
    assert(t->prepare(destination.c_str()));
    t->stopBackgroundWork();
    assert(t->savePersistentState());
    t->projectGeneration = t->reservePersistenceGeneration();
    std::snprintf(t->projectArchivePath.data(), t->projectArchivePath.size(), "%s",
                  archive.c_str());
    assert(t->importProjectFile() && t->savePersistentState());
    t.reset();
    t = std::make_unique<ThruInstance>();
    assert(t->prepare(destination.c_str()));
    t->stopBackgroundWork();
    for (int i = 0; i < 12000 && (!t->engine.laneLoaded(1) || t->sampleLoadBusy() ||
                                  t->clipPreparationInFlight());
         ++i) {
        renderSplit(*t, 1);
        usleep(500);
    }
    assert(t->engine.laneLoaded(1));
    setParam(t.get(), "record_lane", "2");
    setParam(t.get(), "synth_open", "1");
    setParam(t.get(), "synth_type", "0");
    setParam(t.get(), "synth_restore", "1");
    waitSynth(*t);
    assert(encode(t->synthRecipe) == encoded);
    t.reset();
    std::filesystem::remove_all(folder);
    std::puts("Spline workflow: isolated tool pads, immutable/coalesced renders, preview, captured "
              "Save & Load lane, atomic unique WAVs, disk faults, cancellation, recipe recall and "
              "project export/import without original assets: PASS");
}
#include "synth_legacy_fixture.h"
#include "test_synth_generators.inc"
#include "synth_v3_fixture.h"
#include "test_synth_workshop.inc"
#include "test_synth_fields.inc"
#include "test_synth_session.inc"
#include "test_synth_audition.inc"
void testSpline() {
    testSynthSessionStore();testSynthManualSession();testSynthDrumAudition();
    testSplineCore();
    testSplineWorkflow();
    testSynthGeneratorsCore();
    testSynthGeneratorWorkflow();
    testSynthWorkshopCore();
    testSynthWorkshopWorkflow();
    testSynthFieldsCore();
    testSynthFieldsWorkflow();
}
