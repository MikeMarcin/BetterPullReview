---
schema: "implement-rfc.card.v1"
id: "BPR-1535"
title: "Home moves to Command-Shift-H (Ctrl+Shift+H) and Actual Size to Command-0 (Ctrl+0)"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Move Home from Ctrl+0 (Command-0) to Ctrl+Shift+H (Command-Shift-H), and move Actual Size (reset code zoom) from Ctrl+Alt+0 (Command-Option-0) to Ctrl+0 (Command-0). Command-backtick was considered and rejected: macOS uses it to cycle windows.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0032","RFC-BPR-0052"],"related_cards":["BPR-0650","BPR-1336","BPR-1080"],"reservation":"docs/issues/_reservations/BPR-1535--BPR-1535--e5f2afb941c7.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0c450bcfa894aee1becb37669139d41a9cc78fad","evidence":{"kind":"command","detail":"qt_workbench_window_scale_100/125/200: Ctrl+Shift+H through the window system shows Home from Overview, Review, Discuss, Threads and a focused editable editor (unclaimed, types nothing); Ctrl+0 shows Home from none; tooltip names the key. qt_composed_github_scale_100: Back to the review row shows Home's key. application_diff_zoom_public_path: Cmd-Shift-H shows Home in the composed app. Negative control with the old bindings fails."}},{"criterion":"AC2","outcome":"pass","revision":"0c450bcfa894aee1becb37669139d41a9cc78fad","evidence":{"kind":"command","detail":"Actual Size is Ctrl+0 only; Ctrl+Alt+0 bound to nothing and sends nothing; composed app Cmd-0 resets 110% to Code size (application_diff_zoom_public_path); resident bar Actual Size Ctrl+0 (qt_profile_management_window); Settings Diff zoom row names Ctrl+0 (qt_appearance_settings_panel); label menu keeps Cmd-0 as Plain (qt_feedback_label_controls)."}},{"criterion":"AC3","outcome":"pass","revision":"0c450bcfa894aee1becb37669139d41a9cc78fad","evidence":{"kind":"command","detail":"No key bound twice and no widget QShortcut takes Ctrl+0 or Ctrl+Shift+H in the assembled window; Qt standard bindings use Ctrl+H/Meta+H, never Ctrl+Shift+H; macOS has no system Command-Shift-H. Format 22 clean, tidy 22 clean on changed lines, architecture_boundaries and documentation gate pass."}},{"criterion":"AC4","outcome":"pass","revision":"0c450bcfa894aee1becb37669139d41a9cc78fad","evidence":{"kind":"observation","detail":"Owner Accept, no note, in attended session 96a82a58bc3aad238c97b09cc3cc2b22, decision b9df337d-69f1-41b0-b4e9-a32f108bf44d (2026-10-16T01:37:35Z, event 5), on candidate BPR-1535-home-key-v001 fingerprint 6067f20c, frozen root bpr1535-home-key-v001.1htuhc7c. Merged after BPR-1531, BPR-1536 and BPR-1532 onto main through 1dde04281: 28/28 focused and shared window/menu rows pass."}}]
---

# BPR-1535: Home moves to Command-Shift-H and Actual Size to Command-0

The owner asked on 2026-10-15 to move Home from Command-0 (`Ctrl+0`) to
Command-Shift-H (`Ctrl+Shift+H`), and to give Command-0 to Actual Size, the
comparison's zoom reset ([[BPR-1336]]), which had Command-Option-0
(`Ctrl+Alt+0`) because Command-0 was Home. Command-backtick was considered
for Home and rejected: macOS cycles an application's windows with it.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)
AC-06, the Home button in the strip ([[BPR-0650]]), recorded in the
[workbench specification](../../ux/workbench-specification.md); Home's
return row from [RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md);
and the zoom commands of [[BPR-1336]] in the specification and
[appearance and accessibility](../../ux/appearance-and-accessibility.md).
Inside the open label menu Command-0 stays Plain comment ([[BPR-1080]]).

## Acceptance

- AC1: Command-Shift-H (`Ctrl+Shift+H`) delivered through the window system
  shows Home from Review and from the other review surfaces, also with the
  keyboard in a comment editor, and Command-0 no longer shows Home. Home's
  tooltip and the Back to the review row name the new key in the
  platform's notation. A check against the old binding fails.
- AC2: Command-0 (`Ctrl+0`) is Actual Size in the window and on the
  resident desktop menu bar, resets a moved zoom, and does nothing at 100%.
  The window has no Command-Option-0 binding afterwards (replace, not add).
  The Diff zoom row in Settings names Command-0. Inside the open label menu
  Command-0 is still Plain and outside it is the window's.
- AC3: No other action, widget shortcut or standard text-editing key in the
  assembled window takes `Ctrl+Shift+H` or `Ctrl+0`; neither is a macOS
  system or menu key the application would collide with.
- AC4 (human): The owner accepts the frozen candidate: "⌘⇧H goes Home and
  ⌘0 resets zoom".

## Implementation

Qt presents: the bindings live in `workbench_window`, the resident
`desktop_menu_bar`, the Home landing's return row and the appearance
panel's zoom hint. No core, host, provider or storage change. Historical
cards, checkpoints, design frames and release notes keep the keys they
shipped with. Never drive or capture the owner's desktop; the owner checks
the frozen build themselves.

## Checkpoint — candidate v001, 2026-10-15

Product candidate `0c450bcfa894aee1becb37669139d41a9cc78fad` on branch
`bpr-1535-home-key` (worktree
`/Users/jaeger/.codex/worktrees/bpr1535-home-key/BetterPullReview`). Not
merged to main until the owner accepts. Paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1535/handoff.json).

Decisions. Actual Size does not keep `Ctrl+Alt+0` as a second key: the
owner asked to move it, no test or specification needed the old key, and on
Windows `Ctrl+Alt+0` is also AltGr+0, which types `}` on German and Nordic
layouts. The Windows AltGr yield (`editor_keeps_typed_text`) stays, because
it guards any `Ctrl+Alt` window key, not only Actual Size's. Home is
`Ctrl+Shift+H` alone.

Collisions. Before the change `Ctrl+0` had two users: Home's window action
and the open label menu's Plain comment ([[BPR-1080]]), which claims it in
`ShortcutOverride` as a popup; the label menu keeps it. `Ctrl+Shift+H` had no
user: no action, `QShortcut`, key handler or mnemonic in `src/` names it, and
Qt's standard key bindings use `Ctrl+H` (Replace on Windows and GNOME) and
`Meta+H` (Backspace on macOS) but never `Ctrl+Shift+H`, so no text widget
claims it. On macOS ⌘⇧H is no system shortcut (⌘H hides, ⌥⌘H hides others);
Safari and Finder use it for Home. Home is not in a native menu, so it has no
AppKit key equivalent. Actual Size is in View, so ⌘0 becomes its key
equivalent; Qt's `menuHasKeyEquivalent` (6.10.0 source) first offers the key
to the focus object as `ShortcutOverride`, and an open label menu is a key
window that claims ⌘0, so Plain should still win there. That last point was
read from Qt's source, not observed on the native menu bar: the cocoa course
would need the owner's desktop.

| Acceptance | Evidence at `0c450bcfa` | Result |
|---|---|---|
| AC1 | `qt_workbench_window_scale_100/125/200`: `test_home_key_shows_home_from_every_surface` delivers ⌘⇧H through the window system from Overview, Review, Discuss and Threads and from a focused editable editor (which does not claim it and types nothing); ⌘0 shows Home from none; the tooltip names the key. `qt_composed_github_scale_100`: Home's key is `Ctrl+Shift+H` and the Back to the review row shows its native text. `application_diff_zoom_public_path`: ⌘⇧H shows Home in the composed app. Negative control: the old bindings fail "⌘= and ⌘+ zoom in, ⌘− zooms out and ⌘0 is Actual Size" and "⌘⌥0 is no longer Actual Size, nor Home" | Pass |
| AC2 | `test_code_zoom_commands_keys_and_collisions`: Actual Size is `Ctrl+0` only, ⌘0 at 100% sends nothing, ⌘0 after a move is one Actual Size step and not Home, ⌘⌥0 sends nothing and no action binds it. `application_diff_zoom_public_path`: in the composed app ⌘⌥0 triggers nothing and ⌘0 returns both panes from 110% to Code size, persisted. `qt_profile_management_window`: the resident bar's Actual Size is `Ctrl+0`. `qt_appearance_settings_panel`: the Diff zoom row names Actual Size's `Ctrl+0`. `qt_feedback_label_controls`: inside the open menu ⌘0 is Plain, ⌘⌥0 is not; outside it the body does not claim ⌘0 | Pass |
| AC3 | Same zoom test: no key bound twice in the assembled window, no widget `QShortcut` takes `Ctrl+0` or `Ctrl+Shift+H`; source and Qt binding audit above | Pass |
| AC4 human | Session `96a82a58bc3aad238c97b09cc3cc2b22`, candidate `BPR-1535-home-key-v001`, fingerprint `6067f20c…`, ready and protected at cursor 4; frozen root `bpr1535-home-key-v001.1htuhc7c` | Pending |

Gates: clang-format 22.1.8 dry run over the 14 touched C++ files is clean;
clang-tidy 22 `--warnings-as-errors` reports nothing on changed lines (six
findings on unchanged lines of `appearance_settings_panel_tests.cpp`,
`feedback_label_controls_tests.cpp` and `diff_zoom_public_path_tests.cpp`
predate this card); `architecture_boundaries` passes; documentation gate
`DOCUMENTATION_OK` and its 67 tests pass. `test_review_cycle_two_round_public_path`
(its Home key changed to ⌘⇧H) is not part of the dev graph here, so it was
not built.

Next: on hand-back after cursor 4, an Accept rebases `bpr-1535-home-key` onto
main (BPR-1531, BPR-1532 and BPR-1536 also edit `workbench_window.cpp` and
the key table), reruns the focused tests, merges and finishes the card; a
Reject's note becomes v002.

## Closeout — 2026-10-16

**Owner decision, 2026-10-16T01:37:35Z: Accept**, no note (decision
`b9df337d-69f1-41b0-b4e9-a32f108bf44d`, event 5 of session
`96a82a58bc3aad238c97b09cc3cc2b22`), on candidate
`BPR-1535-home-key-v001` at `0c450bcfa`, fingerprint `6067f20c…`. The
session was handed back at event 6 with nothing pending; event cursor
consumed: 6.

BPR-1531, BPR-1536 and BPR-1532 landed first, then the shared text-input
predicate (`1dde04281`). `main` merged into the branch at `08b7dc614`. Two test
files conflicted: the resident View menu's zoom rows now sit two further down
after First and Last change, with Actual Size on Ctrl+0; the window's
no-duplicate-key check keeps BPR-1532's tour-outline exception with Actual
Size on Ctrl+0. BPR-1536's "keys that do not type" check had counted
Command-Option-0, which this card unbinds; it now counts Next file's
Command-Option-Down. The final key set has no collision: ⌥A, the Option-letter
keys, ⌥⇧↑/⌥⇧↓ (shared only with the tour outline, which the window yields to),
⌘⇧H and ⌘0 are each bound once in the assembled window, the resident menu bar
binds no key twice, and inside the open label menu ⌘0 stays Plain. On the
merged tree (`dev`, Debug): `qt_workbench_window_scale_{100,125,200}`,
`application_diff_zoom_public_path`, `qt_composed_github_scale_100`,
`qt_profile_management_window`, `qt_feedback_label_controls`,
`qt_appearance_settings_panel`, `architecture_boundaries`,
`qt_review_state_icon`, `qt_option_keys_typing`, `qt_change_end_keys`,
`qt_files_tree`, `qt_home_list_scale_100`, `qt_appearance_profile` and the
BPR-1531/1532/1536 rows pass 28/28; clang-format 22.1.8 is clean and
clang-tidy passes on the two resolved test files; the documentation gate
prints `DOCUMENTATION_OK`. `test_review_cycle_two_round_public_path` is still
outside the dev graph and was not compiled. The review server (pid 70256) was
stopped, the candidate's protection released and the frozen root removed.
