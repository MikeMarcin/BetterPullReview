---
schema: "implement-rfc.card.v1"
id: "BPR-1815"
title: "The review bar toggles one pane or two and goes to header/source"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1815--BPR-1815--01dfce242fdf.json","kind":"feature","found_by":"owner, 2026-10-29","related_cards":["BPR-1724","BPR-1416","BPR-1578","BPR-1295","BPR-1255","BPR-1700"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "197e22387a74ba897111c1fb8551ab6f4f7444c7", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(qt_workbench_.*|application_github_public_path_uiforetw_74|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_header_source_navigation|architecture_boundaries)$'", "detail": "All pass (macOS 26 arm64, Qt 6.12 offscreen, dev Debug). test_the_review_bar_presents_the_layout_and_header_source: workbenchOnePaneButton and workbenchHeaderSourceButton are the review bar's presenters of one_pane_action and header_source_action, icon-only, strong focus, right of the progress track on its row, the next two focus stops after private progress; a press sends exactly one toggle_workbench_one_pane_intent and the button keeps the projected state; Go to header/source's press is the window's header/source dispatch; one pane published checks the toggle, swaps its glyph and reaches the comparison surface; refused commands disable both. The window-tests and the application's navigation-row probe pin the bar's 19-action order."}}, {"criterion": "AC2", "outcome": "pass", "revision": "197e22387a74ba897111c1fb8551ab6f4f7444c7", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(qt_keyboard_shortcuts_scale_(100|125|200)|qt_workbench_window_scale_(100|125|200)|qt_workbench_focus_order_scale_(100|125|200))$'", "detail": "All pass. shortcut_command::one_pane is Alt+Shift+P in the catalogue, on toggleWorkbenchOnePaneAction and desktopToggleOnePaneAction; the window's assembled-key uniqueness checks pass with it; Option-Shift-P sent through QWindowSystemInterface dispatches the toggle; tooltips start 'One pane (⌥⇧P)', 'Two panes (⌥⇧P)' and 'Go to header/source (⌥O)', named through presented_key so typing's lifted Option keys keep their names."}}, {"criterion": "AC3", "outcome": "pass", "revision": "197e22387a74ba897111c1fb8551ab6f4f7444c7", "evidence": {"kind": "command", "command": "BPR_QT_RENDER_PATH set by qt_workbench_window_scale_{100,200}", "detail": "Bar and window renders, light and dark, two panes and one pane, at 1x and 2x, saved by the new course and kept outside the repository in the implementing session's scratchpad (renders/bar-*.png, renders/window-*.png)."}}, {"criterion": "AC4", "outcome": "pass", "revision": "197e22387a74ba897111c1fb8551ab6f4f7444c7", "evidence": {"kind": "command", "command": "cmake --build build/dev --target coding-format-check; ctest --test-dir build/dev -R '^(command_correctness_registry_contract|application_review_round_trip_composition_contract)$'; python3 scripts/verify_documentation.py --root .", "detail": "clang-format 22.1.8 passes on 1860 files. Round-trip full and manifest pins resealed (github_application.cpp's navigation-row probe only, then recomputed over the BPR-1816 merge); mutation pin 16d135419 unchanged. Workbench test-source digest resealed in the registry and case inventory; default case count 121. Documentation gate run with the main checkout's script."}}]
---

# BPR-1815: The review bar toggles one pane or two and goes to header/source

The owner, 2026-10-29, as relayed by the coordinator: a toggle button for the
one-pane (unified) and two-pane (side-by-side) comparison, with a hotkey, on
the review bar's row with Seen, Read and Accept; and "go to header/source"
there too. It need not be the same toolbar; the implementer chooses, with the
frontend design skill.

## Acceptance

- AC1. The review bar presents a layout toggle and Go to header/source on the
  row with Seen, Read and Accept. Both are the View menu's existing actions,
  so the core still decides the layout and the header/source jump, and no
  widget performs an effect.
- AC2. One pane has a catalogue key that collides with no other binding, and
  each button's tooltip names its command and key.
- AC3. Focused tests next to the existing workbench window tests; offscreen
  renders of the bar, light and dark, in both layouts, outside the
  repository.
- AC4. Format (clang-format 22), the touched ctest targets and the
  documentation gate pass; the round-trip composition pins are resealed if
  `github_application.cpp` changes.

## What existed

- The layout is already the core's: `workbench_one_pane` state,
  `set_workbench_one_pane_intent` / `toggle_workbench_one_pane_intent`, the
  `toggle_one_pane` command projection and `workbench_one_pane_delta`
  (BPR-1724). The window's View > One pane action (`OnePaneAction`) dispatched
  the toggle and the comparison surface presented `set_one_pane_enabled`. It
  had no key.
- "Go to header/source" is View > Go to header/source (`HeaderSourceAction`,
  `shortcut_command::header_source`, Option-O, BPR-1416), resolved by
  `core::header_source_navigation` and dispatched by the composition. The
  owner's "go to HEAD source" is read as this existing command; no command
  opens a file at the head revision as such (see Uncertain below).

## Design

Loaded `frontend-design:frontend-design`. The bar already has one visual
vocabulary: 22-pixel bordered strip buttons, painted 2-pixel line glyphs in the
palette's button ink, accent rails under the groups that move through the
review, and strip rules between kinds of control. The view controls follow it
rather than adding a new look:

- Placement: right of the progress track, after a strip rule, before the
  spacer. They say how the file is shown, next to what the reviewer has done
  with it, on the same row as Seen, Read and Accept, and they carry no rail
  because they do not move through the review.
- The layout toggle is the View menu's checkable One pane action presented as
  an icon button. Its glyph shows the layout on screen, a frame parted down
  the middle (two panes) or the same frame whole with full-width rows (one
  pane), as the title row's Hide drawers glyph shows the drawers on screen; the
  frame is the drawers glyph's frame, so the two read as one family. It is
  checked in one pane, as the menu item is.
- Go to header/source is the View menu's action as an icon button: two pages
  with folded corners and an arrow headed both ways between them.
- Tooltips use the arrows' form (BPR-1328): the first line is the command
  and its key in native notation ("One pane (⌥⇧P)", "Two panes (⌥⇧P)",
  "Go to header/source (⌥O)"); the second says what the layout is or why the
  command is unavailable. The key is read through `presented_key`, so it stays
  named while typing lifts Option keys from the macOS menu bar (BPR-1536).

## Key

`Alt+Shift+P` (⌥⇧P): P for pane, among the Option-Shift view keys
(Alt+Shift+1 to 6, A, R, T, U). Nothing in the catalogue, the desktop menu
bar or a widget's own key handling binds it. The key is the catalogue's
`shortcut_command::one_pane` row (group Views, title "One pane or two
panes"), bound on the window's action and the resident desktop menu bar's
mirror (`desktopToggleOnePaneAction`), so Help > Keyboard Shortcuts lists it.
On macOS, where Option-Shift-P types a character, the existing BPR-1536 rule
lifts it from the menu bar while a text field has the keyboard.

## Width

The two buttons and their rule add 85 pixels to a bar that had about 39
spare at 1280 and 28 at the 900 floor. Nothing folds; the existing yields
take it:

- At 1280 (the default window) the comparison's dates now yield first, as
  the yield order always had them; the positions, "Approve…" and the whole
  pull request name still show. The dates return by 1366.
- The floor follows the bar with everything yielded ([[BPR-1255]]): with the
  test fixtures' summary it is 957 on macOS rather than the platform's 900
  (902 on Windows). Tests now pin the rule, `max(platform, yielded bar)`,
  rather than the old coincidence that the bar fitted inside 900.

A single toggle alone (34 pixels) would have kept the dates at 1280 but would
still have lifted the floor by a few pixels. Both commands were asked for, so
both stay.

## Built

- `shortcut_command::one_pane` (Alt+Shift+P) in the catalogue; the window's
  `OnePaneAction` and the desktop menu bar's mirror take it.
- `strip_glyph::two_panes`, `one_pane` and `header_source`.
- The review bar: `reviewBarViewDividerAction`, then `OnePaneAction` and
  `HeaderSourceAction` as `workbenchOnePaneButton` and
  `workbenchHeaderSourceButton`; in the declared focus order after private
  progress; repainted on a theme change.
- Tooltips through `describe_navigation_action` with `presented_key`.
- `docs/ux/workbench-specification.md`: the layout line, the review bar
  paragraph, the shortcut table, the 1280 and floor facts.

## Uncertain

- "Go to HEAD source" is read as the existing View > Go to header/source
  command, whose name matches the request almost word for word. If the owner
  meant opening the file as it stands at the head revision (outside the
  comparison), that is a new command and a follow-up card.
- The width trade-off above changes facts the owner accepted earlier: the
  dates at 1280 (BPR-1700's render) and the 900 floor. It wants the owner's
  Accept or Reject on a frozen build; none was opened by this card.
