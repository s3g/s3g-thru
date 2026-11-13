# s3g Thru 0.31.1

This update focuses on playback stability, more consistent sequencing and smoother editing.

- **Sequence display timing:** more reliable green pad playheads at faster rates, with less overhead when changing lanes, rates or editor pages.
- **Smoother editing and saving:** editor snapshots, UI polling and background saving place less work on the audio callback, reducing interruptions during performance.
- **More resilient clip fades:** buffered background rendering, faster clip handoffs and lower effects overhead improve timed transitions. PERF LOOP’s **LOOK** setting now defaults to **12ms**, adding about 12 ms to instrument playback for extra fade-start margin. Input monitoring keeps its current response. Saved choices, including Off, are retained; projects without a saved choice use 12ms.
- **Layers Engine Lists:** assign up to eight playback-engine entries to the Layers keyboard, using the existing ordering, random and keyboard-position options.
- **Clearer diagnostics and help:** updated timing reports distinguish audio rendering, callback work and fade-buffer readiness. The manual and on-device help cover the new controls and reports.

Existing projects, samples and MIDI parameter identities remain supported. Existing E16 and Monolit presets can stay in place; matching controller bundles are also included.

Install or update through Schwung Manager using `https://github.com/s3g/s3g-thru`, or download `s3g-thru-module.tar.gz` from this release. Requires Schwung 1.7.3 or newer.
