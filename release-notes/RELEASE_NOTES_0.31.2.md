# s3g Thru 0.31.2

This stability update improves mode selection, setting recall and Pulsar playback consistency.

- **More reliable mode selection:** improves confirmation handling when changing modes in quick succession.
- **Better setting recall:** switching modes retains each part’s settings and preferred stack position, including positions temporarily outside a mode’s lane range.
- **Repeatable Pulsar performances:** clips save variation seeds per part. Recorded notes and Sequence trigs restart the same variation progression on fresh Play or clip recall. Variation evolves across repeats; live playing keeps fresh variation.
- **Consistent Pulsar starts:** a note’s pulse clock starts promptly even when its voice previously used another engine. Sounding engine changes retain their smooth transition.
- **Updated manual:** explains mode behavior and Pulsar’s saved variation.

Existing projects and samples remain supported, and MIDI parameter identities are unchanged. Existing controller presets can stay in place. Older clips receive stable Pulsar seeds; their previously unsaved random variations cannot be reconstructed. New saves require 0.31.2 or newer to reopen. Export a project backup before updating if you may need to return to an older version.

Install or update through Schwung Manager using `https://github.com/s3g/s3g-thru`, or download `s3g-thru-module.tar.gz` from this release. Requires Schwung 1.7.3 or newer.
