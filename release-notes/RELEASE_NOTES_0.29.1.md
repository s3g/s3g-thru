# s3g Thru 0.29.1

This patch includes all 0.29.0 features below and fixes Linux source builds.
An explicit export list keeps the Move module entry point as the only public
symbol, including with the GCC 10 toolchain used by GitHub Actions. Instrument
DSP, controls and saved-set format are unchanged from 0.29.0.

Four new playback engines, editable stack paths, pattern crossfades, and saved
chains expand how you play and arrange the four-lane stereo instrument.

## Four new engines

- **Prism** resynthesizes spectral frames with scanning, freeze, amplitude or
  frequency hold, accumulation, blur, tilt, smear, and keyboard transposition.
- **Morph** blends spectral amplitudes and frequencies with the next source
  lane. **Imprint** uses that lane's changing spectral envelope to articulate
  the carrier. Quad pairs lanes 1 → 2 → 3 → 4 → 1; Split stays within each pair.
- **Texture** generates grouped sample phrases with Spray, Grouped, Motif and
  Ornament modes, scale-aware intervals, timing scatter and a repeatable seed.
- **Formant** provides pitch-synchronous sampling: transpose pitch and resonant
  character independently, tune voiced material toward played notes, freeze a
  source position across notes, add formant key tracking, pulse roughness and
  an octave-below component.

There are now thirteen engines. Each retains per-part settings and the existing
voice budgets in Single, Split, Shared Dual and Quad. Source preparation runs
in the background. Wait for SPECTRAL PREPARING or PITCH PREPARING to finish;
noisy or polyphonic sources can give Formant a grainier character.

## Stack paths and live performance

- Choose **DRAW** on Stack Path and click the main encoder to edit four to eight
  points. Time, lane position and curvature support pauses, uneven travel and
  repeated lane visits. Each voice keeps its independent phase.
- Hold **Capture** for temporary bottom-row controls: **L1–L4, DARK, CRSH, ECHO,
  SPCE**. A lane pad grabs its lane fully; multiple pads blend by pressure.
  Effects respond to pressure, and releasing restores the current settings.
- Active grains and Motion packets follow lane grabs smoothly. Available lanes
  follow the focused part; Quad has no lane rides. ECHO/SPCE use that part's
  sends in multi modes and require audible shared return levels.
- Cues remember both source position and lane blend. Recall holds that blend;
  Stack Start can adjust it, and Loop resumes the scan.
- Mix / Rec can record **PT A–PT D** into the selected stereo lane. The source
  is captured after private processing and before part level, mute and Blend.
  Shared returns and Tape are excluded. Each completed take is archived.
- Parameter dials respond more slowly for finer adjustments; Chain Bank has
  additional protection against skipping several selections.

## Pattern fades and arrangements

- **Pattern Fade** crossfades complete performances, their samples, instrument
  settings and effect histories. Choose CUT or fades up to **30 seconds**, with
  fixed timing or a random duration between LOW and HIGH, plus five curves.
- Touch **XFADE** to hold the blend; turn to move manually in either direction.
  Both performances keep playing. Releasing touch leaves Hold on; main click
  resumes. DONE commits the destination; Shift + knob 8 returns to the source.
- Store **16 chains**, each with up to **64 entries**. Entries choose a clip,
  repeats or WAIT, and inherited or independent fade settings. Each chain can
  end with LOOP, STAY or STOP. The existing chain becomes Chain 01.
- Browse and edit another chain while the current one plays. Queue a chosen
  entry with CUE HERE or Shift + NEXT; cancel an explicit queue without stopping.
- Chain Tools copy whole arrangements or entry references, including fade
  settings. The curved-arrow Undo key covers supported chain edits.
- **Copy / Shift+Copy** in clip and chain views duplicates the recorded clip.
  In Chain, paste creates an independent clip in an unused slot and assigns it
  to the destination entry. Audio files remain shared references. Hold Copy
  and press Step to choose a source; add Shift and press Step to paste.
- **Performance Loop** and Overdub Takes are shared across all parts. Per-part
  pressure behavior is on Pads & Scale. Overdub remains an undoable take;
  finish recording, then use Delete + Record to remove the newest pass.

## Help, sound continuity and timing

- **24 on-device help topics** cover playing, recording, engines, modes, loops
  and shortcuts. After leaving takeover, open Schwung Global Settings with
  Shift + volume touch + Step 2. Shift + main click opens the section picker;
  choose System → Help → Modules → s3g Thru.
- **Shift + Menu** inside Thru retains System Report and Shortcuts. The new
  **Render Timing 3/4** page shows CPU PEAK, WALL PEAK, WAIT PEAK and CPU MISSES.
  Shift + Delete resets peaks and counters while a report view is open.
- Prism, Morph, Imprint and Formant avoid repeated rendering work without
  reducing spectral resolution or voice limits. Work-miss counting now uses
  full-resolution timing rather than rounded display values.
- Rotary delay readers now wrap safely at fractional buffer boundaries in both
  Single and per-part effects, preventing a rare invalid sample read.
- A starved outgoing pattern-fade buffer no longer holds the destination silent
  or prevents a transition from completing. FADE BUF MISSES records its history.
- All engines, voices and effects share Move's processor. CPU headroom depends
  on the combination played; use fresh timing counters when testing dense sounds.

## Upgrading

- **Export a set backup before upgrading if you may need to return to 0.28.0.**
  This release reads earlier set formats and saves format **23**. Older releases
  cannot read sets saved by 0.29.0. Sample WAV archives remain available.
- Existing engine and parameter IDs remain stable. Older sets receive defaults
  for new controls. The source-voice total remains eight.
- Schwung **1.7.3 or newer** remains the declared minimum. Install or update in
  Schwung Manager using `https://github.com/s3g/s3g-thru`.
- **Shift + Capture remains Schwung Skipback.** Open FX Sends through
  Capture + Menu → FX SENDS.

See the illustrated [s3g Thru manual](https://s3g.github.io/s3g-thru/) for all
thirteen engine guides, arrangement controls, effects, recording and shortcuts.
