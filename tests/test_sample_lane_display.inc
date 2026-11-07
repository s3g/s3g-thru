// Exercise the real telemetry consumed by the waveform UI.
void testSampleLaneIndicators() {
    const auto checkSingle=[](ThruInstance& t,int selected) {
        char text[1024]{};assert(getParam(&t,"ui_state",text,sizeof(text))>0);
        const char* cursor=text;
        for(int field=0;field<11;++field) {
            char* end=nullptr;const float value=std::strtof(cursor,&end);assert(end!=cursor);
            if(field>=7)assert(value==(field-7==selected?1000:0));
            cursor=*end==','?end+1:end;
        }
        for(int lane=0;lane<4;++lane)
            assert(t.waveformLaneWeight(lane,t.displayVoice())==(lane==selected?1:0));
    };
    for(const auto algorithm:{Engine::PlaybackAlgorithm::sample,Engine::PlaybackAlgorithm::slices}) {
        auto t=splitFixture();t->setPerformanceMode(0);t->controls.algorithm=algorithm;
        t->controls.sampleMode=Engine::SampleMode::loop;
        t->controls.sampleRegionMode=Engine::RegionMode::equal;
        t->controls.manualLane=2;t->controls.laneCycles=16;t->controls.lanePath=Engine::LanePath::down;
        renderSplit(*t,40);noteMidi(*t,1,60);renderSplit(*t,32);
        auto* first=externalVoice(*t,0,60);assert(first&&first->variantLane==2);
        for(int block=0;block<12;++block) {
            renderSplit(*t,4);checkSingle(*t,2);
            assert(std::abs(first->engine.normalizedLanePosition()-2.f/3)<1e-6f);
        }
        // STAK affects the next note, not an already sounding reader.
        t->controls.manualLane=0;renderSplit(*t,8);checkSingle(*t,2);
        noteMidi(*t,1,61);renderSplit(*t,16);auto* last=externalVoice(*t,0,61);
        assert(last&&last->variantLane==0&&t->displayVoice()==last);checkSingle(*t,0);
        assert(std::abs(first->engine.normalizedLanePosition()-2.f/3)<1e-6f);
        noteMidi(*t,1,61,0);renderSplit(*t,16);checkSingle(*t,2);
        // Capture lane recall can move the actual reader away from the
        // cached note-on variant. Telemetry follows the committed reader.
        t->performanceLive[0][8]=4;t->setStackScanPaused(true);renderSplit(*t,32);
        assert(first->variantLane==2&&first->engine.normalizedLanePosition()==1);checkSingle(*t,3);
        noteMidi(*t,1,60,0);renderSplit(*t,32);checkSingle(*t,-1);

        // Shared Dual sums actual active lanes across both parts; neither
        // editing focus nor an unused scan phase changes this report.
        t=splitFixture();t->setPerformanceMode(2);
        for(int part=0;part<2;++part) {
            t->selectSplitPart(part,false);t->controls.algorithm=algorithm;
            t->controls.sampleMode=Engine::SampleMode::loop;t->controls.manualLane=part*2;
        }
        t->selectSplitPart(0,false);noteMidi(*t,1,60);noteMidi(*t,2,60);renderSplit(*t,60);
        first=externalVoice(*t,0,60);last=externalVoice(*t,1,60);assert(first&&last);
        assert(t->waveformLaneWeight(0,t->displayVoice())>.99f&&t->waveformLaneWeight(2,t->displayVoice())>.99f);
        t->performanceLive[0][8]=2;t->setStackScanPaused(true);renderSplit(*t,32);
        assert(first->variantLane==0&&std::abs(first->engine.normalizedLanePosition()-1.f/3)<1e-6f);
        assert(t->waveformLaneWeight(0,t->displayVoice())==0&&t->waveformLaneWeight(1,t->displayVoice())>.99f);
        assert(t->waveformLaneWeight(2,t->displayVoice())>.99f&&t->waveformLaneWeight(3,t->displayVoice())==0);
    }
    auto t=splitFixture();t->setPerformanceMode(0);t->controls.algorithm=Engine::PlaybackAlgorithm::motion;
    t->controls.lanePath=Engine::LanePath::manual;t->controls.manualLane=1.5f;
    noteMidi(*t,1,60);renderSplit(*t,100);
    assert(t->waveformLaneWeight(1,t->displayVoice())>0&&t->waveformLaneWeight(2,t->displayVoice())>0);
    std::puts("s3g Thru Sample/Slices lane indicators: stable actual lanes, latest held voice, per-voice markers, capture recall, Shared Dual and moving-engine isolation: PASS");
}
