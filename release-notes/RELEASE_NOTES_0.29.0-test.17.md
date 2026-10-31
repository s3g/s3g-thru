# s3g Thru 0.29.0-test.17 — Formant

Local testing build, following test.16. This package adds Formant as a thirteenth playback engine and retains the Imprint, independent recorded-clip duplication, and fade-buffer recovery changes from test.16.

- **Formant:** pitch-synchronous stereo sampling. Notes transpose the recording while Formant Shift changes its resonant character independently. Speed continues to control travel through the sample; stack controls remain available in scanning modes.
- **Small control page:** PIT (−24…+24 semitones), FORM (−24…+24 semitones), and SMTH (5–200 ms). Remaining positions are reserved. Waveform knob 7 adjusts FORM. MIDI CCs 40–42 address these controls on the selected part.
- **Background preparation:** pitch periods and marks are prepared from owned source snapshots, using the existing revision-safe analysis/cache lifecycle. No pitch search, allocation, or file operation runs in the audio renderer. A smooth granular fallback plays while preparation completes or when a region has no reliable pitch.
- **Live edits:** new grains adopt smoothed pitch/formant settings while existing grains finish their captured overlap windows. Both stereo channels share the same marks. Grain work is bounded.
- **Recall and performance capture:** all three parameters store per part, can be recorded as automation, and survive clip copying and recall. Single, Split, Shared Dual, and Quad use their existing voice budgets.
- **Manual:** a Formant guide, screen image, parameter table, starting points, and updated engine/navigation/MIDI references. Refreshed Prism, Morph, Imprint, and Texture screen illustrations show their actual controls.

## Sets and compatibility

Back up/export the set before testing. New saves use **format 22** (199 lock parameters); formats 1–21 remain readable. Older sets initialize Formant Pitch and Shift to zero and Smoothing to 30 ms. Existing engine and parameter IDs remain stable; Formant and its three parameters are appended. Older builds cannot read a format-22 set.

## Testing on Move

Start with a sung vowel or monophonic phrase, wait for PITCH PREPARING to disappear, and compare the root with the octave. Check FORM independently, slow stack travel, fast edits, release tails, eight-note playing and simultaneous effects. In Quad each part uses its own lane. Pitch detection targets roughly 55–1200 Hz; polyphonic/noisy inputs and extreme shifts can sound grainy. Pure tones may become quiet when their only strong partial is moved outside the preserved formant region. Host validation does not establish Move CPU headroom or audible acceptance.
