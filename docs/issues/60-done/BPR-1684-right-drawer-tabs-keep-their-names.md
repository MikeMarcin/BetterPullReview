---
schema: "implement-rfc.card.v1"
id: "BPR-1684"
title: "Right drawer tabs keep their names while they fit, and go compact on purpose"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-20","owner_note":"When the right drawer in review gets too narrow for the full tab labels i cant tell what the tabs are easily, it looks broken. I cant tell if its because of the comments number or its just broken. Also the agents tab seems wider than necessary which might be forcing dropping to narrow labels earlier than necessary.","reservation":"docs/issues/_reservations/BPR-1684--BPR-1684--abfa7f826da7.json","related_cards":["BPR-1134","BPR-1290","BPR-1511","BPR-1592","BPR-1594"],"requested_skill":"frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ae49e2777","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R qt_current_file_unread_badge; QT_SCALE_FACTOR=2 better_pull_review_qt_workbench_window_tests --current-file-unread-badge","detail":"test_the_right_tray_tabs_keep_their_names_while_they_fit drags the drawer through 760, 420, 375, 340, 335, 330, 320, 310, 300, 290 and 280 pixels with and without a count, in the application's style and in every QStyleFactory style (macOS, Windows, Fusion), and requires every name exactly while the row of names fits the room the header's grid gives the tabs. The names need 299 px (macOS) or 307 px (Fusion, Windows) without a count and 321 or 329 px with one; the tools wrap below first. Before: names went at a fixed 340. 1x and 2x pass. Failing-before control: with workbench_window.cpp and .hpp at df2244530 the case fails at 'the Comments tab keeps no room for a count it is not showing (app-fusion, no count)'."}},{"criterion":"AC2","outcome":"pass","revision":"ae49e2777","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R qt_current_file_unread_badge","detail":"Under every style the Agent tab is its spark (16 px), a 4 px gap and its name, padded as Notes is (within 1 px); Tour is padded as Notes; Comments is its name plus its count and a gap when the count shows, and keeps no room for a hidden count. Each name's own style text rectangle (a plain text tab as wide as its tab) holds the glyph, the name and the count, so no style elides it. Measured macOS-style metrics: Agent 115 px before, 85 px after; Comments without a count 116 before, 93 after; the row of names 352 before, 299 after (355 and 321 with a count)."}},{"criterion":"AC3","outcome":"pass","revision":"ae49e2777","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R qt_workbench_window_scale_; ctest --test-dir build/dev -R qt_current_file_unread_badge","detail":"Compact rows: every tab shows its glyph, only the open tab shows its name, whole; a glyph-only tab's tooltip starts with its accessible name and a newline, and every accessible name starts with the tab's name; the Comments count stays a pill inside its tab, beside the glyph; the whole row fits, nothing scrolls or is squeezed; choosing Agent in a compact row names it instead; widening brings every name back. The three window scales (with the existing right-tray case, whose wide-row names, tooltips and accessible names are unchanged) pass. coding-format-check (clang-format 22.1.8, 1695 files) passes; clang-tidy --warnings-as-errors reports nothing in workbench_window.cpp and only the two existing performance-avoid-endl findings in the test runner's case trace (main), none in this change."}}]
---

# BPR-1684: Right drawer tabs keep their names while they fit, and go compact on purpose

## Owner note

The owner, 2026-10-20, verbatim:

> When the right drawer in review gets too narrow for the full tab labels i
> cant tell what the tabs are easily, it looks broken. I cant tell if its
> because of the comments number or its just broken. Also the agents tab
> seems wider than necessary which might be forcing dropping to narrow labels
> earlier than necessary.

## Causes

Measured offscreen at 1x and 2x, with and without a count, in Fusion (the
offscreen style) and from the macOS style's own metrics (it draws nothing
offscreen, but measures).

- **A fixed switch.** `fit_tray_tabs` dropped every name below 340 pixels of
  header, whatever the names needed. They need 299 to 329 pixels, so between
  about 300 and 340 the row went to icons for no reason.
- **The Agent tab was wider than its content on macOS.** For a tab with an
  icon the macOS style centres the name and keeps the icon's width free on
  both sides, and its size hint counts the tab padding twice. The Agent tab
  was 115 pixels for 51 pixels of spark and name, about 30 more than a text
  tab's padding. That pushed the row of names to 352 pixels, over the 340
  switch, so on macOS the names went almost as soon as the drawer narrowed
  from its 375 default.
- **Room for a hidden count.** The count was always the Comments tab's
  button, and QTabBar sizes and lays out a hidden button too. So Comments was
  19 to 23 pixels wider than its name without a count, and every glyph sat off
  centre.
- **The compact row looked broken.** Each icon-only tab was forced to its
  icon plus 20 pixels while the style still laid the label out for a wider
  tab. The glyphs sat off centre, and the count, still the Comments tab's
  button, was drawn over the speech bubble at the tab's left edge. Notes was
  a padlock, which does not say Notes, and Tour had no tooltip. Nothing
  showed which tab was open except its shape.

## Change

`src/adapters/qt/src/workbench_window.cpp` only, the review tray's own
`tray_tab_bar` and `fit_tray_tabs`. The shared drawer parts
(`drawer_header`, `configure_drawer_tabs`, `drawer_count_pill` in
`drawer_parts.cpp`) are unchanged. The Overview's Status drawer uses them,
so a tab added there is not affected.

- **Each tab is as wide as what it shows.** That is the style's own text tab
  for its name, plus the glyph and a 4 pixel gap when it shows one, plus the
  count and a gap while the count shows. The count is the tab's button only
  while there is one.
- **The bar places each label.** The style draws each tab's shape and its
  name, as a plain text tab whose centre is the name's place. The bar draws the
  glyph, so the glyph and the name are one group, centred in the tab or ending
  a gap before the count. The macOS style's own label layout would elide a
  content-sized Agent tab's name to 15 pixels, so this is what lets the tab be
  its content.
- **Three faces, widest first.** The row takes the first that fits the room
  the header's grid gives the tabs alone on their row. The tools move to a row
  of their own before any name goes, as [[BPR-1290]] arranged.
  1. Every name, the Agent with its spark beside its name ([[BPR-1594]]).
  2. The open tab's glyph and name, and the others' glyphs. Choosing a tab
     names it instead.
  3. Glyphs only, where even that does not fit (280 pixels with the count and
     Comments open).
- **Glyphs:** a speech bubble for Comments, with its count as a pill beside
  it; a page for Notes (the glyph the Notes empty state already uses, in place
  of the padlock); the route for Tour; the spark for Agent.
- **Tooltips and names.** A tab showing only its glyph gives its accessible
  name first in its tooltip, on its own line, then what the tab holds, so
  Comments with new comments starts "Comments, 2 threads with new comments in
  this file". Tour gains a tooltip. Accessible names are unchanged, and
  Comments has one from the start.
- `docs/ux/workbench-specification.md` describes this in place of "Below 340
  pixels the tray's tabs show their icons only" and the stale "its spark
  alone".
  RFC-BPR-0085 D5's sentence about 340 pixels is left as the adopted
  decision's record.

### Why no core transition

Which face fits is text and glyph geometry at the Qt boundary, as eliding a
label is. It changes no state the core keeps, and no intent or selection.

## Verification

- `test_the_right_tray_tabs_keep_their_names_while_they_fit` (in
  `--current-file-unread-badge` and the default run, so 1x, 1.25x and 2x)
  covers AC1 to AC3. It runs in the application's style and in each
  QStyleFactory style, with and without a count. Failing-before control:
  with `workbench_window.cpp` and `workbench_window.hpp` at `df2244530`, it
  fails at "the Comments tab keeps no room for a count it is not showing".
- `qt_current_file_unread_badge` and `qt_workbench_window_scale_{100,125,200}`
  pass. The 2x selector run passes too.
- `coding-format-check` passes, and clang-tidy is clean on the change.
- The new case is in two selectors, so `tests/workbench_case_inventory.json`
  lists it under `--current-file-unread-badge` and `<default>`. Its source
  identity, the workbench family's source pin in
  `tests/command_correctness_registry.json`, and the default run's case count
  in `tests/scripts/command_correctness_registry_tests.py` (115 to 116) are
  resealed. `scripts/workbench_case_inventory.py --execute-lists` and
  `command_correctness_registry_contract` pass.
- Rebased onto `571d82928`, which had changed the same files. The rebased
  tree was rebuilt, and every row above was run again and passes.
- The documentation gate refuses only the provisional ID. With the ID set to
  a real one for a local check (and the counter past it), it printed
  `DOCUMENTATION_OK`.

The offscreen renders are Fusion. The native macOS look is the owner's,
below.

## Owner look

Two Accept or Reject questions on the frozen build are pending in their own
short attended session. They are recorded below and are not acceptance
criteria, because they wait on the owner.

- Session `83dd264a5c78cb8797e0cfccf83da201`, opened 2026-10-21 with fresh
  state `/Users/jaeger/.mask/bpr-pending-g-tray-tabs-v001-20261021/attended-review`.
  Both candidates were ready at event 6 and the loopback host was serving. The
  authenticated URL stays in the external state.
- Candidates, revision `frozen-04119f672-v001`, one question each:
  - `BPR-1684-compact-v001` (`1f1def2b37ee…`): the narrow tab row reads
    as intended, not broken.
  - `BPR-1684-widths-v001` (`b8db69fa2593…`): each tab is the right
    width, so the names last as long as they can.

  Each carries before and after offscreen renders at 1x or 2x, kept in
  `/Users/jaeger/.mask/bpr-pending-g-tray-tabs-v001-20261021/renders`.
- Frozen root `/Users/jaeger/.mask/attended-review/bpr-pending-g-tray-tabs-v001.feygvcwt`
  from `04119f672`, the change before its rebase onto `571d82928` (the rebase
  changed none of its source lines; fixture `inline-drawer-v1`, open file
  `src/ui/conversation_cards_view.cpp`). The executable's SHA-256 is
  `f9760d4654ab…9e9353`. The launcher's own check printed
  `CHECK_OK (first head): ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`.
  Launch with `python3 /Users/jaeger/.mask/attended-review/bpr-pending-g-tray-tabs-v001.feygvcwt/launch.py`.
- Verdicts, handed back 2026-10-21 at event 11: `BPR-1684-compact-v001` **Accept** (event 8) and `BPR-1684-widths-v001` **Accept** (event 10), both without a note. The session is closed, its host stopped and the frozen root removed.
