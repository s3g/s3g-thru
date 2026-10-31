# s3g Thru 0.29.0-test.20 — on-device quick reference

Local testing build following test.19, including its rendering optimizations and timing reports.

- **On-device help:** Schwung's Help viewer now includes s3g Thru topics for getting sound, samples, lanes and engines, instrument modes, performance loops, and shortcuts. After leaving takeover, open Global Settings → System → Help → Modules → s3g Thru.
- **Readable topics:** short ASCII lines fit the help viewer's 128-pixel display. The package checks its topic structure, supported characters and text width.
- **Existing controls:** Shift+Menu inside Thru continues to open System Report and Shortcuts. This build adds the standard Schwung help file; the playing controls and diagnostic shortcuts keep their functions.
- **Packaging:** the installable includes `help.json` alongside the module UI and DSP. The module description now lists all thirteen playback engines.

Set format remains **23**, with the same 205 stored parameters. DSP processing is unchanged from test.19; the reported Move timing spike still needs hardware verification.
