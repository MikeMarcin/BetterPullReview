---
schema: "implement-rfc.card.v1"
id: "BPR-1532"
title: "First and last change in the current file on Option-Shift-Up and Option-Shift-Down"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Add Alt+Shift+Up / Alt+Shift+Down (Option-Shift-Up / Option-Shift-Down on macOS) to go to the first / last change in the current file.","governing_rfc":"RFC-BPR-0076","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028"],"related_cards":["BPR-1328","BPR-1417","BPR-1493","BPR-1531"],"reservation":"docs/issues/_reservations/BPR-1532--BPR-1532--fc312ac373f6.json"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"82b3ef02b3ced3b0db22f2880c4c525d00ce3fa6","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_diff_semantics|core_workbench_interaction)$'","detail":"Outermost transition from changes, unchanged lines and no selection; kept at the ends; no_changes keeps the line; projection disables with no workbench and, in Review, with no comparison or no changes; offered outside Review. Negative control: stepping one change fails core_diff_semantics."}},{"criterion":"AC2","outcome":"pass","revision":"82b3ef02b3ced3b0db22f2880c4c525d00ce3fa6","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_change_end_keys|qt_profile_management_window|application_profile_management_public_path|qt_workbench_window_scale_(100|125|200))$'","detail":"Fi&rst change / Last c&hange after Next change in both View menus with Alt+Shift+Up/Down; R and H unique in View; keys from the code dispatch the outermost intent; disabled projection sends nothing."}},{"criterion":"AC3","outcome":"pass","revision":"82b3ef02b3ced3b0db22f2880c4c525d00ce3fa6","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_change_end_keys$'","detail":"Tour outline reorders on Alt+Shift+Up/Down and nothing navigates; a line field, a note and the suggestion editor (read-only too) take no gesture. Negative controls without each yield fail."}},{"criterion":"AC4","outcome":"pending","revision":"82b3ef02b3ced3b0db22f2880c4c525d00ce3fa6","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session eb0e1b14042f51587e16ba8961e1a43c, candidate BPR-1532-change-ends-v001, fingerprint 3468c7c29582f7d97cd64bc7e2fed024a73743bbded5774e258d6481a4249728, cursor 3; frozen root bpr1532-change-ends-v001.ub219uos."}}]
---

# BPR-1532: First and last change in the current file on Option-Shift-Up and Option-Shift-Down

The owner asked on 2026-10-15 for Option-Shift-Up and Option-Shift-Down
(Alt+Shift+Up and Alt+Shift+Down elsewhere) to go to the first and the last
change in the current file. They sit beside Previous change and Next change,
Option-Up and Option-Down ([[BPR-1328]]), and Previous file and Next file,
Option-Command-Up and Option-Command-Down.

Governing design: [RFC-BPR-0076](../../rfcs/RFC-BPR-0076-file-change-navigation-enters-review.md)
(file and change navigation enters Review, [[BPR-1417]]), under the
[workbench allocation](../../architecture/system-architecture.md#rfc-bpr-0005-workbench-allocation):
the core decides the target and whether the command is offered, the host
resolves the exact Review context, and Qt translates the key and presents.
The keys are recorded in the [workbench specification](../../ux/workbench-specification.md).

## Behaviour

- **Unit.** A change is what Option-Up and Option-Down step through: one
  change group of the selected file's exact comparison, in the comparison's
  order. The comparison shows the whole file; it folds no hunks and hides no
  change, so the first and last change are the first and last group in that
  order, in two panes and in One pane alike. Change navigation never crosses
  into another file.
- **Target.** First change selects the first group and Last change the last,
  wherever the selection is: on a change, on an unchanged line, or nowhere.
  The core decides the target through the existing change-navigation
  transition, extended with a reach that names the outermost change.
- **Already there.** On the first change, First change keeps that selection
  and scrolls it into view again, with no bound message: the press is the way
  back to it after scrolling away. Likewise Last change.
- **Offered.** Both commands are disabled where nothing can be reached: no
  active workbench, and in Review a file with no open comparison or with no
  changes. The menu item greys and its tooltip names the reason. From
  Overview, Discuss or Threads the commands are offered and enter Review
  first, as the other four do ([[BPR-1417]]); if the Review context then has
  no changes, the press shows the existing three-second message and keeps
  the position.
- **Menus.** The window's View menu and the macOS desktop bar's View menu
  offer `Fi&rst change` and `Last c&hange` after `Ne&xt change`, with the
  keys. The mnemonics R and H are unused in View. The review bar gains no
  buttons.
- **Keys yield.** In any widget that takes typed text (editable line and
  multi-line editors, editable combo boxes, spin boxes and the Review Cycle
  suggestion editor) Option-Shift-Up and Option-Shift-Down keep their text
  meaning: selection extension on macOS, box carets in the suggestion editor.
  The window keys are unbound there. Where the focused widget or one of its
  ancestors holds its own widget-scoped binding of the same key, as the tour
  editor's stop outline does to reorder stops, the window keys are unbound
  too, so that binding runs and Qt sees no ambiguous shortcut.

## Acceptance

- AC1: Core: the change-navigation transition selects the first or last
  group from a change, an unchanged line and no selection; on the first or
  last change it keeps the selection; with no changes it selects nothing.
  The command projection disables First/Last change with no active
  workbench and, in Review, with no open comparison or no changes, and
  offers them otherwise. A negative control (the transition stepping one
  change instead of reaching the end) fails a test.
- AC2: Qt: the window's View menu and the desktop bar's View menu carry
  `Fi&rst change` and `Last c&hange` with Alt+Shift+Up and Alt+Shift+Down,
  in the same order, no other View mnemonic uses R or H, and the keys
  delivered to the window dispatch the outermost change intent. Disabled
  state follows the projection.
- AC3: Qt: with the tour editor's outline focused, Alt+Shift+Up and
  Alt+Shift+Down reorder the stop and do not navigate; with an editable text
  widget or the suggestion editor focused, the keys do not navigate.
- AC4 (human): The owner accepts the frozen candidate: "⌥⇧↑/⌥⇧↓ jump to
  the first/last change in a file".

## Evidence

Candidate `82b3ef02b3ced3b0db22f2880c4c525d00ce3fa6` on branch
`bpr-1532-change-ends` (worktree
`/Users/jaeger/.codex/worktrees/bpr1532-change-ends/BetterPullReview`), over
main `c2d6bb7cc`. Not merged: product code waits for AC4.

Design. Core: `change_navigation_reach` (`adjacent`, `outermost`) on
`navigate_diff_change_intent`, and `navigate_outermost_change` in
`diff_semantics`, which the existing `transition` uses for the outermost
reach. Two commands, `first_change` and `last_change`, follow the existing
twelve, so no earlier entry moves (`workbench_command_count` 14). Their
projection (`change_end_command`) disables them with no workbench, and in
Review with no open comparison or no changes. Outside Review they are offered,
with the window comparison's bound named. Qt:
`workbench_window` builds `Fi&rst change` and `Last c&hange` after Next
change in View, dispatches through the existing `dispatch_navigation` and
`perform_review_navigation` (RFC-BPR-0076 entry), and unbinds the keys on
focus change. They are unbound in a widget that takes typed text, or where
the focus or an ancestor binds the same key widget-scoped (the tour outline).
`suggestion_editor` claims Alt+Shift+Arrow in `ShortcutOverride`, read-only
too. `diff_workbench::navigate_outermost_change` reveals an end that is
already selected. The desktop bar mirrors the two items, disabled.

Verification at the candidate's sources (macOS arm64, dev preset):

- `ctest --preset dev -R '^(core_diff_semantics|core_workbench_interaction|qt_diff_workbench_scale_100|qt_dif_workshop_placement|qt_change_end_keys|qt_workbench_window_scale_(100|125|200)|qt_profile_management_window|qt_suggestion_editor_1_0|qt_composed_github_scale_100|application_profile_management_public_path|application_review_navigation_entry)$'`:
  all pass.
- Negative controls: `navigate_outermost_change` stepping one change fails
  `core_diff_semantics` ("First change selects the first change and Last
  change the last…"). With no yield at all, `qt_change_end_keys` fails "in an
  editable line field the keys are unbound". Without the outline yield it
  fails "with the tour outline focused, the window's keys step aside". With
  the suggestion editor not claiming its keys, it fails "and its own box
  gesture still claims Option-Shift-Down". In the test, binding the window
  key on the outline as well makes the press reorder nothing (Qt ambiguity).
- The two "no key bound twice" checks in `workbench_window_tests` exempt
  only `tourMoveUp` and `tourMoveDown` on Alt+Shift+Up/Down, which the
  window yields to.
- clang-format 22.1.8 on every touched C++ file: no change. clang-tidy
  (`_coding-standard/scripts/tidy.py --warnings-as-errors`) on every touched
  translation unit: clean, except the baseline naming findings at
  `tests/core/diff_semantics_tests.cpp:826`
  (`test_tour_revision_mapping`, from `22fd21507`), which are not in this
  change.
- `python3 scripts/verify_documentation.py --root <worktree>`:
  `DOCUMENTATION_OK`.
- Frozen build:
  `/Users/jaeger/.codex/attended-builds/bpr1532-change-ends-v001.ub219uos`,
  fixture `uiforetw-74`, file `UIforETW/UIforETWDlg.cpp` (eight changes). Its
  `--check` passed (`ATTENDED_FILE_SHOWN`). Executable SHA-256
  `32e27125a2928c966b7ba63752c12409c575874adf5d87e96e41915933839648`.
- AC4 pending: attended session `eb0e1b14042f51587e16ba8961e1a43c`, candidate
  `BPR-1532-change-ends-v001`, fingerprint
  `3468c7c29582f7d97cd64bc7e2fed024a73743bbded5774e258d6481a4249728`,
  cursor 3. Checkpoint:
  [handoff](../_checkpoints/BPR-1532/handoff.json).

