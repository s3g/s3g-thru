# s3g Thru 0.30.0-test.62 — Portable projects and save recovery

Local testing build; not published.

- Capture + Menu → Projects exports a `.thru` ZIP64 project containing all
  clips, chains, settings, slice edits and referenced lane/Bank/library audio.
- Import stages and validates the archive, creates a BeforeImport backup and
  activates a separate project folder when takeover is reopened. Previous
  projects remain intact; destination MIDI settings and controller maps stay.
- Exported files live in Samples/Thru Projects. Download/upload through
  Schwung Manager's Files page. Back cancels archive work.
- File contents and directory entries flush before successful save indicators.
  Interrupted metadata rotations retain a coherent recoverable generation.
- Failed Bank takes have Retry, Keep Waiting and confirmed Discard. Normal
  exits wait for pending audio; shutdown retries retained audio once more.
- Includes the Custom MIDI and Bank safeguards from test.60–61. Existing clip
  format 36, state 19, MIDI IDs and controller scene files are unchanged.

Move/Schwung export/import, Manager transfer, recording and listening tests
remain to be completed before publication.
