# s3g Thru 0.25.1

## Move compatibility fix

- Rebuilds the native module with a toolchain whose glibc requirements are
  compatible with Ableton Move. The 0.25.0 GitHub release accidentally linked
  against symbols newer than Move's glibc, so its DSP could not load even
  though the interface appeared.
- Restores sample loading, live stereo recording, capture recall, and playback
  for installations made from the GitHub URL.
- Adds a release gate that rejects any future `dsp.so` requiring a glibc
  version newer than Move's glibc 2.34.

The instrument format and saved data are unchanged from 0.25.0. Existing
recordings and clip assignments remain compatible.
