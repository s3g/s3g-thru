# s3g Thru 0.30.0-test.43

Local native E16 discrete-control candidate. The public GitHub installer remains
at 0.29.1.

- Makes every bounded integer and choice on the script-free native E16 pages
  advance one value per detent after safe pickup alignment. This includes
  Wavesets PROC, ALG, PATH, MODE and effect algorithms.
- Returns priority feedback after each discrete step so the E16 ring aligns to
  the new canonical value, while still accepting fast successive detents before
  feedback arrives. Continuous parameters retain full 14-bit resolution.
- Repairs pickup storage for shared/global native aliases such as Reverb. Their
  global part scope can no longer address outside the A/B pickup array.
- Adds a regression sweep across the bounded integer controls exposed by every
  adaptive engine page, plus explicit Wavesets PROC and shared Reverb coverage.
- Retains test.42 startup protection, consistent group page fractions and the
  four-bank Pages hub.
