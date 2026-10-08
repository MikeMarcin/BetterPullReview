---
schema: "implement-rfc.card.v1"
id: "BPR-1331"
title: "The private progress button shows the file's current state"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"origin":"Owner chat note, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1331--BPR-1331--755aeb836635.json","related_cards":["BPR-1129","BPR-1083","BPR-0647","BPR-1296"],"acceptance":["BPR-1331:core-decides","BPR-1331:glyphs","BPR-1331:follows","BPR-1331:owner-look"]}
---

# BPR-1331: The private progress button shows the file's current state

## Origin

The owner, in chat on 2026-10-08, quoted exactly:

> I think the toolbar for seen read accept needs to update its icon to the
> current state

This is the review bar's ✓▾ button, the selected file's private progress
(seen, read, accepted, excluded), on ⌥⇧4.

## Inventory before the change

- **Control.** `workbench_window::ReviewControlsButton`, a `menu_tool_button`
  on the review bar (object name `workbenchReviewControlsButton`), icon only,
  instant popup, strong focus, key Alt+Shift+4. Its menu
  (`workbenchReviewControlsMenu`) holds four checkable actions whose labels the
  core's projection decides: Seen, Read or Mark unread, Accept or Revoke
  acceptance, Exclude or Include.
- **Icon.** `strip_icon(strip_glyph::private_progress)`, a plain tick, set
  once per palette in `refresh_toolbar_icons`. It never changed with the file.
- **Tooltip and accessibility.** [[BPR-1129]] made the tooltip say the state
  ("Private progress (⌥⇧4) / This file is Read, accepted. Seen, Read, Accept or
  Exclude") and the accessible description repeat it. The accessible name was
  always "Private progress".
- **Where the state comes from.** The host publishes
  `application::file_receipt_action_projection` (the `core::file_receipt` and
  the four actions' legality from `core::apply`) to `workbench_window::publish`;
  a file switch clears it (`clear_review_ledger`) until the next file's
  receipt arrives. All of these funnel into `update_receipt_actions`. The
  receipt holds three independent facts: the read progression
  (unread, seen, read), accepted, and excluded.

## Outcome

- **The core decides one mark.** `core::progress_mark(file_receipt)` in
  `src/core/include/bpr/core/file_progress_mark.hpp`: excluded, else accepted,
  else the read progression. The core never builds a receipt that is both
  accepted and excluded.
- **Five glyphs, one circle.** `strip_glyph::progress_unread` to
  `progress_excluded`, painted through `painted_glyph_icon` like every strip
  glyph, in the strip's ink, one 16-unit circle in every state so only the fill
  moves. They follow the Overview read column's dots ([[BPR-0647]]: hollow,
  half, filled) and [[BPR-1083]]'s weight scale, where an emptier mark claims
  less:
  unread is a broken ring, seen a ring with its left half filled, read a filled
  disc, accepted the disc with a tick cut out of it, excluded a ring struck
  through. With no file's progress known (no selection, ledger unavailable) the
  plain tick stays. `qt::progress_glyph` maps the mark to the glyph.
- **Name, tooltip, description.** The accessible name is "Private progress:
  read" (unread, seen, read, accepted, excluded, "no file selected",
  "unavailable"). The tooltip's first line says the same with the key, then
  the facts ("This file is read and accepted."), then what a click offers,
  from the actions the core enabled ("Click to choose Seen, Read, Accept or
  Exclude."; with none enabled, "Click to open Seen, Read, Accept or Exclude,
  none available now.").
- **Follows.** `update_receipt_actions` sets the icon on every publication,
  failure and clear, so a change, a file switch and a refresh show at once;
  `refresh_toolbar_icons` paints the current mark, not the tick, on a palette,
  theme or scale change.

## Acceptance

- `BPR-1331:core-decides`: the mark is the core's, walked through the real
  transitions. **Pass** at `22fd752f5`: `core_review_ledger`
  (`test_one_progress_mark_sums_up_a_receipt`).
- `BPR-1331:glyphs`: each state has its own glyph and name. **Pass** at
  `22fd752f5`: `qt_review_state_icon` compares the button's icon with each
  glyph at 1x and 2x, checks the six glyphs differ pairwise, and checks the
  name, tooltip and description per state.
- `BPR-1331:follows`: the icon follows a menu change, a file switch, a
  palette refresh and an unavailable ledger. **Pass** at `22fd752f5`:
  `qt_review_state_icon`.
- `BPR-1331:owner-look`: pending, the owner's Accept or Reject of
  "review-state icon" on the frozen build.

## Verification

macOS arm64 Debug, `build/dev`, at `22fd752f5`:

- `ctest -R '^(core_review_ledger|qt_review_state_icon|qt_workbench_window_scale_(100|125|200)|qt_workbench_focus_order_scale_.*|qt_dif_workshop_placement|qt_document_tabs_scale_100)$'`:
  all pass. The window rows run the whole workbench test list, which now
  includes the new test.
- `architecture_boundaries` passes. The two composition contracts pin
  `src/app` only, which this card does not touch; they were not rerun here
  (the worktree built test targets only).
- **Failing-before controls.** Reverting `workbench_window.cpp` to its parent
  fails `qt_review_state_icon` ("before any file's progress is known the
  button shows the plain tick and says so"). Dropping only the icon update
  fails "the button's icon is the decided mark's glyph". Painting the tick in
  `refresh_toolbar_icons` again fails "a palette refresh keeps the accepted
  mark, in the new ink". Dropping acceptance from `progress_mark` fails
  `core_review_ledger` ("acceptance outranks the read progression").
- The first run of `qt_workbench_window_scale_*` caught [[BPR-1129]]'s
  assertion that the tooltip names the four actions with no file selected;
  the tooltip now names them then too.
- clang-format clean on every changed file. clang-tidy on the changed product
  files reports only findings on lines this card does not touch.
- Offscreen render of the review bar in the five states, 1x and 2x
  (`BPR_QT_RENDER_PATH` with `--review-state-icon`), looked at before the
  owner look.

`frontend-design:frontend-design` was loaded before the glyph design.

The candidate `22fd752f5` landed as `166773498` after a rebase onto
`c127e2413` (other cards' commits only); `core_review_ledger`,
`qt_review_state_icon` and `qt_workbench_window_scale_100` pass there too.

Linux and Windows have not run this change; it adds no platform code.

## Owner look, session v001

Pending: `BPR-1331:owner-look` is the owner's. One short session, one
candidate, one question, Accept or Reject.

- Session `032c5c2af586f5fb8fc113830ee1c1e0`, its own fresh state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1331-review-state-v001`,
  ready and protected at event cursor 3. Candidate
  `BPR-1331-review-state-icon-v001` ("review-state icon", revision
  `frozen-22fd752f5-v001`), with the frozen build's 2x review bar in the five
  states. The host was started detached and answered on loopback.
- Frozen root `/Users/jaeger/bpr1331-review-state-v001.uwoGJs`: `git archive`
  of `22fd752f5` into `src/`, configured with `app-release` and
  `BUILD_TESTING=ON` into `src/build/app-release-tests`, built
  `better_pull_review`, source read-only; executable SHA-256
  `6250e7236a8a957c64ff87c4c8ddb561cd7b2fcfccc086d76caa78e548b4e755`.
  Offscreen with the launcher's arguments it printed
  `ATTENDED_FILE_SHOWN:src/engine/renderer.cpp`.
- Launch: `python3 /Users/jaeger/bpr1331-review-state-v001.uwoGJs/launch.py`
  (`conventional-comments-v1`, signed in, `src/engine/renderer.cpp` open in
  Review). It checks the build and the fixture tree first and reports an exit
  within seconds.

Next: read the session's decisions (`status --after 3`). Accept satisfies
`:owner-look` and the card finishes; a Reject comes back with its note for a
v002.
