---
schema: "implement-rfc.card.v1"
id: "BPR-1336"
title: "Zoom the code in the comparison: View > Zoom In, Zoom Out and Actual Size"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner chat request, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1336--BPR-1336--e0eddadebca1.json","related_cards":["BPR-1334","BPR-1335","BPR-1328","BPR-1295","BPR-1107","BPR-1291","BPR-0118"],"requested_skill":"frontend-design","acceptance":["BPR-1336:C1","BPR-1336:C2","BPR-1336:C3","BPR-1336:C4","BPR-1336:C5","BPR-1336:C6"]}
acceptance: [{"criterion":"BPR-1336:C1","outcome":"pass","revision":"7a35a4bed","evidence":{"kind":"command","detail":"qt_workbench_window_scale_{100,125,200}, qt_diff_workbench_scale_{100,125,200} and application_diff_zoom_public_path: View > Zoom In/Zoom Out/Actual Size and their keys step both panes, the gutter, the tab stop and the detail pane through the core ladder and clamps with reasons; Code size and the drawer's code font are unchanged."}},{"criterion":"BPR-1336:C2","outcome":"pass","revision":"7a35a4bed","evidence":{"kind":"command","detail":"Every key in the assembled window and desktop bar bound once; Cmd-0 still Home and Plain in the label menu, Cmd-Opt-0 is Actual Size and not Plain; editor_keeps_typed_text keeps AltGr+0 '}' in editable fields only (Windows filter not run here); Cmd-scroll and pinch step, plain scroll still scrolls."}},{"criterion":"BPR-1336:C3","outcome":"pass","revision":"7a35a4bed","evidence":{"kind":"command","detail":"sqlite_appearance_settings_storage (schema 9, v8/v7 migrate to 100%, 49/301 refused), application_appearance_host (two gestures two steps, persists, restart, reset), qt_appearance_settings_panel (Diff zoom row), application_profile_startup_public_path (Work reopens at 125%, Default unchanged)."}},{"criterion":"BPR-1336:C4","outcome":"pass","revision":"7a35a4bed","evidence":{"kind":"command","detail":"qt_diff_workbench: reader's line at y 127 stays within one row in the same turn and after remeasurement at 150% and Actual Size; pill and polite announcement say 'Code zoom 150%, 13.50 pt/points' and the pill leaves after ~1.2 s."}},{"criterion":"BPR-1336:C5","outcome":"pass","revision":"7a35a4bed","evidence":{"kind":"command","detail":"Frozen Release build of 7a35a4bed, application_large_review_responsiveness twice: zoom dispatch 3.7-6.1 ms, largest input gap 12.7-17.0 ms, completion 334-387 ms on the 20,000-line comparison; a 150 ms stall control fails the gates."}},{"criterion":"BPR-1336:C6","outcome":"pass","revision":"7a35a4bed","evidence":{"kind":"observation","detail":"Owner Accept, no note, on candidate BPR-1336-diff-zoom-v002 'diff zoom' (fingerprint 8a4dd304..., frozen Release build of 7a35a4bed, binary 86c6e7a2) in session 2a0daa3bc755d694f88a49274b1484cf, decision e2ae12e4-d515-4785-9ab3-d1539706c720 (2026-10-09T01:22:49Z), handed back at event 6."}}]
---

# BPR-1336: Zoom the code in the comparison

## Owner request

The owner, in chat on 2026-10-08, verbatim:

> I think we should have a more accessible zoom/text size for the diff pane

## What exists today (inventory, `origin/main` a8f4531e8)

- **Code size lives in Settings only.** Settings > Appearance > Type has a
  **Code size** row (`appearanceCodeSizeRow`, a point spin box bounded by
  `core::bounds_of(point_size)`, 6 to 72 pt). It is one field of
  `core::code_font_settings`, persisted per profile in the appearance store
  (schema version 8) and applied to every code surface: the panes, the
  conversation cards, the finder, Overview's reader and the share sheet.
  Reaching it takes the Settings window, a scroll to Type and a spin box.
- **No zoom exists.** No action, key, wheel or gesture changes the code size.
  `measured_geometry_change::zoom` is declared and nothing raises it.
  `docs/ux/appearance-and-accessibility.md` already promises that "runtime code
  zoom preserves the top visible source line and does not silently overwrite
  saved font settings", with nothing behind it.
- **⌘-scroll scrolls.** The comparison's scroll controller handles every wheel
  event over the panes and ignores Control/Command, so ⌘-scroll is a plain
  scroll; `QPlainTextEdit`'s own Ctrl-wheel zoom never runs. Pinch is not
  handled.
- **How the comparison scales with the code font.**
  `appearance_profile::apply_code_font_to(comparison)` sets the font on the
  comparison and every `QPlainTextEdit` in it (both panes, the one-pane
  document and the Detail pane), sets their tab stops, and publishes the tab
  stop. Each pane's gutter re-widths on `FontChange` from its digit count, so
  the line numbers already follow the font. The connector and link map are
  painted from measured geometry, which the font change invalidates and
  rebuilds over bounded turns (512 lines or 4 ms a turn). The scroll
  controller keeps the line on its synchronization rail, 43% down the
  viewport, across a remeasurement ([[BPR-1291]]).
- **Line details.** [[BPR-1334]] (in progress, not on `origin/main`) turns the
  Detail pane into the Line details strip. The Detail pane is a
  `QPlainTextEdit` inside the comparison, so it takes the comparison's code
  font with the panes.
- **The drawer's code snippets** take `appearance_profile::code_font()` through
  `adopt_appearance`, separately from the comparison.
- **Keys.** ⌘0 (Ctrl+0) is Home; inside the open label menu it is Plain
  ([[BPR-1080]]). ⌘= ⌘+ ⌘− and ⌘⌥0 are unbound in the window, its desktop
  menu bar and the comparison control (which holds Ctrl+Alt with arrows, R, P,
  Shift+D and Escape).

## Decision: a zoom multiplier, beside Code size

Zoom is a percentage, **Diff zoom**, stored per profile with the code font and
shown in Settings > Appearance > Type directly under Code size. It does not
change Code size.

- **Actual Size has a meaning.** With a multiplier, Actual Size returns to the
  reviewer's own Code size. If zoom rewrote Code size, Actual Size could only
  return to the factory size and would throw away a size the reviewer chose.
- **It keeps the existing contract.** The appearance spec already says runtime
  zoom does not overwrite saved font settings.
- **It is scoped to the comparison.** Drawers, Overview and the composer stay at
  Code size, so zooming the diff to read a long line does not also reflow the
  conversation column.
- **It persists like Code size.** The profile's appearance record carries it
  (schema 9, a version-8 record takes 100%). A restart, a second window and
  Settings all show the same value, and the Type card's Reset resets it with
  the code font.

## Change

- **View > Zoom In, Zoom Out, Actual Size**, in the window's View menu and the
  resident desktop menu bar (disabled there, per [[BPR-0796]]).
  - Zoom In: ⌘= and ⌘+ (Ctrl+= and Ctrl++).
  - Zoom Out: ⌘− (Ctrl+−).
  - Actual Size: **⌘⌥0 (Ctrl+Alt+0)**. ⌘0 stays Home, and Plain inside the
    label menu. ⌘⌥ is the modifier pair the comparison control already uses
    for its own commands, and the label menu matches ⌘0 exactly, so ⌘⌥0
    never chooses Plain. On Windows, where AltGr arrives as Ctrl+Alt, an
    editable text field that receives AltGr+0 as a typed character (`}` on
    German and Nordic layouts) keeps the character.
- **⌘-scroll and pinch** over the code panes zoom by one step for each wheel
  notch (120 eighths of a degree; a trackpad's small deltas add up to the
  same) or for each 0.15 of pinch. A reversal starts the count again.
- **Steps** are 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250 and 300
  percent. Core owns the steps, the clamp and the zoomed size. The zoomed size
  is the Code size times the percentage, held to the 6 to 72 pt bounds. Zoom In
  is disabled when the next step would not change the size, and Zoom Out at
  the bottom. Each disabled command says why.
- **What zooms:** both panes and the one-pane document, their gutters (which
  re-width to the digits), the connector and link map (remeasured), the tab
  stops, and the Detail pane / Line details strip. The drawer's snippets keep
  Code size.
- **Indicator:** a small pill at the top centre of the comparison shows the
  percentage and the size on screen ("125%  16.25 pt") for 1.2 s, in the
  theme's own surface and text colours, with no animation. The same words are
  a polite accessibility announcement.
- **Settings:** a **Diff zoom** row under Code size, one item per step, each
  saying the size it shows ("125% (16.25 pt)").
- **Scroll anchor:** a visible current line stays where it was in the viewport.
  Otherwise the line on the rail stays, as for any remeasurement.

## Acceptance

- **C1 (direct).** In the production window, View > Zoom In, Zoom Out and Actual
  Size and their keys step the comparison's code font through the core steps,
  and stop at the clamps with the reason shown. The panes, the gutter (wider
  for the digits), the tab stop and the Detail pane all follow. Code size is
  unchanged, and the drawer's snippets keep Code size.
- **C2 (direct).** Keys do not collide. Each zoom key is bound once in the
  assembled window and in the desktop menu bar. ⌘0 still opens Home, and ⌘0
  inside the label menu still chooses Plain while ⌘⌥0 there does not. The
  AltGr claim keeps a typed `}` in an editor. ⌘-scroll and pinch step the
  zoom, and a plain scroll still scrolls.
- **C3 (direct).** Zoom persists per profile through the appearance host and
  store: a version-8 store migrates to 100%, a zoomed profile reopens zoomed,
  Settings shows and sets the same value, and Type's Reset returns it to 100%.
- **C4 (direct).** A visible current line keeps its viewport position across a
  zoom step, and the indicator and the announcement report the new
  percentage and size.
- **C5 (direct).** Zoom In, Zoom Out and Actual Size on the 20,000-line large
  file in the optimized large-review benchmark each stay within the 100 ms
  input target.
- **C6 (owner).** The owner looks at a frozen build and answers Accept or Reject
  for "diff zoom".

## Implementation and verification — 2026-10-08

The `frontend-design` skill was loaded before the design work, as the owner
asked on 2026-10-06. Product commit `7a35a4bed` on `main`, after BPR-1328
v002, BPR-1335 and BPR-1212 slice 1. The comparison's `eventFilter` takes the
zoom gesture before the line cursor's keys; neither claims the other's
events.

- **Core.** `code_font_settings::ZoomPercent`, `typography_field::code_zoom`
  (50 to 300), `code_zoom_steps`, `zoomed_point_size`, `stepped_code_zoom`
  and `code_zoom_steps_from`.
- **Host and store.** `step_code_zoom_intent`, stepped by the appearance host
  from the record it holds. The appearance store is at schema 9
  (`code_zoom_percent`, CHECK 50 to 300). A version-8 or version-7 file
  migrates to 100%, and an out-of-range value is refused whole.
- **Qt.** `appearance_profile::apply_comparison_code_font_to` applies the
  zoomed font and tab stop to the comparison only. The window has the three
  View commands with core-decided state and reasons. The resident desktop bar
  mirrors them, disabled. The comparison scroll controller lets a
  Control-modified wheel through, and the comparison turns ⌘-scroll and pinch
  into steps. The comparison notes the reader's line on its own
  `FontChange`, before its panes take the font. Each pane keeps that line by
  whole lines in the same turn, and the remeasured axis places it exactly
  (`ReadingPositionsHeld`). The zoom pill and the polite announcement show
  each step. Settings has the Diff zoom row.
- **Drawer snippets keep Code size.** The drawer is a narrow column beside the
  code, and zooming the comparison to read a long line should not reflow it.
  The owner can ask for them to follow.

**C1, unattended, pass.** `qt_workbench_window_scale_{100,125,200}`
(`test_code_zoom_commands_keys_and_collisions`): the three commands, their
order in View, their states and reasons at 100%, 300% and 50%, and dispatch
by key with the keyboard in a code pane. `qt_diff_workbench_scale_{100,125,200}`
(`test_comparison_zoom_scales_and_holds_the_reading_line`): both panes and
the detail pane at 150%, the tab stop scaled, the gutter wider and still
holding its four digits, and `code_font()` unchanged.
`application_diff_zoom_public_path` (new, real composition, inline-drawer
1557-line file): two Zoom Ins show both panes at 125%, persisted and shown in
Settings with Code size unchanged; ⌘-scroll steps back down; Actual Size
returns.

**C2, unattended, pass.** Every `QAction` key in the assembled window and on
the desktop bar is bound once, and no widget `QShortcut` takes a zoom key or
⌘0. ⌘0 still triggers Home and no zoom. ⌘⌥0 is Actual Size and not Home.
`qt_feedback_label_controls`: inside the open label menu, ⌘⌥0 is not claimed
and leaves the label, and ⌘0 is still Plain. `editor_keeps_typed_text`
claims AltGr+0 with `}` in an editable line or multi-line editor, and not
with no text, in a read-only field or in a code pane. The filter is
installed on Windows only, and Windows is not run here. ⌘-scroll steps, a
trackpad's small deltas add up, and the connector zooms. ⌘-scroll neither
scrolls nor zooms a pane by itself, and a plain scroll still scrolls. Pinch
steps at 0.15.

**C3, unattended, pass.** `sqlite_appearance_settings_storage`: every step
round-trips, a version-8 or version-7 file migrates to 100% and keeps every
other value, and 49 and 301 are refused. `application_appearance_host`: two
gestures are two steps (125%), Code size is untouched, the zoom persists and
is published as typography, stepping stops at 300% with no store, the zoom
survives a new host, and the code-font reset returns 100%.
`qt_appearance_settings_panel`: the Diff zoom row sits under Code size with
"125% (16.25 pt)" items, choosing 200% offers exactly that, and a Code size
gesture carries the zoom. `application_profile_startup_public_path`: Work
zooms to 125%, Default stays at its size, and Work reopens at 125%.

**C4, unattended, pass.** The reader's line at y 127 stays within one row
(132) in the same turn and after remeasurement at 150%, and returns at
Actual Size. The pill reads "Code zoom 150%, 13.50 pt", the announcement
reads "Code zoom 150%, 13.50 points", and the pill leaves by itself after
about 1.2 s. Offscreen renders in four themes at 67%, 100% and 150%, and the
Type card, are in the frozen root's `renders/`. Design pass on those renders:
the pill's size text first used the placeholder grey, which failed contrast
on High Contrast Light, so both texts now use the window text colour. The
pill was also enlarged.

**C5, unattended, pass.** `application_large_review_responsiveness` gains
`diff_zoom_in`, `diff_zoom_out` and `diff_actual_size_from_125` on the
20,000-line comparison. Each command's dispatch and every turn of the
remeasurement after it are held to the 100 ms target, and completion to
1,000 ms. On the optimized tree (the frozen Release build of `7a35a4bed`,
macOS 26.6.2 arm64, Qt 6.11.2), two runs pass every enforced budget:

| Zoom phase | Dispatch | Largest input gap | Completion |
|---|---|---|---|
| Zoom In | 6.0 and 5.1 ms | 17.0 and 15.5 ms | 348 and 339 ms |
| Zoom Out | 3.8 and 3.7 ms | 13.5 and 12.7 ms | 347 and 334 ms |
| Actual Size from 125% | 6.1 and 5.8 ms | 15.7 and 14.9 ms | 387 and 342 ms |

The pre-rebase build `7092c1dca` measured the same, to within 1 ms of
dispatch and 2 ms of gap. The machine's load average was 40 to 50 during
these runs, because other worktrees were building. A Debug run at a load
average of about 93 failed nearly every phase, the zoom phases with them, and
composition startup took 2.5 s against its 1 s limit. That run is load and is
not recorded as a result.

**Failing-before controls.** Against the base, the new tests do not compile:
the zoom API is absent. Each mutation below was applied in this worktree
alone, its row was run, and the source was restored. Each row failed where
expected:

- Notes no reading position → "in the same turn, the reader's line is still
  where it was".
- Skips the post-remeasure placement → "and stays there once the geometry is
  remeasured: 127 to 51".
- The controller consumes ⌘-scroll → "a ⌘-scroll notch away zooms in one
  step".
- Actual Size on Ctrl+0 → "no keyboard command is claimed twice in the
  assembled workbench".
- The store drops the zoom on read → "and loads back exactly: 50%".
- The core steps without the clamp check → "Zoom In is spent once the size
  is at the point-size maximum".
- The host ignores the step → "two gestures are two steps".
- The label menu matches ⌘ with any modifier → "inside the open menu ⌘⌥0 is
  not the menu's key".
- A 150 ms stall in the zoom turn on files over 10,000 lines →
  `diff_zoom_*_dispatch` 155 ms and `_input_gap` 166 ms over the 100 ms
  target, "Mac fixture responsiveness budget exceeded".

**Also passed.** These pass at `7a35a4bed` (dev): `architecture_boundaries`,
`test_network_isolation_manifest`, `core_appearance_policy`,
`application_appearance_{host,profile_consumers,gesture_contract,store_fallback_contract,theme_customization_contract}`,
`qt_appearance_{profile,settings_panel}`, `qt_settings_window`,
`qt_composed_github_scale_100`, `qt_profile_management_window`,
`application_profile_{startup,management}_public_path`,
`application_diff_workbench_public_path`,
`application_change_arrows_across_files` and
`application_large_review_{benchmark,responsiveness}`.
`coding-format-check` passes. `tidy.py` on the changed translation units
reports only two warnings, both already present in
`appearance_settings_database.cpp`. `verify_documentation.py`:
`DOCUMENTATION_OK`.

**Owner look (C6) pending.** This is a fresh short session,
`2a0daa3bc755d694f88a49274b1484cf`, with state in
`/Users/jaeger/.mask/bpr-1336-diff-zoom-20261008-v002/attended-review` and
the cursor at 4. Its one ready, protected Accept/Reject candidate is
`BPR-1336-diff-zoom-v002` ("diff zoom", fingerprint `8a4dd304…`). The
detached host is at `http://127.0.0.1:52768/` (the token is in the
handoff, not here). The frozen root `artifacts/bpr-1336/diff-zoom-frozen-v002/`
holds the exported source of `7a35a4bed` (archive `351f0dbd…`), its Release
build (binary `86c6e7a2…`) and `launch.py` (`c091cb3d…`). Its public course
passes 1/1, and its attended entry reaches ready offscreen in 2.7 s. Start it
with:

`python3 /Users/jaeger/Documents/GitHub/BetterPullReview/artifacts/bpr-1336/diff-zoom-frozen-v002/launch.py`

An earlier session, `abb2042c` (v001), was built from the pre-rebase
`7092c1dca`. It was withdrawn before anyone opened it: its host is stopped,
its protections are released and its frozen root is deleted.

## Windows follow-up (bpr-main-win, 2026-10-08): the zoom indicator was read after it had gone

On Windows `application_diff_zoom_public_path` failed in the Debug tree at "the
indicator says 125%" and passed in the optimized tree (9.8 s).

**Cause: the test's timing, not the product.**

- The zoom indicator hides 1200 ms after each step (`ZoomIndicatorTimer`, by
  design).
- The course checked it only after waiting for the zoom to persist and the
  1557-line comparison to remeasure.
- A temporary probe, not committed, measured that wait at 2509 ms in Debug.
  By then the pill had shown "Code zoom 125%" and hidden itself.
- Windows users see the indicator as designed. Both panes reached 125%, and
  the reader's line held.

**Fix** (`tests/end_to_end/diff_zoom_public_path_tests.cpp`). The course
records whether the indicator was visible saying "Code zoom 125%" at any
moment while it waits, then asserts that.

**Evidence:**

| Run | Result |
|---|---|
| Debug, with the fix | passes (21.9 s) |
| Release, with the fix | passes (9.6 s) |
| Control: the product never shows the pill (`ZoomIndicator->show()` removed), Debug | fails at "the indicator says 125%" |

## Owner decision, session 2a0daa3b (2026-10-09)

Session `2a0daa3bc755d694f88a49274b1484cf`, read from cursor 0 to event 6
(handed back).

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-1336-diff-zoom-v002` ("diff zoom") | Accept (`e2ae12e4`, 01:22:49Z) | (none) |

The verdict was given on the frozen Release build of `7a35a4bed` (binary
`86c6e7a2…`, fingerprint `8a4dd304…`). It satisfies `BPR-1336:C6`, asked as
one question. C1 to C5 stand on their recorded evidence at `7a35a4bed`, with
the Windows follow-up above. The drawer's snippets keep Code size, as built.
The card is Done.

The session's candidate was released (`resume-development --all`, event 7).
The host was already stopped. The frozen root
`artifacts/bpr-1336/diff-zoom-frozen-v002` was deleted.

