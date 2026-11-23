# s3g Thru 0.33.2

RESON stability improvements and gentler control of overlapping hits.

- **Steadier release tails:** released RESON voices keep their current stack blend, preventing decaying HIT tails from scanning back into a sustained TONE patch. Held notes continue normal stack scanning; TONE keeps its full ADSR.
- **New DENS control:** Capture + Menu → Play → Play Gain, knob 2 compresses the focused part’s combined RESON voices before effects. It works across all eight voices in Single mode and excludes other engines in mixed Engine Lists. Try 30–50%; zero bypasses it. Stereo balance and attack are retained, with no added latency or makeup gain.
- **Lighter, cleaner HIT tails:** finished percussion stops consuming synthesis work and no longer turns down other notes through polyphony compensation. Held keys retain their pad lights and Engine List positions until release.
- **Recall and recording:** DENS is saved per part with clips and projects, supports recorded gestures and MIDI, and survives copying and clip fades. Earlier projects open with DENS at zero. The manual and device help explain the new behavior.

Install or update through Schwung Manager using `https://github.com/s3g/s3g-thru`, or download `s3g-thru-module.tar.gz` from this release. Existing controller presets continue to work.

Projects saved in 0.33.2 require 0.33.2 or newer to reopen. Keep a [project backup](https://s3g.github.io/s3g-thru/projects.html) if you may need to return to an older version.
