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
metadata: {"origin":"Owner chat note, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1331--BPR-1331--755aeb836635.json","related_cards":["BPR-1129","BPR-1083","BPR-0647","BPR-1296"],"acceptance":["BPR-1331:core-decides","BPR-1331:glyphs","BPR-1331:follows","BPR-1331:push","BPR-1331:owner-look"]}
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
  v003 replaces the glyphs with one coloured family on every surface; see
  "v003: one icon language, the circles, with colour".
- `BPR-1331:follows`: the icon follows a menu change, a file switch, a
  palette refresh and an unavailable ledger. **Pass** at `22fd752f5`:
  `qt_review_state_icon`.
- `BPR-1331:push`: across a push, a changed file's button reopens unread,
  an unchanged file keeps its mark, an excluded file stays excluded (the
  owner's v001 note). **Pass**: `application_review_progress_push_contract`,
  `core_diff_model`; see "When the pull request gets a push".
- `BPR-1331:owner-look`: pending, the owner's Accept or Reject of
  "review-state icon" on the frozen build, including the push.

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

## Owner verdict, session v001

Reject, 2026-10-09T02:19:42Z, decision `d9d02f72-5f3a-4f96-8429-7616e5ae3401`
at session event 4. The note, verbatim:

> …/aito  main❯ python3 /Users/jaeger/bpr1331-review-state-v001.uwoGJs/launch.py
> FIXTURES_MISSING: /Users/jaeger/bpr1331-review-state-v001.uwoGJs/src/tests/fixtures/github/pull-requests/conventional-comments-v1
>
> I just want to make sure the flow continues to make sense when the PR gets an update.

The build never opened, so `BPR-1331:owner-look` stays pending. The cause and
the launcher hardening are on [[BPR-1339]] ("Owner verdict, session v001"):
the launcher checked a fixture path no commit has had, and the freeze never
ran the launcher itself.

## When the pull request gets a push

The owner's question. RFC-BPR-0007: a receipt survives a new epoch exactly
when the compared material is unchanged. When it changed, the core reopens
the read progression to unread (not seen) and drops the acceptance; exclusion
is a standing statement about the file and survives. So after a push:

| File | Before the push | After the push |
|---|---|---|
| changed by the push | accepted | unread (broken ring) |
| not changed by the push | accepted | accepted |
| excluded, changed or not | excluded | excluded |

The button shows the open file's receipt once the ledger has answered for the
file's new material, on opening the file. Overview and the Files drawer show
the stored receipt until then.

**Found and fixed: unchanged files reopened too.** The composed application
attached the receipt to `core::comparison_digest` of the selected comparison.
That digest encodes both anchors, and an anchor names its epoch, so every push
moved every file's digest and every file came back unread on opening, changed
or not. RFC-BPR-0007's "changed material invalidates only affected receipts"
was verified at the core and host with hand-made material identities; nothing
ran a real push through the composition. The receipt now attaches to
`core::file_material` (`src/core/include/bpr/core/diff_model.hpp`): the
comparison digest's dimensions without the anchors (pull request, file, both
blobs, options, algorithm), domain `BPR-file-material-v1`. Notes, defects and
drafts keep the comparison digest. One consequence: receipts written before
this change carry the old identity, so each file reopens once, the first time
it is opened after the update.

**Fixture.** `conventional-comments-v1` now has a next push
(`tests/fixtures/github/pull-requests/conventional-comments-next-push-v1`,
`scripts/build_conventional_comments_next_push_pack.py`): head
`cd24836cfb58a9c14f4d3464d68a57ff8aa2a186` on top of `b20cb4434…`, changing
only `src/engine/renderer.cpp` lines 12 and 13 (the clamp its line-12 thread
asks for "before the next push"). It is served only under an epoch phase:
`single` over a data root is the first head, `reuse` over the same root the
push.

**Tests.**

- `application_review_progress_push_contract` (new, real composition, two
  processes over one data root): on the first head the course accepts
  `renderer.cpp` and `src/main.cpp` and excludes `src/ui/widgets/button.cpp`
  through the review bar's actions; on the push it reopens each and reads the
  private progress button's accessible name: `renderer.cpp` "Private progress:
  unread", `main.cpp` "Private progress: accepted", `button.cpp` "Private
  progress: excluded". **Failing-before control:** with `file_material`
  returning the comparison digest, the same course reads `main.cpp` as
  "Private progress: unread".
- `core_diff_model` (`test_file_material_outlives_a_push`): advancing both
  anchors' epochs moves the comparison digest and keeps the material; a moved
  head or base blob moves it; every other dimension moves it; through
  `core::carry_over`, an unchanged accepted file stays `accepted`, a changed
  one reopens `unread` without its acceptance, an excluded one stays
  `excluded`, and a receipt keyed by the comparison digest would reopen.
- `application_review_ledger_composition_contract` now also reads the button:
  "accepted" after the seed gestures on `github-com`, "unread" when the
  changed file is reopened on its second head.

## Owner look, session v002

Pending: `BPR-1331:owner-look` is the owner's, Accept or Reject.

- Session `0d079940affe6da5c393df07d82a22d8`, its own fresh state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1331-1339-1091-v002`, ready and protected at event cursor 9, shared by the v002 looks
  of [[BPR-1331]], [[BPR-1339]] and [[BPR-1091]] (one question each). The
  host was started detached and answered on loopback.
- v001's session was released (`resume-development --all`) and its root
  removed with `scripts/freeze_attended_build.py remove`.
- Candidate `BPR-1331-review-state-icon-v002` (revision `frozen-54dfbd463-v002`, fingerprint
  `2f93777b…`), with the v001 2x review bar in the five states.
- Frozen root `/Users/jaeger/bpr1331-review-state-v002.o_c2hktq`: frozen by `scripts/freeze_attended_build.py` from
  `54dfbd463` (exported, built with `app-release` and `BUILD_TESTING=ON` in the
  root, source read-only); executable SHA-256 `a2330d3f6a616198b70d7b7c7227a910907b177a3bf1db652ab4721bcda4ed1c`. The launcher's own
  `--check` passed at freeze time, offscreen, and once natively with
  `--check --native` (ready, then closed).
  `launch.py --next-push` reopens the same review data on the pushed head
  `cd24836c…`; `--fresh` starts over. Its freeze check opened both heads.
- Launch: `python3 /Users/jaeger/bpr1331-review-state-v002.o_c2hktq/launch.py`.

Next: read the session's decisions (`status --after 9`).

## Owner verdict, session v002

Reject, 2026-10-09T04:10:42Z, decision `6d9fa78e-5c7c-45fd-982d-0e1fa0b5dc11`
at session event 11 (candidate `BPR-1331-review-state-icon-v002`,
fingerprint `2f93777b…`, revision `frozen-54dfbd463-v002`). The note,
verbatim:

> It's very weird to me that we have 2 icon languages for the same concept. Pick one. I think the I think the circles here is probably better overall but it should get some color. All green aint the best either.

One attachment: the private progress menu open under the button (which shows
the broken-ring unread mark), its four actions drawn in the older pictorial
icons: a green ball for Seen, a green tick for Read, a green arrow for Accept
and a red cross for Exclude.

`BPR-1331:owner-look` stays pending. The direction for v003: one icon
language for the button and its menu, the circles, with colour, and not all
green. The session was handed back at event 13, released with
`resume-development --all`, and the root
`/Users/jaeger/bpr1331-review-state-v002.o_c2hktq` removed with
`scripts/freeze_attended_build.py remove`; v003 freezes its own root and opens
its own short session.

## Linux station run (bpr-main-linux, 2026-10-08)

At `e602e2cf9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner, all pass:
- the push contract, `application_review_progress_push_contract`;
- `core_diff_model` and `core_review_ledger`;
- `qt_review_state_icon`, `qt_workbench_window_scale_{100,125,200}` and
  `qt_workbench_focus_order_scale_{100,125,200}`;
- `qt_dif_workshop_placement` and `qt_document_tabs_scale_100`.

Windows is still owed.

## v003: one icon language, the circles, with colour

The owner's v002 note asked for one picture of each state, the circles, with
colour and not all green. `frontend-design:frontend-design` was loaded before
the design.

**Design.** One family, painted by one function,
`qt::paint_progress_mark` (`src/adapters/qt/include/bpr/qt/progress_mark.hpp`),
everywhere a file's private progress shows. Each mark is the v002 circle, 16
units across in a 24-unit box. Its shape inside the circle carries the state
without colour, and its colour, a theme role, makes it readable at a glance.
Colour builds up as the review moves on:

| Mark | Shape | Colour role | Light | Dark |
|---|---|---|---|---|
| unknown (no file, ledger unavailable) | dashed ring | `state_stale` | `#5B6570` | `#8B949E` |
| unread (and no receipt) | ring | `state_stale` | `#5B6570` | `#8B949E` |
| seen | ring, left half filled | `state_attention` | `#AD7A00` | `#D29922` |
| read | filled disc | `state_unread` (the blue) | `#0969DA` | `#58A6FF` |
| accepted | disc, tick cut out | `state_accepted` | `#1A7F37` | `#3FB950` |
| excluded | ring struck through | ring `state_stale`, strike `state_defect` | `#CF222E` | `#FF7B72` |

Read is blue rather than teal or purple. A colour study at 16 px showed
teal too close to the accepted green on the light theme, and purple too loud.
The blue is the theme role named "State unread". The vocabulary has no other
blue, and adding one would change the core's closed role set. So a theme that
recolours "State unread" also recolours the read mark. Recorded here, not
hidden.

**Surfaces.**

- *Review bar button* (`workbench_window::refresh_progress_icons`): the
  selected file's mark, as before. The plain tick for "no file selected" and
  "unavailable" is now the family's dashed ring, so the button never leaves
  the family.
- *Its menu*: Seen, Read, Accept and Exclude show the seen, read, accepted and
  excluded marks, so the menu is the family's legend. The icons stay the same
  when a label flips (Mark unread, Revoke acceptance, Include). This replaces
  the platform's `SP_Dialog*` pictures (green ball, green tick, green arrow,
  red cross) from the owner's attachment. On a highlighted row the mark keeps
  its colour inside a thin ring of the menu's own background, the way a tag
  dot looks in a macOS menu, so the blue disc stays visible on a blue
  selection.
- *Overview's read column*: the same painter at 15 units, 10 px across, as
  the dot it replaces. Read was green (`state_accepted`) and is now blue, and
  seen is amber. The accepted mark the owner accepted on [[BPR-1339]] keeps
  its shape and its green.
- *Files drawer*: every file the overview has described shows its mark in the
  same slot, not only the faint unread ring, as RFC-BPR-0007 says ("Overview's
  read column and the Files drawer show the mark"). A file with no receipt
  shows unread, as on Overview. The unread-comments accent dot
  ([[BPR-0656]]) used to replace the ring. It now sits beside the counts, so
  the two never share a slot.
- *Status line*: text only ("Private progress: read, accepted."), no icon, so
  it is unchanged. The Review Cycle view's "Mark reviewed" tick
  ([[BPR-1182]]) marks the pull request, not a file's progress, so it keeps
  the strip tick.

The theme reaches the surfaces through the composition:
`github_application::apply_appearance` hands the window
`qt::progress_mark_colours_for(profile)` beside the attention amber.
Overview and the Files drawer read the same colours in their own
`adopt_appearance`. Before the hand-over, the window uses the built-in light
or dark colours, whichever its palette is. The v002 `strip_glyph::progress_*`
glyphs and `qt::progress_glyph` are gone. The menu icon and the strip glyphs
share the adapter's one `QIconEngine` (`painted_mode_icon`, beside
`painted_glyph_icon`), so `architecture_boundaries` keeps its single exempt
`clone()`.

Accessible names, tooltips, descriptions and the status text did not change.
The v002 push behaviour did not change either (`core::file_material`, the
table above).

**Tests, all failing before the matching product change and passing after.**

- `qt_review_state_icon` (`test_review_state_marks_are_one_coloured_family`,
  `test_review_state_icon_shows_the_decided_mark`): in each of the four
  built-in themes, the five colours differ and read and seen are not green.
  Each mark paints its own colour, with excluded struck in the defect red.
  The six marks differ in shape in monochrome at 1x and 2x. The button shows
  the decided mark through every state, a menu change, a file switch, a
  palette refresh and an unavailable ledger. The menu's four actions show the
  family's marks through every state. A highlighted row keeps the blue. A
  theme hand-over repaints the button and the menu.
- `qt_overview_read_marks` (new row; `--read-marks` runs
  `test_the_read_column_shows_the_mark` alone): each row paints its state's
  colour, only accepted is green, and accepted is a green disc with a tick
  cut out. Light and dark renders come from `BPR_OVERVIEW_MARK_RENDER_ROOT`.
- `qt_files_tree` (`test_file_marks_are_the_review_bar_family`, delegate
  painted at 2x, light and dark): each file's colour, a file with no receipt
  unread, only accepted green, unread comments leave the mark alone and
  sit beside the counts, and a file the overview has not described shows
  no mark.
- Failing-before controls, each a product-only revert on the candidate:
  restoring the `SP_Dialog*` menu icons fails `qt_review_state_icon` ("Seen,
  Read, Accept and Exclude show the seen, read, accepted and excluded marks
  the button shows"). Painting Overview's read in the accepted green fails
  `qt_overview_read_marks`. Drawing the drawer's mark only while unread fails
  `qt_files_tree` ("each file's mark is painted in its state's colour").
  Dropping the repaint from `set_progress_mark_colours` fails
  `qt_review_state_icon` ("the theme's colours reach the button and the
  menu"). Against the v002 product the new tests do not compile, because
  `progress_mark.hpp` does not exist there.

**Verification.** macOS arm64, `app-release` with `BUILD_TESTING=ON`
(`build/rel` in the worktree), offscreen:
`ctest -R '^(qt_files_tree|qt_overview_read_marks|qt_review_state_icon|architecture_boundaries|qt_workbench_window_scale_100|application_review_round_trip_composition_contract|application_review_ledger_composition_contract|application_review_progress_push_contract|application_accepted_file_in_overview)$'`
passes. `qt_overview_read_marks` and `qt_files_tree` also pass at
`QT_SCALE_FACTOR=2`, and `--review-state-icon` at 1.25. clang-format is clean
on the changed lines. A direct clang-tidy run on the changed product files
reports two findings, both on lines this card does not touch.

- **Pins.** `github_application.cpp` changed, so the round-trip contract's
  full-application and production-manifest digests are resealed in the same
  commit. The mutation pin did not change.
- **Not this card's failure.** `qt_pull_request_overview_page` fails on `main`
  at `c2ea0e158` without this change, at [[BPR-1094]]'s new "light icons at
  100%: a refresh keeps the scroll position and the chip" in this macOS
  Release build. That is why the read column has its own row.

**Renders** (offscreen, looked at before the owner look). The review bar in
the six states and the open menu with Read highlighted, light and dark at 2x
(`BPR_QT_RENDER_PATH` with `--review-state-icon`). Overview's read column and
the Files drawer, light and dark.

Linux and Windows have not run v003. It adds no platform code.

The candidate passed the focused set above as `6c0dca434`. It landed on main
as `ba3c970b9`, rebased over an issue-ID allocation only, with identical
source.

## Owner look, session v003

Pending: `BPR-1331:owner-look` is the owner's. One new short session, one
candidate, one question, Accept or Reject.

- Session `fa2c1dc9748e742614c16cfe36bb3943`, its own fresh state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1331-review-state-v003`,
  ready and protected at event cursor 3. The host was started detached and
  answered on loopback.
- Candidate `BPR-1331-review-state-icon-v003` ("One coloured circle family
  for a file's review state", revision `frozen-ba3c970b9-v003`, fingerprint
  `dcfbb35c…`). It carries two offscreen 2x renders: the button in each state
  and the menu with Read highlighted, light and dark; then Overview's column
  and the Files drawer, light and dark.
- Frozen root `/Users/jaeger/bpr1331-review-state-v003.i5hf09qz`, frozen by
  `scripts/freeze_attended_build.py` from `ba3c970b9` (`--next-push`).
  Executable SHA-256
  `264beea3a84d2a7722b07e9798b8918bd36016069465d88b084f83965887e576`. The
  launcher's own check showed `src/engine/renderer.cpp` on the first head
  `b20cb443…` and on the pushed head `cd24836c…`.
- Launch: `python3 /Users/jaeger/bpr1331-review-state-v003.i5hf09qz/launch.py`
  (`--next-push` for the pushed head, `--fresh` to start over).

Next: read the session's decisions (`status --after 3`). Accept satisfies
`:owner-look` and the card finishes. A Reject comes back with its note for a
v004.

## Linux station run (bpr-main-linux, 2026-10-09)

At `b0ef8d7b9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner: v003's `qt_review_state_icon`, `qt_files_tree` and `qt_overview_read_marks` pass.
