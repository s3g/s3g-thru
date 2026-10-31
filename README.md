# s3g Thru

s3g Thru is a four-lane stereo sampler and performance-data looper for
[Ableton Move](https://www.ableton.com/move/). It runs as a
[Schwung](https://github.com/charlesvestal/schwung) takeover module, with thirteen playback engines and eight
polyphonic voices. Single uses one four-lane stack; Split and Shared Dual offer
two independent instruments; Quad offers four instruments with one lane each.

Read the illustrated [s3g Thru manual](https://s3g.github.io/s3g-thru/) for the
complete control map, recording workflow, playback-engine reference, effects,
performance loops, and shortcuts.

## Install with Schwung

1. Install Schwung 1.7.3 or newer on Move.
2. Open Schwung Manager at `http://move.local:7700`.
3. Choose **Modules → Install Custom Module → GitHub URL**.
4. Enter `https://github.com/s3g/s3g-thru` and install.
5. Restart Move when Schwung Manager requests it.
6. On Move, open Overtake and select **s3g Thru**.

The installer reads [`release.json`](release.json) and downloads the matching
ARM64 package from [GitHub Releases](https://github.com/s3g/s3g-thru/releases).

For on-device help, leave takeover, hold **Shift**, touch the volume knob and
press **Step 2** for Global Settings. **Shift + main encoder click** opens its
section picker: choose **System → Help → Modules → s3g Thru**. Inside Thru,
**Shift + Menu** opens the diagnostic reports and shortcut reminder.

## Build from source

Requirements are Node.js 18 or newer, a native C++17 compiler for tests, and an
AArch64 Linux C++ cross-compiler for the Move package.

On macOS:

```sh
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu
npm test
npm run build
```

On Linux, use an AArch64 toolchain with a glibc 2.34-or-earlier sysroot. The
pinned toolchain and checksum used for official packages are documented in
[the release workflow](.github/workflows/release.yml). A current distribution’s
default cross-compiler may target a newer glibc and will be rejected by the
Move ABI check.

The installable is written to `dist/s3g-thru-module.tar.gz`. Run
`npm run docs:serve` to preview the manual locally and `npm run release:check`
before creating a version tag.

## AI assistance

s3g Thru is developed with AI assistance for code and documentation. Instrument
design draws largely on s3g’s prior work in environments such as Max/MSP. s3g
directs design, hardware testing, and release decisions, conducts code review,
and edits documentation.

AI-assisted content may contain errors and bugs. Please keep this in mind for
your intended use. Functionality may change in new releases.

## License

The software is provided **“as is,” without express or implied warranties**,
under the BSD 3-Clause [LICENSE](LICENSE). See the license for the full warranty
disclaimer and limitation of liability, and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for third-party notices.
