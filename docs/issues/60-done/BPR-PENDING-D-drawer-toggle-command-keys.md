---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-D"
title: "Files and Repositories drawers: a command key each, for automation"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-20","owner_note":"I would like a hotkey for the file drawer toggle. And maybe one for the repository drawer toggle. They dont need to be great hotkeys just something to help with automations.","provisional_id":"allocated at landing","related_cards":["BPR-0651","BPR-1536","BPR-1567","BPR-1676","BPR-1677"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1058b8908","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_keyboard_shortcuts_scale_(100|125|200)$'","detail":"test_drawer_keys_toggle_their_drawers through core::transition: Control-Command-1 and Control-Command-R each send one toggle_workbench_drawer_intent of their drawer from the code pane and from the files filter (its text unchanged); the drawer, the toggle's isChecked and the title-row button's accessible checked state change and the key again restores them; in Overview each key is the toggle's gesture and the toggle stays unchecked as the core keeps it; Option-Shift-1 and Option-Shift-R in the filter send nothing (negative control). Failing-before control: with keyboard_shortcuts.cpp and workbench_window.cpp at the previous revision the case fails at 'the catalogue keeps each drawer's Option-Shift key first and adds a command key'; with the catalogue change but the window still binding only the first key, test_shortcut_catalogue_is_what_the_window_binds fails with 'keys differ: toggleWorkbenchFilesDrawerAction'. macOS offscreen; the Ctrl+Shift branch for Windows and Linux is owed a station run, not gating."}},{"criterion":"AC2","outcome":"pass","revision":"1058b8908","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_keyboard_shortcuts_scale_(100|125|200)|qt_option_keys_typing|qt_change_end_keys|qt_workbench_window_scale_100)$'","detail":"No two window-wide catalogue entries share a key; the new keys are no other entry's in any place, no Qt standard key, not claimed by QLineEdit or QPlainTextEdit in ShortcutOverride, and in the assembled window only the two toggles bind them; the catalogue-against-window check, Option keys while typing, the change-end keys and the window suite (its no-shared-key check and the BPR-1022 Alt chords) pass. coding-format-check (clang-format 22.1.8, 1695 files) passes; clang-tidy --warnings-as-errors on keyboard_shortcuts.cpp, workbench_window.cpp and workbench_window_tests.cpp reports only two existing performance-avoid-endl findings in the test runner's case trace (b059676cc), none in this change. macOS."}},{"criterion":"AC3","outcome":"pass","revision":"1058b8908","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_keyboard_shortcuts_scale_(100|125|200)$'","detail":"Help > Keyboard Shortcuts' Files drawer and Repositories drawer rows show both keys joined by 'or', the accessible text names the spoken key ('Control-Command-1'), and the row has no typing note; docs/ux/workbench-specification.md's shortcut table lists both drawers with both keys per platform. macOS."}}]
---

# BPR-PENDING-D: Files and Repositories drawers: a command key each, for automation

## Owner note

The owner, 2026-10-20, verbatim:

> I would like a hotkey for the file drawer toggle. And maybe one for the
> repository drawer toggle. They dont need to be great hotkeys just something
> to help with automations.

## Before

The toggles already had keys: Files drawer `Alt+Shift+1` (⌥⇧1) and
Repositories drawer `Alt+Shift+R` (⌥⇧R), [[BPR-0651]]. On macOS both are
Option-Shift with a character, which types text (`⁄`, `‰`). Since
[[BPR-1536]] any widget that takes typed text claims those keys while it
has the keyboard: Home's Open bar, the files filter, Find, every comment and
note editor. A script cannot know where the keyboard is, so it cannot rely
on them. [[BPR-1676]]'s promotional capture clicks a fixed point on the title
row to hide Files for that reason.

## Change

Each toggle keeps its key and gains a second key: the same 1 or R with the
platform's command modifier, which no text field takes.

| Drawer | macOS | Windows and Linux |
|---|---|---|
| Files | ⌥⇧1 or ⌃⌘1 (`Ctrl+Meta+1`) | `Alt+Shift+1` or `Ctrl+Shift+1` |
| Repositories | ⌥⇧R or ⌃⌘R (`Ctrl+Meta+R`) | `Alt+Shift+R` or `Ctrl+Shift+R` |

- **Why these keys.** None is in the catalogue on its platform, none is a Qt
  standard key, and neither `QLineEdit` nor `QPlainTextEdit` claims them.
  macOS's Command-Shift-3 to 6 are the system's screenshots, so macOS uses
  Control-Command, which neither the system nor a text field uses with 1 or
  R. Elsewhere Ctrl+Alt is AltGr, which types text on many layouts, so those
  platforms use Ctrl+Shift. Keeping each drawer's existing 1 or R means
  only the modifier changes.
- **One catalogue entry each.** The keys are the `files_drawer` and
  `repositories_drawer` entries' second keys in `keyboard_shortcuts.cpp`.
  The window's drawer factory now binds every catalogue key
  (`setShortcuts(shortcut_keys(…))`) on the existing checkable toggle
  actions, so the key is the toggle's gesture. It goes to the core as
  `toggle_workbench_drawer_intent`, and the checked state, tooltip and the
  title-row button's accessible state follow what the core publishes. Neither
  toggle is in a menu, so the menu bar's Option-key lifting does not touch
  them. The first key, which the tooltip and `shortcut()` name, is
  unchanged.
- **Wherever the drawers are.** The actions are window-wide. In Review the
  key opens or closes the drawer. In Overview, which keeps every drawer
  collapsed, it is the same refused gesture as a click.
- **Help > Keyboard Shortcuts** lists "⌥⇧1 or ⌃⌘1" (and so on). On macOS a
  row said "Not while typing" when any of its keys was Option with a
  character; it now says so only when every key is, so these two rows carry
  no typing note. No other row has a mix. The workbench specification's
  shortcut table gains both rows.

### Why no core transition

The key only selects a gesture the core already decides
(`toggle_workbench_drawer_intent`). The binding and its keys are Qt-boundary
presentation, as every other catalogue key is.

## Acceptance

- **AC1.** ⌃⌘1 (Ctrl+Shift+1) and ⌃⌘R (Ctrl+Shift+R) each send one toggle
  of their drawer through the real core, from the code and from a
  single-line field, without typing there. The drawer, the toggle's checked
  state and its button's accessible checked state change, and the key again
  restores them. In Overview the key is the toggle's gesture and the toggle
  stays unchecked as the core keeps it. On macOS ⌥⇧1 and ⌥⇧R are still the
  field's (negative control).
- **AC2.** The catalogue has no conflict: no two window-wide entries share a
  key, the new keys are no other entry's in any place and no Qt standard
  key, no text widget claims them, and in the assembled window only the
  toggles bind them. The catalogue-against-window check still passes.
- **AC3.** Help > Keyboard Shortcuts lists both keys for each drawer, spoken
  as well, with no typing note. The workbench specification's shortcut table
  lists them.

## Verification

- `qt_keyboard_shortcuts_scale_{100,125,200}`:
  `test_drawer_keys_toggle_their_drawers` covers AC1-AC3 through the real
  core. Failing-before control: with the two source files at their previous
  revision the case fails at its first check (the catalogue has one key per
  drawer). With the catalogue change but the window binding only the first
  key, the catalogue-against-window check fails with "keys differ".
- `qt_option_keys_typing`, `qt_change_end_keys` and
  `qt_workbench_window_scale_100` pass with the change.
- `coding-format-check` (clang-format 22.1.8) passes. clang-tidy on the
  three changed units reports only two existing `std::endl` findings in the
  test runner's case trace, outside this change.

macOS only. Windows and Linux station runs of
`ctest --preset dev -R '^qt_keyboard_shortcuts_scale_100$'` are owed, not
gating: that branch is the same catalogue entry with `Ctrl+Shift`.
