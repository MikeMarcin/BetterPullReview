---
schema: "implement-rfc.card.v1"
id: "BPR-1241"
title: "Compose review and the tour editor's retarget both claim Alt+R"
status: "Parked"
rfcs: []
depends_on: []
metadata: {"origin":"recorded as pre-existing under BPR-1157 (RFC-BPR-0053-0061 ledger); filed by the main stabilization milestone on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1157","BPR-1129","BPR-1135"]}
parked_on: "human-review"
acceptance: [{"criterion":"The six scale rows pass on macOS","outcome":"pass","revision":"e9553b26a","evidence":{"kind":"command","detail":"qt_document_tabs_scale_{100,125,200} and qt_workbench_focus_order_scale_{100,125,200} pass on macOS arm64 offscreen at e9553b26a (first passed at ddecba284)."}},{"criterion":"application_review_tour_publication_public_path still retargets through the action","outcome":"pass","revision":"e9553b26a","evidence":{"kind":"command","detail":"application_review_tour_publication_native and _compat pass at e9553b26a after the app and course were rebuilt."}},{"criterion":"Owner judgment of the proposed retarget key","outcome":"pending","revision":"e9553b26a","evidence":{"kind":"freeform","detail":"v001 (ddecba284) was rejected with the question whether the retarget is Beyond Compare's Copy to Right (session cbd48b91000b00f1e72e1ad62c7c384c, event 865). It is not; attended item BPR-1241-retarget-key-v002 asks again on the frozen e9553b26a build, served on port 59998."}},{"criterion":"Documentation gate","outcome":"pass","revision":"e9553b26a","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1241: Compose review and the tour editor's retarget both claim Alt+R

## Observation

At `main` e9e65eab2 on macOS arm64, `qt_document_tabs_scale_{100,125,200}`
fail at `assembled keys are unique` and
`qt_workbench_focus_order_scale_{100,125,200}` at `no keyboard command is
claimed twice in the assembled workbench`. Two actions in one workbench window
carry `Alt+R`:

- `composeReviewAction`, Compose review (`src/adapters/qt/src/workbench_window.cpp`),
  the owner-accepted Option-R of RFC-BPR-0052 AC-10 ([[BPR-1129]]);
- `tourRetarget`, "Use selected source as replacement" in the tour outline's
  stop menu (`src/adapters/qt/src/review_tour_editor.cpp`), added by 54ba51bf8
  on 2026-09-27.

Qt treats a key two enabled actions in one window match as ambiguous and runs
neither, so with the tour outline focused Option-R neither composes the review
nor retargets the stop. That is why the rows require one claim per key.

## Outcome

Compose review keeps Option-R. The retarget moves to a key nothing else in the
window claims. The key is a UX choice, so the owner accepts or rejects the
proposal.

## Direct acceptance

- The six scale rows pass on macOS.
- `application_review_tour_publication_public_path` still retargets through the
  action.
- Owner judgment: the proposed retarget key (attended item `BPR-1241-retarget-key-v002`; v001 was rejected with a question).
- `python3 scripts/verify_documentation.py --root .` passes.

## Candidate (macOS arm64, 2026-09-29)

`ddecba284` moves `tourRetarget` to `Alt+Shift+U` (Option-Shift-U), which no
other action in the assembled window claims; Compose review keeps Option-R.

| Check | Result |
|---|---|
| `qt_document_tabs_scale_{100,125,200}`, `qt_workbench_focus_order_scale_{100,125,200}` | Pass (they failed at the duplicate key before) |
| `application_review_tour_publication_{native,compat}`, which trigger `tourRetarget` | Pass |
| Tour editor Qt rows | Pass |

Owner judgment is parked as attended item `BPR-1241-retarget-key-v001` in
session `cbd48b91000b00f1e72e1ad62c7c384c` (prepared and protected, no server
started): Accept keeps Option-Shift-U; Reject names another key in the note.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

Evidence only; the card stays parked for the owner. `qt_workbench_focus_order_scale_{100,125,200}` pass, so the Alt+R conflict is gone here too. `qt_document_tabs_scale_{100,125,200}` fail on Linux at a different check: `activation reveals the entire active tab including after restoration`. A temporary trace, reverted and never committed, measured it with one tab at the window's floor: the strip's viewport is 179 px, but `document_tab_strip::minimum_tab_width` is 220, so the active tab (rect −41..178) cannot be fully revealed. The Linux font makes the chrome around the strip wider than on the Mac. Whether the floor, the minimum tab width or the check should give is a layout decision, so nothing was changed.

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## v001 rejected with a question, 2026-10-03

The owner first accepted `BPR-1241-retarget-key-v001` (event 864), then
changed the verdict to Reject (event 865, session
`cbd48b91000b00f1e72e1ad62c7c384c`) with this note: "Is this equivalent to the
beyond compare copy to right command? If so use those hotkeys": Copy to Right
(block or selection) ⌥⌘→ and Copy Line to Right ⌃⌘→.

### Finding: not equivalent

Beyond Compare's Copy to Right writes the left file's text for the current
block, selection or line into the right file. The two sides differ afterwards
in content, and the command has a direction and a Left counterpart.

"Use selected source as replacement" does none of that. It reads the line
range selected in either comparison pane (`selected_tour_source` in
`src/app/src/github_application.cpp` accepts the left or the right side) and
dispatches one `edit_stop` that sets the selected tour stop's `Mapped` range
to it, with `tour_mapping::author_selected` and the observed head. It changes
which code the stop points at. No text moves between panes, no file changes,
and the result does not depend on which side the selection is on, so it has
no direction and no Left counterpart. BPR has no command that copies text
between the panes, so nothing in the window corresponds to Copy to Right or
Copy Line to Right. The two key families are therefore not bound and no line
or Left variant is added. ⌥⌘→, ⌥⌘←, ⌃⌘→ and ⌃⌘← stay free if BPR ever gains
such a command.

The word "replacement" in the label probably invites the reading the owner
asked about. The key is the question here, so the label is unchanged. The
stop menu's tooltip now says what the command does, as below.

### Candidate v002 (macOS arm64, e9553b26a)

The retarget keeps Option-Shift-U, the key the owner first accepted. What
changed since v001:

- The stop menu shows tooltips. The retarget's tooltip reads "Point this stop
  at the lines selected in the comparison (⌥⇧U). No code is copied or
  changed." The key is written in the platform's native text, so on Windows
  and Linux it reads Alt+Shift+U.
- `test_tour_retarget_key_reaches_only_the_outline` in
  `tests/adapters_qt/workbench_window_tests.cpp` (rows `qt_dif_workshop_placement`
  and the full run) drives keys through the window system into the assembled
  workbench. With a stop selected and the outline focused, Option-Shift-U
  retargets that stop once and opens nothing else. Option-R there still opens
  Compose review and does not retarget. With focus away from the outline,
  Option-Shift-U retargets nothing.

Collisions: the assembled-window rows that refuse a key claimed twice pass.
macOS has no default system shortcut on ⌥⇧U. In a text field ⌥⇧U types "¨",
but the binding is a widget shortcut on the outline, which is a list, so the
tour's text fields keep it, as the away-from-the-outline check shows.

| Check at e9553b26a | Result |
|---|---|
| `qt_dif_workshop_placement` (composition keys and the new retarget-key course) | Pass |
| `qt_document_tabs_scale_{100,125,200}`, `qt_workbench_focus_order_scale_{100,125,200}` | Pass |
| `qt_scoped_prose_editor` (tour editor rows) | Pass |
| `application_review_tour_publication_{native,compat}` | Pass |
| clang-format (whole tree) and clang-tidy (both changed files) | Pass |

Negative controls, each applied to the working tree, built and reverted, never
committed:

| Mutation | Failing check |
|---|---|
| Outline actions made `Qt::WindowShortcut` | "Option-Shift-U away from the outline retargets nothing" |
| The retarget's callback emptied | "Option-Shift-U on the outline retargets the selected stop once and opens nothing else" |
| The retarget put back on Alt+R (the original collision), with the test pressing Option-R | the same check fails: Qt runs neither of two actions on one key |

The frozen bundle is `/Users/jaeger/bpr1241-review-v002.fgVUyw/BetterPullReview-bpr1241-v002.app`
(executable sha256 `40c9ee894971647eef22af8129bdcb33bf79bedeee8a24ed84d0ad69beb0d29a`),
copied from `build/dev` at e9553b26a (Debug, tests on), and launched by
`/opt/homebrew/bin/python3 /Users/jaeger/bpr1241-review-v002.fgVUyw/launch.py` on the
`nested-folders-v1` fixture, with HOME and CFFIXED_USER_HOME set to the root's own `home`. An
eight-second offscreen launch in a scratch home stayed up. I did not drive
the stop menu in the frozen app myself; the key is proven by the assembled-window
course above, and the retarget through the composition root by the publication rows.

Owner judgment is attended item `BPR-1241-retarget-key-v002` in the same
session: Accept keeps Option-Shift-U; Reject names another key in the note.
v001 was released by candidate name.
