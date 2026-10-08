---
schema: "implement-rfc.card.v1"
id: "BPR-1328"
title: "Change arrows continue to the next file instead of greying out"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"found_by":"owner chat note, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1328--BPR-1328--8bce629ab070.json","related_cards":["BPR-1202","BPR-1287"],"acceptance":["BPR-1328:C1","BPR-1328:C2","BPR-1328:C3","BPR-1328:C4","BPR-1328:C5"]}
parked_on: "human-review"
---

# BPR-1328: Change arrows continue to the next file instead of greying out

## Owner note

The owner, in chat on 2026-10-08:

> It doesn't feel good for next change arrows to get greyed out when there's only 1 change

## Behaviour before this card

The review bar carries two position groups, each a label and a pair of arrows.

- **File n of m, ‹‹ ››.** Previous file (`Alt+Up`) and Next file
  (`Alt+Down`) step through the filtered actionable order the workbench host
  publishes. With no file selected, Next file opens the first and Previous
  file the last. Under the production stop policy, Previous file is disabled
  on the first file ("This is the first actionable file.") and Next file on
  the last ("This is the last actionable file."); with an empty order both
  say "This pull request has no actionable files."
- **Changes n of m, ‹ ›.** Previous change (`Alt+PgUp`) and Next change
  (`Alt+PgDown`) step through the open comparison's semantic change groups.
  The core's `plan_change_navigation` decided them from the bounded change
  cursor alone: under stop, Previous change was disabled on the first change
  ("This is the first change in this comparison.") and Next change on the
  last ("This is the last change in this comparison."). A file with one change
  is both its first and its last change, so both arrows were grey there, which
  is the owner's note. With no comparison open both said "No file comparison
  is open."; with a comparison that has no changes, "This comparison has no
  changes to navigate."
- **Wrap.** `change_navigation_policy::wrap` makes Next file wrap to the first
  file and Next change wrap to the comparison's first change (and the reverse).
  Production always runs stop; only a fixture probe publishes wrap.

Follow-ups from the owner the same day, relayed by the coordinator:

- The owner chose to keep the existing keys and make them discoverable: every
  file and change arrow's tooltip names its shortcut in the platform's native
  notation ("Next change (⌥PgDn)", "Next file (⌥↓)"; Ctrl/Alt elsewhere).
- Then: "Actually can it be cmd+down for change or does that conflict". ⌘↓ and
  ⌘↑ (Ctrl+Down and Ctrl+Up on Windows and Linux) become Next and Previous
  change, with ⌥PgDn and ⌥PgUp kept as alternates. In text editors they keep
  their native document end and start; in the code panes the change wins.
- The v001 owner look (session `6b391cb7…`) was stopped unanswered when the
  owner changed their mind, verbatim: "I changed my mind let cmd+down scroll
  to the bottom and cmd+up to the top. Option left right can be next prev
  change".
- Before that was built, the owner settled it, verbatim: "You're right that
  conflict is real. Okay final choice: option cmd down next file. Option down
  next change". So ⌥↓/⌥↑ (Alt+Down/Alt+Up) are Next/Previous change, moved
  from the files; ⌥⌘↓/⌥⌘↑ (Ctrl+Alt+Down/Ctrl+Alt+Up) are Next/Previous
  file; ⌘↓/⌘↑ scroll the code to its bottom and top; ⌥←/⌥→ are not used; and
  ⌥PgDn/⌥PgUp stay as change alternates. In text editors ⌥↑/⌥↓ (paragraph
  moves) and ⌘↑/⌘↓ keep their native meaning.

## Change

Like Visual Studio Code's next change across files, the change arrows now
continue past the ends of a comparison:

- At a file's last change, Next change moves to the **first change of the next
  actionable file**; at its first change, Previous change moves to the **last
  change of the previous file**. The destination is exactly the file Next or
  Previous file would choose.
- The arrows are disabled only where there is nowhere to go: the first change
  of the first file ("First change in the pull request.") and the last change
  of the last file ("Last change in the pull request."). A one-change file
  between two others keeps both arrows live.
- When a press would cross, the tooltip names the destination: "Continue to
  the next file: docs/guide.md", "Go back to the previous file: README.md".
- A file with no changes passes the arrows through to its neighbour.
- The wrap policy keeps wrapping inside the comparison, as before.

The core decides the target. `plan_change_navigation` returns a crossing (the
file decision the file command would make, plus whether to enter at the first
or last change) on the command projection, and the new
`cross_workbench_change_intent` decides it again in `transition`. The window
dispatches that intent when the projection offers a crossing and otherwise
sends the unchanged RFC-BPR-0004 diff intent. The host resolves the file
decision exactly as for Next file. The comparison surface is told the
destination file and the entry end; the next accepted comparison of that file
is entered through `navigate_change` from no selection, and an accepted
comparison of any other file drops the request.

**Keys and tooltips (final, v002).** Next change is `Alt+Down` then
`Alt+PgDown`, Previous change `Alt+Up` then `Alt+PgUp`; Next file is
`Ctrl+Alt+Down` and Previous file `Ctrl+Alt+Up` (Qt's `Ctrl` is ⌘ and `Alt` is
⌥ on macOS). All four are window commands, live from anywhere in the window.
Each arrow's tooltip is "Name (key)" in `QKeySequence::NativeText` for its
first key: "Next change (⌥↓)", "Previous change (⌥↑)", "Next file (⌥⌘↓)",
"Previous file (⌥⌘↑)" on macOS; "Next change (Alt+Down)", "Previous file
(Ctrl+Alt+Up)" elsewhere. A second line names a crossing's destination
("Continue to the next file: …") or the disabled reason; the status tip is that
line, or the arrow's purpose. The No file open page's direction reads the
same key ("⌥⌘↓ opens the next file").

**Scrolling in the code.** In a code pane (either side of two panes, the link
map between them, or the one-pane comparison) the platform's
`QKeySequence::MoveToEndOfDocument` and `MoveToStartOfDocument` scroll the
comparison to its bottom and top and take no gesture: ⌘↓/⌘↑ on macOS (where
Qt also lists End/Home, which already did this), `Ctrl+End`/`Ctrl+Home` on
Windows and Linux, where Ctrl+Down/Ctrl+Up are not the document keys.

**Conflicts.**

- *In the window.* The one other holder of a new key was the Updates
  control: Load earlier epochs (`Ctrl+Alt+Down`) and Return to newest epochs
  (`Ctrl+Alt+Up`), scoped to the control and shown in the Comparing menu. Qt
  treats two live commands on one key as ambiguous and runs neither, and the
  window course already requires that no key is offered twice in the window
  (it failed on exactly this). So those two commands move to `Ctrl+Alt+PgDown`
  (⌥⌘PgDn) and `Ctrl+Alt+PgUp` (⌥⌘PgUp), the same family paged; no document
  named their old keys. That is a change to another feature's keys for the
  owner to see. The Hide Drawers course's duplicate-key table names `Alt+Up`,
  `Alt+Down`, `Alt+PgUp`, `Alt+PgDown`, `Ctrl+Alt+Up` and `Ctrl+Alt+Down`
  once each and no `Ctrl+Up`/`Ctrl+Down`. Feedback-label reordering
  (`Alt+Up/Down`) is in the separate Settings window, the Review Cycle share
  sheet's note moves (`Alt+Up/Down`) are in their own dialog, and the tour
  editor's reorder is `Alt+Shift+Up/Down`; none meets these keys.
- *Qt standard keys.* The window test checks every standard key on the
  platform it runs on: only macOS's `MoveToStartOfBlock`/`MoveToEndOfBlock`
  (⌥↑/⌥↓) hold any of the six, and those belong to editors. ⌥←/⌥→ are not
  used, so `QKeySequence::Back`/`Forward` (Alt+Left/Right on Windows and
  Linux) are untouched; the application binds no Back/Forward and has no
  window history navigation.
- *Editors.* An editable multi-line editor (composer, reply, suggestion, note)
  claims ⌥↑/⌥↓ and ⌘↑/⌘↓ in `ShortcutOverride`, as it claims ⌘B, and hands
  the press to the editor alone, so the caret moves exactly as an editor on
  its own moves it, and an ignored press (⌥↑ on the first line) cannot travel
  on to the window or the code pane around it. A read-only view does not
  claim them. macOS marks the arrow keys as keypad keys, which the v001 yield
  compared exactly, so on a real Cocoa build it would not have claimed ⌘↓/⌘↑;
  the yield now looks past the keypad mark, as Qt's shortcut map does.
  Single-line fields are not yielded, as the file keys
  (`Alt+Up/Down`) were not before.
- *Operating systems, from documentation and not observed on a station.*
  `Ctrl+Alt+Up/Down` may never reach the application on some machines:
  Windows with Intel graphics hotkeys enabled (older Intel HD Graphics drivers
  rotate the display on Ctrl+Alt+arrows; the newer Intel Graphics Command
  Center ships with them off), Xfce (workspace up/down), Cinnamon (Expo and
  Scale) and GNOME 3.x (vertical workspace switching; GNOME 40 and later moved
  to horizontal workspaces, and whether Up/Down are still bound there was not
  checked). KDE Plasma uses Meta+Ctrl+Up/Down, and macOS reserves no
  ⌥⌘↑/⌥⌘↓. The file arrows and the Files drawer remain where the keys are
  taken. `Alt+Up/Down` and `Alt+PgUp/PgDown` have no default system binding
  on these platforms.

## Acceptance

- **C1.** The core enables a change command at an end of the open comparison
  exactly when the matching file command has a neighbour, with a crossing that
  names that decision and the entry end; it refuses only at the ends of the
  pull request, with `at_first_change` or `at_last_change`. Tests fail before
  the change.
- **C2.** Through the real composition on `nested-folders-v1`, Next change
  walks from the first change of the first file to the last change of the
  last, crossing each file once; Previous change walks back, entering each
  file at its last change. Tooltips name each destination and each end.
- **C3.** Owner look, frozen build, one Accept/Reject: "change arrows continue
  to the next file".
- **C4.** ⌥↓/⌥↑ (Alt+Down/Alt+Up) navigate changes and ⌥⌘↓/⌥⌘↑
  (Ctrl+Alt+Down/Ctrl+Alt+Up) files from the code; ⌘↓/⌘↑ (Ctrl+End/Ctrl+Home
  on Windows and Linux) scroll the code to its bottom and top; an editable
  multi-line editor keeps ⌥↑/⌥↓ and ⌘↑/⌘↓; owner look in the same session,
  one Accept/Reject. Tests fail before the change.
- **C5.** Every file and change arrow's tooltip names its key in native
  notation ("Next change (⌥↓)", "Next file (⌥⌘↓)"); owner look in the same
  session, one Accept/Reject.

## Execution record

The frozen candidate is `c863dd850`, this change on `6bfde1e6d`. Main moved before the push; on main the same change is the BPR-1328 product commit rebased onto `fe63ed5e3`, with the round-trip pins resealed again over BPR-1212 slice 2's reseal (`a90419791`). Only other cards' files differ between the two.

**C1, unattended, pass.** `core_workbench_interaction`: the new
`test_change_commands_continue_into_the_neighbouring_file` covers the
one-change middle file, both ends of the pull request, the only file, a file
with no changes, a nonresident selection, the wrap policy, the stale crossing
gesture and the republished commands. Failing-before control: a probe of the
owner's case (one change, file 2 of 3) built against the pre-change core
reports both change commands disabled (`at_first_change`, `at_last_change`)
and exits 1; against this core both are enabled and it exits 0. Mutation
controls in `qt_workbench_window`: disabling the window's crossing dispatch
fails "next change at the file's last change leaves as one crossing workbench
intent"; disabling the comparison's entry fails "going back enters the
previous file at its last change"; giving the editor yield no change keys
fails "Command-Down in an editor takes no change gesture".

**C2, unattended, pass.** `application_change_arrows_across_files` (new),
real composition on `nested-folders-v1`: 17 files, 16 crossings forward and
16 back, 4 in-file steps, 12 one-change files between two others, every
destination and end tooltip exact. Offscreen renders confirm Review with both
arrows live on a one-change file.

**C4/C5 unattended evidence.** `qt_workbench_window_scale_{100,125,200}` pass
with `test_change_keys_are_named_and_yield_in_editors` (keys, native tooltip
text, ⌘↓/⌘↑/⌥PgDn from the code pane, editor yield measured against an editor
on its own, read-only negative control, every Qt standard key checked) and the
Hide Drawers duplicate-key table naming all four change keys. The native
cocoa code-pane path is not driven here (offscreen only); the owner look is
its evidence.

Focused set at the rebased source: `architecture_boundaries`,
`test_network_isolation_manifest`, `core_workbench_interaction`,
`core_diff_transition`, `qt_workbench_window_scale_{100,125,200}`,
`application_review_round_trip_composition_contract` (resealed over
BPR-1212 slice 2's reseal; mutation pin unchanged) and
`application_change_arrows_across_files` pass. Before the follow-ups, also
`qt_composed_github_scale_{100,125,200}`, `qt_diff_workbench_scale_{100,125,200}`,
`application_uiforetw_selected_diff_contract`,
`application_workbench_composition_contract`,
`application_github_public_path_nested_folders_v1` and
`application_overview_summary_1` passed. `application_composition_contract` passes 8/8 run directly (559 s); through ctest it twice hit its 600 s bound while three worktrees ran it at once. Format check passes; clang-tidy on
the changed translation units reports only pre-existing naming findings in
`github_application.cpp`, none on changed lines.

**v001 owner look, released unanswered.** Session
`6b391cb7c7f4cc221feb4da5b5672b3c` (candidates `BPR-1328-arrows-v001`,
`BPR-1328-cmd-keys-v001`, `BPR-1328-key-tooltips-v001` on frozen `c863dd850`)
was stopped before any verdict when the owner changed the keys; its
candidates were released (`resume-development --all`, cursor 10) and its
frozen root `artifacts/bpr-1328/change-arrows-frozen-v001/` deleted.

### v002: the owner's final keys

Product commit `3a5707d28` (rebased on main; the rebases moved no `src/` or
`tests/` file of this change).

**C4/C5 unattended evidence, v002.** `qt_workbench_window_scale_{100,125,200}`
pass with the rewritten `test_change_keys_are_named_and_yield_in_editors`:
the six keys and their order; native tooltip text (⌥↓, ⌥↑, ⌥⌘↓, ⌥⌘↑ on
macOS, written out elsewhere) including a disabled arrow; every Qt standard
key on the platform (only macOS's paragraph moves hold ⌥↑/⌥↓); ⌥↓, ⌥↑ and
⌥PgDn from the code are Next, Previous and Next change, ⌥⌘↓ and ⌥⌘↑ Next and
Previous file; ⌘↓/⌘↑ (Ctrl+End/Ctrl+Home off macOS) from the left pane, the
right pane and the one-pane comparison scroll to the exact bottom and top of a
400-line comparison, take no gesture and keep the keyboard, with and without
the keypad mark; in a note being written ⌥↓, ⌥↑, ⌘↓, ⌘↑ and keypad-marked
⌥↓/⌘↑ take no gesture and move the caret exactly as an editor on its own
does, with a read-only negative control; in the Updates line ⌥⌘↓/⌥⌘↑ are
the file keys and ⌥⌘PgDn/⌥⌘PgUp load earlier epochs and return to the newest.
The Hide Drawers duplicate-key table and the window's one-key-one-command rule
pass. `application_change_arrows_across_files` passes with the new keys (one
run under load average 60 timed out opening the first file; the rerun passed
in 4 s).

Failing-before control: the new tests built against the pre-change source
(with only the scroll-edge helper the test names) fail
`qt_workbench_window_scale_100` on "an enabled file action names itself and
its key" and `application_change_arrows_across_files` on "each arrow's
tooltip names its key". Mutation controls, each failing exactly its
assertion: no scroll in the two-pane controller ("two panes, left,
Command-Down and Command-Up: scroll to the bottom and the top"); no scroll in
the one-pane comparison ("one pane, …"); the v001 exact-key yield ("Option-Down
(keypad) in an editor takes no gesture …"); the epoch keys back on
Ctrl+Alt+Down ("no keyboard command is claimed twice in the assembled
workbench").

Focused set at the product source: `architecture_boundaries`,
`qt_epoch_comparison_scale_{100,125,200}`,
`qt_comparison_scroll_controller_scale_{100,125,200}`,
`qt_diff_workbench_scale_{100,125,200}`,
`qt_workbench_window_scale_{100,125,200}`, `qt_feedback_label_settings`,
`qt_hide_drawers_native_menu_key` and `application_change_arrows_across_files`
pass; after the rebase the window, Hide Drawers and change-arrows tests pass
again. The documentation gate passes. clang-format passes on the changed
files. clang-tidy was not run this round: the machine's disk was full (under
1 GB free, other worktrees building), so the dev build tree that holds the
compile database was deleted before freezing. `src/app` is unchanged, so the
round-trip composition pins are untouched.

**Owner look (C3, C4, C5) pending.** Fresh short session
`57eb8f9479ce4db96d41ea195bccbff5` (state
`/Users/jaeger/.mask/bpr-1328-change-keys-20261008-v002/attended-review`),
cursor 12, three ready, protected Accept/Reject candidates on one frozen
build: `BPR-1328-arrows-v002` (C3, `e5e0a129…`), `BPR-1328-keys-v002` (C4,
`237f2a67…`), `BPR-1328-key-tooltips-v002` (C5, `c5713401…`). Frozen root
`artifacts/bpr-1328/change-keys-frozen-v002/` holds the exported source of
`3a5707d28` (archive `ecf2ab32…`, deleted after extraction for disk), its
Release build (binary `99519275…`) and `launch.py` (`5fa75ff5…`, modes
`arrows` and `keys`); its public course passes (17 files, 16 crossings, 12
one-change files between two others) and both attended entries reach ready
offscreen, `arrows` on `docs/api/reference.md` and `keys` on the
inline-drawer pull request's 1557-line `src/ui/conversation_cards_view.cpp`.
