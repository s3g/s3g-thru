# Third-party notices

## Schwung plugin ABI

The minimal ABI declaration in `include/thru_schwung_abi.h` follows the
MIT-licensed Schwung `plugin_api_v1.h` structure prefix.

- Project: https://github.com/charlesvestal/schwung
- Copyright: Schwung contributors
- License: MIT

## s3g Sample Motion engine

`include/sample_motion_app.h` is the fixed-memory engine developed in the
s3g Daisy firmware repository and is redistributed under this repository's
BSD-3-Clause license.

- Source repository: `s3g-daisy`
- Source path: `firmware/common/sample_motion_app.h`
- Imported source SHA-256:
  `73385c00a02d05a3af1dea74ced8c60ff57261c2e87efc6d0c6f82c11492f2d7`


## Reverb design reference

`include/thru_reverb.h` is an original BSD-3-Clause implementation. Its Plate
algorithm follows the network topology and delay-time constants in Jon Dattorro,
“Effect Design, Part 1: Reverberator and Other Filters”, JAES 45(9), 1997.
The Room algorithm uses a four-line orthogonal feedback-delay network.

Reference: https://www.convexoptimization.com/TOOLS/EffectDesignPart1.pdf

## miniz ZIP archive library

`include/vendor/miniz` contains the upstream miniz 3.1.2 amalgamation, used
for portable `.thru` project archives.

- Source: https://github.com/richgel999/miniz/releases/tag/3.1.2
- Source archive SHA-256: `f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a`

Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
