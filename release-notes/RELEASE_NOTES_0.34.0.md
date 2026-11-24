# s3g Thru 0.34.0 — draft

Local testing candidate: **0.34.0-test.13**. Not published.

- **Page Actions:** main-encoder clicks open tools for the current page, including compatible copy/paste, page reset, engine selection and starting sounds. Confirmations identify the target and support Undo.
- **Shared naming:** label clips, chains, Bank pads and lanes using the same pad keyboard as Process. Project export remembers its name and adds a timestamp for each backup.
- **Sample file tools:** rename Thru captures and Process files, save exact named copies of library WAVs, or save and load a copy. Shared assignments follow managed renames; existing files are never overwritten.
- **Browser live panel:** compact live menu panels show engine and effects controls together, with sliders and value fields. Clip and editing-part changes follow Move. Play and Stop use the existing transport.
- **Browser Sequence editing:** four visible lanes, step selection, core trig controls and engine/amp/effect locks through the menu panels. Lock highlights, individual resets, Burst-event inheritance and Lane defaults make the editing target explicit. Smaller panels keep more controls visible.
- **Clip and list controls:** Single clips retain their inactive layout preferences without breaking the browser link. Clip changes recover automatically, and Pads & Scale includes One/List, engine slots, entry setup and pressure controls with native readback.
- **Cutups lane balance:** L1–L4 now affect Cutups playback, including outgoing cuts during joins, with smooth live level changes in every mode.
- **Slider readouts:** thin line markers replace round thumbs; parameter boxes follow each movement while dragging.
- **Live feedback:** values update in place, edits are acknowledged, and the connection is marked Linked only after valid state arrives. Loaded-sample state and version checks prevent a successful handshake from hiding an unlinked panel.
- **Browser clip workspace:** named clip strip, copy/paste and duplication, separate clears, note and curve editing, undoable touch overdubs, phrase transforms and portable multi-clip collections with optional WAVs and chains.
- **Composition layout:** notes and automation stay beside an independently scrolling parameter dock. Menu-group tabs, a resizable divider, tool drawers and separate Notes / motion and Sequence views reduce navigation. Smaller screens use Editor / Controls.
- **Browser piano roll and cursor automation:** a taller, resizable pitch grid, independent edit and playback cursors, menu-driven breakpoints, grouped slider Undo and readable part/patch/FX-corner curve names. Play/Engine and supported synthesis and effects controls follow the automation value at the edit cursor.
- **Sequence cursor:** continuous browser animation follows native phase and traversal instead of skipping steps between status updates.
- **Portable composition files:** versioned JSON with optional WAVs and Thru-specific clip extensions, a validation/pack CLI, schema and example composition support manual and generated material. Complete project backups remain `.thru` files.
- **Updated help and manual:** control descriptions and screen illustrations cover the new actions and naming workflow.

- **Browser editing refinements:** Clip length / Resize clip labels, compact parameter panels, an eight-note overlap safeguard, Delete / Backspace and Enter editing, and Space for Play / Stop.

Existing projects and MIDI parameter identities are retained. Record and Shift + Record keep their performance and audio recording roles.
