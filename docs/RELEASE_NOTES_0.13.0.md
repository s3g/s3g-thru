# S3G Thru 0.13.0

This is the combined instrument-completion release. It deliberately collects
the sampling-reliability, composition, performance-capture, and maintenance
work in one state-format transition instead of spreading it across several
public versions.

## Sampling and project safety

- Pattern lanes now store explicit `INHERIT`, `FILE`, `CAPTURE`, or `EMPTY`
  ownership. `LOADING` and `MISSING` are reported as runtime states.
- Stereo recordings are written as pattern-local float WAV captures.
- A second complete four-lane bank prepares queued-pattern sources off the
  audio thread. All four lanes switch together at a safe boundary.
- Metadata and completed captures autosave through temporary files and atomic
  renames. Normal exit still performs a complete recovery-bank save.
- One-level Undo covers sequence-Step, pattern, lane-source, clear, and sample
  edit operations. Alternating edit captures preserve the preceding audio.
- The Sample page adds trim, reverse, normalization, and edge-fade actions.

## Sequencing and performance

- Patterns contain up to 64 Steps, presented as four 16-Step hardware pages.
- Explicit note-on/note-off boundaries can cross Step pages.
- Every Step stores probability, 1–4 ratchets, and bipolar microtiming.
- A 16-slot pattern chain stores pattern numbers and 1–16 repeats per slot.
- Automation Record writes on-device knob movement and external CC 20–45 into
  the existing parameter-lock system.
- A rolling free-play buffer can be committed with Shift + jog-click.
- Shift + Back invokes Undo.
- External MIDI CC 58–68 addresses the new Step, Capture, Undo, and Chain
  controls.

## Compatibility and diagnostics

- Pattern persistence is version 7. Versions 1–6 remain readable and migrate
  on save.
- Existing module ID, component type, parameter IDs, playback engines, voice
  allocation, and v0.12.0 library paths remain compatible.
- Help reports the maximum render duration and audio-deadline overrun count.
- The active bank, prepared bank, and decoder staging allocation are reported
  through `memory_state` for device diagnostics.

## Release gate

Native, sanitizer, UI, AArch64 build, and package verification must pass before
packaging. The exact tarball must then pass `docs/HARDWARE_TEST.md` on Move,
with particular attention to the approximately 37 MiB sample allocation,
eight dense Grains voices, long-WAV pattern preparation, autosave, and repeated
overtake open/close cycles.
