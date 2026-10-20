# s3g Thru 0.21.0

## Glitch engine

- Adds Glitch as algorithm ID 6, preserving the six existing algorithm IDs.
- Gives every playback voice an independent bounded rolling history and repeat
  reader; no memory is allocated in the audio callback.
- Adds Repeat, Stutter, Scatter, Divide, and Override behaviors.
- Adds free or tempo clocking, musical divisions, buffer length, repeat count,
  seeded chance, and continuous buffer-override depth.
- Adds 1–16-step distributed pulse patterns with pulse count, rotation,
  history jump, pitch range, reverse chance, repeat decay, and deterministic
  seed. Odd pattern lengths can cycle against the performance loop without a
  conventional step editor.
- Reuses the shared Stack Path page so every Glitch voice can move through the
  four lanes before its private history is captured.

## Effects

- Adds one Effects page after Mix / Rec for every engine.
- Adds a lightweight post-voice stereo chain: bipolar tone shaping, soft
  drive, bit reduction, sample-rate reduction, and a 1–750 ms delay with up to
  90% feedback and variable stereo cross-feed.
- Defaults remain transparent. Delay Mix affects only the delay stage, so the
  color controls remain useful with a dry delay.
- Input Monitor stays dry. Browser Preview and Resample use the processed
  instrument path.
- All eight effect controls are clip-local and can be captured as continuous
  performance-loop gestures or overdub takes.

## Fidelity and direction

- Adds an engine-by-engine comparison against the current Sample-family DSP in
  `design/ENGINE_FIDELITY_AUDIT.md`.
- Records the highest-value next fidelity work: per-lane Lanes transport,
  direct-play voice behavior, manual Cutups patterns, and bounded Grains sync.
- Adds `design/S4_DIRECTION.md`, translating the useful ideas from the S-4
  study into Thru's own model: lockable live history, relative performance
  macros, temporary offsets, additive modulation recorders, conditional clip
  chains, and compact effect characters.

## Persistence and documentation

- Clip-bank format 4 appends Glitch and Effects base values while continuing
  to import formats 1, 2, and 3 with safe defaults for the new fields.
- The state JSON remains version 6 and treats all new values as optional, so
  existing state files load unchanged.
- Adds complete Glitch and Effects manual pages, generated Move-screen figures,
  practical starting points, and an expanded hardware acceptance pass.

## Verification

Native ABI/DSP tests, ASan/UBSan, UI tests, legacy clip-bank migration,
documentation rendering/link checks, AArch64 build validation, and package
verification must pass before installation. Move hardware remains the final
authority for realtime CPU margin, screen behavior, pad interaction, and audio
quality.
