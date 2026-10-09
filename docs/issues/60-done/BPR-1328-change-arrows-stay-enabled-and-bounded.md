---
schema: "implement-rfc.card.v1"
id: "BPR-1328"
title: "Change arrows stay enabled as a bounded previous and next"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"found_by":"owner chat note, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1328--BPR-1328--8bce629ab070.json","related_cards":["BPR-1083","BPR-1202","BPR-1287"],"acceptance":["BPR-1328:C1","BPR-1328:C2","BPR-1328:C3","BPR-1328:C4","BPR-1328:C5"]}
acceptance: [{"criterion":"BPR-1328:C1","outcome":"pass","revision":"dda61041d","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1328-change-arrows-stay-enabled-and-bounded.md","detail":"core_workbench_interaction test_navigation_commands_are_bounded_and_always_enabled: one-change comparison in every file position bounds both change commands (at_first_change/at_last_change), still enabled; file bounds; inactive workbench disables all four with no bound; wrap unbounded. Failing-before control (bounded_command disabling every bound) fails. Frozen as f88d25546. macOS; Linux and Windows station runs owed (commands on the card), not gating."}},{"criterion":"BPR-1328:C2","outcome":"pass","revision":"dda61041d","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1328-change-arrows-stay-enabled-and-bounded.md","detail":"application_change_arrows_across_files on nested-folders-v1 through the real composition: 17 files by Next file, 8 in-file steps, 37 presses at a bound leave file, change and comparison unchanged, arrows enabled, status bar names the bound; 12 one-change files between two others. Passes again after the round-trip reseal 1a2a003e5. macOS; Linux and Windows station runs owed (commands on the card), not gating."}},{"criterion":"BPR-1328:C3","outcome":"pass","revision":"f88d25546","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1328-change-arrows-stay-enabled-and-bounded.md","detail":"BPR-1328-arrows-v003 Accepted 2026-10-08, no note (fingerprint d93b38e5..., frozen build of f88d25546 landed as dda61041d, session b8abe3a59fc6912343d5f3bc64fac01c, decision 5c416092, handed back at 11). v002 Rejected (crossing withdrawn). macOS; Linux and Windows station runs owed (commands on the card), not gating."}},{"criterion":"BPR-1328:C4","outcome":"pass","revision":"3a5707d28","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1328-change-arrows-stay-enabled-and-bounded.md","detail":"BPR-1328-keys-v002 Accepted 2026-10-08 (session 57eb8f9479ce4db96d41ea195bccbff5, fingerprint 237f2a67...); qt_workbench_window_scale_{100,125,200} test_change_keys_are_named_and_yield_in_editors with failing-before and mutation controls. macOS; Linux and Windows station runs owed (commands on the card), not gating."}},{"criterion":"BPR-1328:C5","outcome":"pass","revision":"f88d25546","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1328-change-arrows-stay-enabled-and-bounded.md","detail":"BPR-1328-key-tooltips-v003 Accepted 2026-10-08, no note (fingerprint 34d0741a..., session b8abe3a59fc6912343d5f3bc64fac01c, decision d6342f65, handed back at 11); qt_workbench_window_scale_{100,125,200} test_strip_tooltips_sit_directly_below_their_controls anchors all 23 strip controls, mutation control fails. v002 Rejected (tooltip far from button). macOS; Linux and Windows station runs owed (commands on the card), not gating."}}]
---

# BPR-1328: Change arrows stay enabled as a bounded previous and next

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

## Owner verdicts on v002

Session `57eb8f9479ce4db96d41ea195bccbff5`, handed back at cursor 16:

- `BPR-1328-keys-v002` (C4): **Accept**.
- `BPR-1328-arrows-v002` (C3): **Reject**, verbatim: "No not at all, I want
  the arrow keys to be there and selectable, pressable so the ui doesnt change
  but I want them to just try to move to the next change and realize there are
  no more and effectively do nothing. Just let it be a bounded prev and next
  that you can always use."
- `BPR-1328-key-tooltips-v002` (C5): **Reject**, verbatim: "Whys this tooltip
  so far from the button?" Its screenshot showed the Next change tooltip
  ("Next change (⌥↓)" over "Continue to the next file: …") under the tab
  strip, well below and right of the arrow.

The coordinator's reading, relayed with the verdicts: drop cross-file
crossing; the change arrows are always enabled and pressable, and at the first
or last change of the file they do nothing (no wrap, no other file, no
disabled state), the keys likewise; a subtle status hint is allowed but no
layout change; the file arrows follow the same rule if consistent; and every
title-row and review-bar tooltip sits right next to its control, directly
below it, with BPR-1083's anchored tooltip, keeping the key names.

## Change

The v002 crossing (Next change at a file's last change opening the next file)
is withdrawn: `workbench_change_crossing`, `workbench_change_entry`,
`cross_workbench_change_intent`, the comparison's `enter_next_comparison`
and the composition's file-gesture clause for it are removed.

**Bounded and always enabled (v003).** While a pull request is open the four
navigation commands are enabled. Each projection carries a `Bound`: the reason
a press would do nothing now (`at_first_change`, `at_last_change`,
`at_first_file`, `at_last_file`, `no_open_comparison`, `no_changes`,
`no_actionable_files`), or `none` when a press moves. Only an inactive
workbench disables them (`no_active_workbench`, no bound). The window sends no
gesture while a command is bounded, so a press by arrow or key at the first or
last change of the file, or the first or last file, changes nothing: no wrap,
no other file, no reload. The arrows keep their size and place. The press
shows its reason in the existing status bar for three seconds: "First change in
this file.", "Last change in this file.", "First file in this pull request.",
"Last file in this pull request." The tooltip's second line and the status tip
name the bound the same way. The file arrows follow the same rule as the change
arrows. The fixture-only wrap policy still wraps inside the comparison.

**Tooltips beside their controls (v003).** `anchor_descendant_tooltips`
(added to BPR-1083's `anchored_tooltip`) follows every widget inside a root as
it arrives and shows each one's own `toolTip()` directly below it, left edges
aligned and `anchored_tooltip_gap` (4 px) apart, or above it where below would
leave the screen. Both `workbench_strip`s, the title row and the review bar,
install it, so all of their controls anchor: the file and change arrows, the
private-progress button (✓▾), Compose review, Comment, Comparing, Updates, the
position labels, Home, the drawer toggles, the modality segments, Hide drawers,
Account, Settings and Refresh. A widget with no tooltip of its own keeps its
usual handling. The tooltip texts, key names included, are unchanged.

**Keys and tooltips (final, v002).** Next change is `Alt+Down` then
`Alt+PgDown`, Previous change `Alt+Up` then `Alt+PgUp`; Next file is
`Ctrl+Alt+Down` and Previous file `Ctrl+Alt+Up` (Qt's `Ctrl` is ⌘ and `Alt` is
⌥ on macOS). All four are window commands, live from anywhere in the window.
Each arrow's tooltip is "Name (key)" in `QKeySequence::NativeText` for its
first key: "Next change (⌥↓)", "Previous change (⌥↑)", "Next file (⌥⌘↓)",
"Previous file (⌥⌘↑)" on macOS; "Next change (Alt+Down)", "Previous file
(Ctrl+Alt+Up)" elsewhere. A second line names the bound (v003; v002 named a
crossing's destination); the status tip is that line, or the arrow's purpose. The No file open page's direction reads the
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

- **C1.** While a pull request is open, the core enables the four navigation
  commands everywhere and names a bound exactly where a press cannot move: the
  first or last change of the open comparison, the first or last file, no open
  comparison, no changes. Only an inactive workbench disables them. Tests fail
  before the change. (v002's C1, a crossing into the neighbouring file, is
  withdrawn by the owner's Reject.)
- **C2.** Through the real composition on `nested-folders-v1`, every file is
  opened with Next file and walked to its last change and back; one more press
  at either end, and Next file on the last file, changes nothing (same file,
  same change, same comparison), every arrow stays enabled, and the tooltips
  and status bar name each bound.
- **C3.** Owner look, frozen build, one Accept/Reject: "the arrows stay
  enabled and do nothing at the ends". **Accepted** on v003.
- **C4.** ⌥↓/⌥↑ (Alt+Down/Alt+Up) navigate changes and ⌥⌘↓/⌥⌘↑
  (Ctrl+Alt+Down/Ctrl+Alt+Up) files from the code; ⌘↓/⌘↑ (Ctrl+End/Ctrl+Home
  on Windows and Linux) scroll the code to its bottom and top; an editable
  multi-line editor keeps ⌥↑/⌥↓ and ⌘↑/⌘↓; owner look, one Accept/Reject.
  Tests fail before the change. **Accepted** on v002.
- **C5.** Every file and change arrow's tooltip names its key in native
  notation ("Next change (⌥↓)", "Next file (⌥⌘↓)") and, like every title-row
  and review-bar tooltip, appears directly below its control; tests fail
  before the change; owner look, one Accept/Reject. **Accepted** on v003.

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

### v003: bounded, always-enabled arrows; tooltips under their controls

v002 look recorded above (Accept keys; Reject arrows; Reject key tooltips).
Its candidates were released (`resume-development --all`, cursor 17) and its
frozen root `artifacts/bpr-1328/change-keys-frozen-v002/` deleted.

The frozen candidate is `f88d25546` (on `38a70a792`). Main moved before the push;
on main the same product patch is the BPR-1328 bounded-arrows commit rebased onto `54dfbd463`
(identical by `git range-diff`), followed by a reseal of the round-trip pins over
BPR-1331's `7dfde8915`, which had changed `github_application.cpp` and `main.cpp`
without moving them; the round-trip contract and the change-arrows course pass
again there.

**C1, unattended, pass.** `core_workbench_interaction`: the new
`test_navigation_commands_are_bounded_and_always_enabled` (a one-change
comparison in every file position bounds both change commands with
`at_first_change`/`at_last_change`, still enabled; first and last file bound
the file commands; a press at a file bound decides nothing; an inactive
workbench disables all four with no bound; wrap has no bound), and the command
table now requires an enabled, unbounded command to be exactly an accepted
gesture and a refusal to be explained by the bound. Failing-before control:
making `bounded_command` disable every bound, the pre-card behaviour, fails
"the first change bounds previous change, still enabled".

**C2, unattended, pass.** `application_change_arrows_across_files`, real
composition on `nested-folders-v1`: all 17 files opened with Next file, each
walked to its last change and back (8 in-file steps), 37 presses at a bound
(each file's two ends, Previous change and Previous file at the start, Next
file at the end) leave the same file, change and comparison object, every
arrow enabled, and the status bar naming the bound; 12 one-change files
between two others name both bounds.

**C5 anchoring, unattended, pass.** `qt_workbench_window_scale_{100,125,200}`:
`test_strip_tooltips_sit_directly_below_their_controls` sends a tooltip event
to all 23 visible title-row and review-bar controls with a tooltip and requires
the anchored tooltip with the control's own text at exactly
`anchored_tooltip_geometry` (directly below, left edges aligned, moved left
only to stay on the 800-pixel offscreen screen) and no `QToolTip`.
`test_change_arrows_are_bounded_and_always_enabled` covers the window: arrows,
keys (⌥↓, ⌥↑, ⌥PgDn, ⌥⌘↓) and file arrows at their bounds send nothing,
stay enabled and show the bound in the status bar. Mutation controls: no bound
check in `dispatch_navigation` fails "next change at the file's last change
sends nothing and stays on that change"; no `anchor_descendant_tooltips` in
`workbench_strip` fails "Updates: the anchored tooltip shows the control's own
tooltip".

Focused set at the product source: `core_workbench_interaction`,
`architecture_boundaries`, `qt_workbench_window_scale_{100,125,200}`,
`qt_hide_drawers_native_menu_key`, `qt_feedback_label_threads`,
`application_change_arrows_across_files` and
`application_review_round_trip_composition_contract` (resealed over the
BPR-1338 attended-browser reseal; mutation pin unchanged) pass. Format check
passes on the changed files; clang-tidy on `anchored_tooltip.cpp`,
`workbench_interaction.cpp` and `workbench_strip.cpp` reports nothing.
`test_network_isolation_manifest` was not run to completion: it needs every
test executable built, and no test registration changed. The documentation
gate passes.

**Owner look (C3, C5); accepted 2026-10-08, below.** Fresh short session
`b8abe3a59fc6912343d5f3bc64fac01c` (state
`/Users/jaeger/.mask/bpr-1328-change-keys-20261008-v003/attended-review`),
cursor 8, two ready, protected Accept/Reject candidates on one frozen build:
`BPR-1328-arrows-v003` (C3, `d93b38e5…`) and `BPR-1328-key-tooltips-v003`
(C5, `34d0741a…`). Frozen root `artifacts/bpr-1328/change-keys-frozen-v003/` holds the
exported source of `f88d25546` (archive `3163d2bd…`, deleted after
extraction), its Release build with its own dependency cache (binary
`dcf603e9…`) and `launch.py` (`e74455d1…`, no arguments). Its public course
passes offscreen (17 files, 37 bounds, 8 steps, 12 one-change files between
two others) and the attended entry reaches ready offscreen and natively once
(Cocoa) on `docs/api/reference.md`, closed afterwards.

### Owner look v003 answered (2026-10-08)

The owner handed session `b8abe3a59fc6912343d5f3bc64fac01c` back at event 11,
with no notes:

- `BPR-1328-arrows-v003` (C3, `d93b38e5…`): **Accept** (decision
  `5c416092`, event 9, 03:34:11Z).
- `BPR-1328-key-tooltips-v003` (C5, `34d0741a…`): **Accept** (decision
  `d6342f65`, event 10, 03:34:33Z).

With `BPR-1328-keys-v002` (C4) accepted in session `57eb8f94`, every owner
look is answered, and C1 and C2 stand on their recorded evidence. The card is
Done on macOS.

The host was already stopped. The candidates were released with
`resume-development --all` at event 12, and the frozen root
`artifacts/bpr-1328/change-keys-frozen-v003/` was deleted. The session's state
folder keeps the record.

### Owed station runs (not blocking)

Linux and Windows have not run this change (v001, v002 or v003). At a revision
containing the bounded-arrows commit, a station runs `core_workbench_interaction`,
`qt_workbench_window_scale_{100,125,200}`, `qt_hide_drawers_native_menu_key`
and `application_change_arrows_across_files`. There the keys read
`Alt+Down`/`Alt+Up` and `Ctrl+Alt+Down`/`Ctrl+Alt+Up`, the code pane scrolls on
`Ctrl+End`/`Ctrl+Home`, and the desktop may take `Ctrl+Alt+Up/Down` (see
Conflicts).

## Linux station run (bpr-main-linux, 2026-10-08)

At `e602e2cf9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner:
- **Pass:** `core_workbench_interaction`, `qt_workbench_window_scale_{100,125,200}` and
  `application_change_arrows_across_files`.
- **Not registered on Linux:** `qt_hide_drawers_native_menu_key`, so it was
  not run.
- **Format gate:** `coding-format-check` (clang-format 22.1.8) flags
  `src/adapters/qt/src/anchored_tooltip.cpp` (include order) and
  `tests/end_to_end/change_arrows_across_files_public_path_tests.cpp`. That is
  22-versus-23 drift, reported to the coordinator. The repo pins only a
  minimum version.

Windows is still owed.

## Follow-up: `application_workbench_host` (bpr-main-win, 2026-10-08)

`dda61041d` made the change and file commands always enabled, but
`tests/application/workbench_application_host_tests.cpp` still expected the
old greyed-out arrows, so `application_workbench_host` failed on every
platform ("the core projects the exact observed change neighbours"). The test
now checks BPR-1328's rule: both change commands stay enabled, and in a
separate assertion each one's `Bound` is `none`, `at_first_change` or
`at_last_change` exactly as the observed cursor says. The same file's
last-file check now expects `next_file` enabled with `Bound` `at_last_file`.
Test-only; no product change.

## Windows station run (bpr-main-win, 2026-10-08)

At `2779ac75a` with this follow-up's test change, Windows 11, MSVC 19.50,
`build/windows-dev-tour-cli` (Debug), offscreen, network-isolated runner:
- **Pass:** `application_workbench_host`, `core_workbench_interaction`,
  `qt_workbench_window_scale_{100,125,200}`,
  `application_change_arrows_across_files` and `architecture_boundaries`.
- **Not registered on Windows:** `qt_hide_drawers_native_menu_key`, so it was
  not run.

## Follow-up: `application_workbench_composition_contract` (2026-10-09)

`dda61041d` also broke `application_workbench_composition_contract` on
macOS: `WORKBENCH_PROJECTION_WIRE_FAILED` and the seven clauses after it
(selected diff request, exact comparison, change, drawer width, identity
mismatch, invalidation and failure wires), because the course stops at the
first false clause. A `git bisect` from `54dfbd463` (pass) to `052702a54`
(fail), building only `app_better_pull_review` and running that row, named
`dda61041d` as the first failing commit.

The product is right and the course was stale: the fixture's one file is also
its last, and `exercise_workbench_composition`'s `ProjectionDelivery` clause
still required Next file to be disabled there. It now requires Next file
enabled with the status tip "Last file in this pull request.", which is what
BPR-1328 delivers. The invalidation clause's "both file arrows disabled"
stays: an invalidated review is an inactive workbench. The round-trip
composition pins are resealed over the edit in the same commit. Test-only; no
product change. On macOS `application_workbench_composition_contract` (both
the normal run and the cut-selection-wire counterexample) and
`application_review_round_trip_composition_contract` pass.
