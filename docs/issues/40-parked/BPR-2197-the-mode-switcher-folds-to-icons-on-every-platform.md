---
schema: "implement-rfc.card.v1"
id: "BPR-2197"
title: "The mode switcher folds to icons on every platform"
status: "Parked"
rfcs: ["RFC-BPR-0112"]
metadata: {"design":"docs/design/windows-title-fit-v2/README.md","design_branch":"claude/windows-title-fit-mockups","design_revision":"615bd65c8","design_session":"20c49e523db3f8c071bda18a1ee8d2bd","earlier_design_session":"1073f45be455a420999e60fd5e502ee3","owner_decision":{"date":"2026-11-15","quote":"I think all 3 platforms should do the same icon on narrow trick."},"reservation":"docs/issues/_reservations/BPR-2197--BPR-2197--36dc5cc18504.json"}
depends_on: ["BPR-2196"]
parked_on: "human-review"
---

# BPR-2197: The mode switcher folds to icons on every platform

## Scope and acceptance

The owner's accepted design `windows-title-fit-v2`
(`docs/design/windows-title-fit-v2/README.md` on
`claude/windows-title-fit-mockups` at `615bd65c8`; attended review
`windows-title-fit-v002`, session `20c49e523db3f8c071bda18a1ee8d2bd`, state
`~/.attended-review-state/windows-title-fit-v002-2026-11-15/`, Accepted
2026-11-15 with no note). It follows round 1
(`docs/design/windows-title-fit-v1/README.md`, session
`1073f45be455a420999e60fd5e502ee3`), where the owner chose icons that open:
"I like icons when it gets narrow because we already do that for the drawer
tabs", then "Okay nvm the ^ on windows" and "I think all 3 platforms should
do the same icon on narrow trick."

Build it after [[BPR-2196]] lands: both change the same switcher. The owner
has since renamed the fifth mode from Revise to **Author**. The design draws
it as Revise; build it as Author. Its pixel figures were measured with the
word Revise and are a guide, not a contract.

- **Faces, widest first.** Names and keys; names; icons with the open mode
  named; icons alone. The first two are what `modality_control` shows today.
  The last two are the drawer tabs' narrow faces (`fit_tray_tabs`:
  `face::open_named`, `face::glyphs`). No mode shows an icon while every name
  is showing, as in the drawers since [[BPR-1755]].
- **Which face.** The widest beside which the whole name fits: repository,
  number and title. When none does, the open mode keeps its name for as long
  as a title of the row's minimum (`identity_title_minimum`) still fits beside
  it; the last face is the icons alone. So the other modes' names yield before
  the title is cut, and the open mode's name yields before the title is lost.
  No face returns at a narrower width.
- **Icons.** Existing glyphs only, in the tabs' size and stroke: Overview
  `conversation_glyph::rendered`, Review `conversation_glyph::source`, Discuss
  `conversation_glyph::comment`, Threads `conversation_glyph::finder`, Author
  `strip_glyph::edit_file`. The open mode's glyph is drawn in the
  highlighted-text ink, since it sits on the accent.
- **Key hints.** Each platform keeps its own in the widest face: Command-1 to
  Command-5 on macOS, Ctrl+1 to Ctrl+5 on Windows and Linux
  (`QKeySequence::NativeText`, as today). No caret form.
- **Tooltips and names.** A mode showing only its glyph says its name first
  in its tooltip, as a folded tab does; the tooltip keeps the key. Accessible
  names keep the whole word in every face.
- **The same on macOS, Windows and Linux**, with four modes as with five.
- **What goes.** The three-letter prefixes (`set_abbreviated`); the
  Windows-only threshold that drops the keys below 1300 ([[BPR-1324]]); and
  the rule that shows the title only if it would also fit beside the keyed
  switcher ([[BPR-1251]], `IdentityTitleBudget`). The fold decides all three
  from the room.
- **One fold helper.** The face choice is one shared helper used by the
  tray's tabs (`fit_tray_tabs`), Overview's drawer tabs and the switcher. It
  must not become a third copy.
- **Architecture:** presentation only. Qt presents; nothing here is decided
  in the core or performed by the host.

Load `frontend-design:frontend-design` before the Qt work and say so in the
report.

## Intended verification

A Qt course over the switcher at falling widths with a long and a short
title, four modes and five: the face at each width, that no face returns at
a narrower width, that the title is cut only after the other modes' names
have gone, each platform's key hint, the tooltip of a glyph-only mode and
the accessible names. The drawer tab courses pass unchanged on the shared
helper. Native renders on each platform against the design's sheets
(`docs/design/windows-title-fit-v2/png/`).

## Built (2026-11-15)

Design merged into the lane with `git merge --no-ff`. The
`frontend-design:frontend-design` skill was loaded before the Qt work.

- **One fold.** `bpr/qt/fold_faces.hpp`: `fold_face` (names and keys,
  names, open mode named, glyphs), `fold_text`, `fold_tooltip` and
  `choose_fold_face`, the first face widest first for which the row fits.
  `drawer_tab_bar::fold` applies it to a tab row, and Review's tray
  (`fit_tray_tabs`) and Overview's drawer (`fit_tabs`) both call it in place
  of their two copies; the switcher calls `choose_fold_face` with its own
  measure.
- **The switcher.** `modality_control::set_face` replaces `set_compact` and
  `set_abbreviated`. Glyphs are the five the design names; the open mode's
  is drawn in the highlighted-text ink and follows the open mode; a
  glyph-only segment's tooltip says its name first and keeps its key;
  accessible names keep the whole word. Keys are `QKeySequence::NativeText`
  (⌘1 to ⌘5 on macOS, Ctrl+1 to Ctrl+5 elsewhere).
- **The rule.** `apply_strip_width` asks each face widest first: the whole
  name fits beside it, or, for the open mode named, the name with a title of
  `identity_title_minimum`; the glyphs alone otherwise. Gone: the
  three-letter prefixes, the Windows 1300 threshold (BPR-1324) and the
  BPR-1251 title budget (`IdentityTitleBudget`).
- **The floor.** Five segments fit the 900-pixel floor: the window course
  offers Author and checks all five at the floor again.

Measured offscreen on macOS (Debug, `--title-row`), Author open: the
switcher is 471 pixels with keys, 343 with names, 212 with the open mode
named and 162 with glyphs (the design's 162). Long title: names at 1600,
open mode named from 1366 to 960. Short title: keys to 1280, names at 1100,
open mode named at 960. A long repository name reaches the glyphs at 1100.

Later commits: `0f7716a53` (an env-gated title-row render,
`test_render_the_switcher_fold`), `c58080042` and `2d045a544` (its font
for a remote session).

Verified on macOS (Debug, dev): `qt_fold_faces` (the shared fold chooses
the tray tabs' faces as before at every room), `qt_workbench_title_row_scale_{100,125,200}`
(new `test_the_switcher_folds_to_glyphs`; `test_strip_identity_stays_at_every_width`
with four modes and five), `qt_workbench_window_scale_100` (Author at the
floor), `qt_drawer_parts_scale_*`, `qt_drawer_surfaces_fit_scale_*`,
`qt_local_draft_window`, `qt_button_faces`, `qt_button_faces_cocoa`,
`architecture_boundaries`, `application_review_round_trip_composition_contract`;
clang-tidy on the changed units; documentation gate. Linux station (WSL
Ubuntu, Debug, offscreen): `--title-row` passes. Windows station (Release,
over SSH): `test_strip_is_the_title_row` and
`test_the_floor_follows_the_review_bar` pass;
`test_strip_identity_stays_at_every_width` stops at "the review bar's
positions keep their words at 1280" and `test_the_switcher_folds_to_glyphs`
at "at 1600 the long title shows whole", with the session's misread system
font (the design notes the same); not confirmed at the real Windows font.

## Owner look title-fold-v001 filed (2026-11-15)

Session `84fbe840afbd99aaf6ce0a68bebe6cbc`, state
`/Users/jaeger/.attended-review-state/title-fold-v001-2026-11-15/state` (URL
in that directory's `serve.log`), cursor 3, host PID 97593 (Tailscale child
97603). Frozen root `/Users/jaeger/title-fold-v001.tfds9x8z` from
`0f7716a53878` (executable SHA-256 `9af0310fc583…`; the launcher's check
reached `ATTENDED_PULL_REQUEST_OPENED` and started no real agent), on
`nested-folders-v1`, a pull request the fixture viewer owns, so all five modes
show. One Accept/Reject question, `BPR-2197-title-fold`, with title-row
renders at 1440, 1200 and 900 pixels: macOS (Cocoa, dark and light), Windows
11 (Segoe UI 9 pt) and Linux (offscreen). If accepted, BPR-2197 is Done.
