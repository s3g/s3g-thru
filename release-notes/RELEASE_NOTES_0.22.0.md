# s3g Thru 0.22.0

## Glitch Buffer Lock

- Adds a live Buffer Lock for the independent rolling history inside every
  Glitch voice.
- Hold Shift+Loop for a temporary lock and release to resume live capture.
- Tap Shift+Loop twice quickly to latch the lock; press the chord once while
  latched to return to live history.
- Keeps plain Loop dedicated to Stack Hold, so lane motion and Glitch history
  capture remain independent performance gestures.
- Shows an orange Loop key, a `LOCK` waveform-header tag, a locked footer, and
  an `LCK` parameter-page prefix while the histories are frozen.

## Voice behavior

- Pattern, stack motion, pitch, and effects continue while history is locked.
- A voice triggered during a lock fills its own complete 8,192-frame history
  before freezing. At 48 kHz this is about 171 ms and prevents an empty or
  unusably short repeat buffer.
- Locking and unlocking do not allocate memory or copy buffers in the audio
  callback.
- Locked history remains deliberately ephemeral. Resample records the audible
  result into a persistent lane when required.

## Compatibility

- Buffer Lock is a live performance state rather than a stored clip parameter,
  so the state schema and clip-bank format remain unchanged.
- Existing engine IDs, parameter IDs, MIDI mappings, sample assignments, and
  saved performance clips are unchanged.

## Verification

Native DSP/ABI tests, ASan/UBSan, UI gesture tests, persistence/export tests,
documentation rendering/link checks, AArch64 build validation, and package
verification must pass before installation. Move hardware remains the final
authority for gesture timing, LED behavior, transitions, and realtime margin.
