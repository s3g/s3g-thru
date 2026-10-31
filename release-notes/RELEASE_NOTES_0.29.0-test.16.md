# s3g Thru 0.29.0-test.16

Local testing candidate.

- Added Imprint as a Morph mode: the next lane’s spectral envelope articulates the carrier while keyboard pitch and stereo phase remain with the carrier. MODE is knob 6; knob 1 becomes IMPR strength and FREQ is reserved. Offset, Blur and Pitch remain active. Mode has per-part/per-clip recall, performance-data automation and MIDI CC 45.

- Chain-page Copy / Shift+Copy now duplicates the recorded clip rather than copying a whole chain bank or only its reference. Paste chooses an unused clip slot, assigns it to the destination entry, and copies the source entry’s repeats/fade. Recorded notes, takes, sound programs, paths, cues and sample assignments are independent; existing audio files remain shared references.
- Copy+Step selects a source; add Shift while keeping Copy held and press a destination Step to paste. Each Step reads the current Shift state. Confirmation names the clip and destination. Whole-chain and reference-only entry copies remain on Chain Tools.
- Undo restores the destination and reclaims an unchanged, unused duplicate. Slots already referenced by any chain are reserved; a full bank reports No unused clip slots.
- Pattern fade timing, manual interpolation and DONE continue advancing if the outgoing audio buffer underruns. A starved outgoing bus no longer pins the incoming engine at zero gain. The outgoing audio worker now uses normal priority ahead of lower-priority file/analysis jobs.
- Spectral lookup now accounts for each source’s sample rate, including carrier/shaper combinations. Added 22.05/48 kHz pitch checks.
- Added eight-note switching checks across all 12 engines and four instrument modes. No persistent engine mute was reproduced in these host tests. An old clip’s restored algorithm state may need reselection; hardware sound/performance checks remain necessary.

The FADE BUF MISSES counter records outgoing crossfade-buffer underruns for the session, including earlier transitions. A fixed historical count does not mean a current fault. Sets now save in format 21; formats 1–20 remain readable and old clips default to Morph mode. Existing parameter IDs are unchanged; the new Morph Mode is appended. Back up sets before testing: older builds cannot read a set saved in format 21.
