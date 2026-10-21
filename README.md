# s3g Thru

s3g Thru is a four-lane stereo sampler and performance-data looper for
[Ableton Move](https://www.ableton.com/move/). It runs as a
[Schwung](https://github.com/charlesvestal/schwung) takeover module, with eight
polyphonic voices that can move independently through recorded or loaded sample
lanes.

Read the illustrated [s3g Thru manual](https://s3g.github.io/s3g-thru/) for the
complete control map, recording workflow, playback-engine reference, effects,
performance loops, and shortcuts.

## Install with Schwung

1. Install Schwung 1.7.3 or newer on Move.
2. Open Schwung Manager at `http://move.local:7700`.
3. Choose **Modules → Install Custom Module → GitHub URL**.
4. Enter `https://github.com/s3g/s3g-thru` and install.
5. On Move, open Overtake and select **s3g Thru**.

The installer reads [`release.json`](release.json) and downloads the matching
ARM64 package from [GitHub Releases](https://github.com/s3g/s3g-thru/releases).

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

On Debian or Ubuntu:

```sh
sudo apt-get install g++-aarch64-linux-gnu file
npm test
npm run build
```

The installable is written to `dist/s3g-thru-module.tar.gz`. Run
`npm run docs:serve` to preview the manual locally and `npm run release:check`
before creating a version tag.

## License

BSD 3-Clause. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
