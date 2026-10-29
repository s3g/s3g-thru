# s3g Thru 0.28.0 — local testing build 15

- Menu, Back, Shift, Delete, navigation arrows, Capture, and Copy now illuminate
  where they are used. Available controls are dim white; held buttons and active
  modes brighten. Their lights appear immediately when takeover opens.
- Capture stays lit in Single mode to help locate Capture + Menu for Pages.
  In Split, Capture still indicates A/B focus and Mute indicates the focused
  part's mute state. Mute remains dark in Single, where it has no assignment.
- Navigation arrows follow the current view. Back also brightens while Shift
  offers an available safety undo. Copy goes dark while recording blocks a mode
  change. Unassigned icon keys remain dark.
- Loop uses its white LED brightness correctly: bright for a live Stack Hold
  or Glitch Buffer Lock, medium for a recorded Stack Hold, and dim while running.
  The footer identifies the active hold. No RGB color values are used for Loop.
- Icon lights clear on exit and are restored when takeover reopens. Unchanged
  states produce no repeated LED messages.

Includes all features from testing build 14. This update changes UI, illumination,
and the manual; audio processing and saved-set formats are unchanged. Local
package for testing on Move.
