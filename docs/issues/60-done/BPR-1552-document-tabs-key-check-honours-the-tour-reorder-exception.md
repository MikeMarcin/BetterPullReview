---
schema: "implement-rfc.card.v1"
id: "BPR-1552"
title: "The document-tabs key check honours the tour outline's reorder exception"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"regression on main 2026-10-16","regressed_by":"BPR-1532","governing_rfc":"RFC-BPR-0076","related_cards":["BPR-1532","BPR-1132"],"reservation":"docs/issues/_reservations/BPR-1552--BPR-1552--f28f517dbb38.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"06cd859d3","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_document_tabs_scale_(100|125|200)$'","detail":"Fail on main 0917321fd ('assembled keys are unique'), pass with yields_to_tour_reorder in all three duplicate-key checks. Negative controls in the document-tabs course: an action named tourMoveUp, widget-scoped, on Ctrl+W fails 'assembled keys are unique: Ctrl+W'; a non-tour widget-scoped action on Alt+Shift+Up fails 'assembled keys are unique: Alt+Shift+Up'."}},{"criterion":"AC2","outcome":"pass","revision":"06cd859d3","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_change_end_keys|qt_workbench_window_scale_(100|125|200)|qt_profile_management_window)$'","detail":"5/5 pass; no shortcut changed."}},{"criterion":"AC3","outcome":"pass","revision":"f5d7743b2","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_label_publication_recovery$'","detail":"Regression from BPR-1538 (5a09d4f61): fails deterministically on main 0917321fd at 'read-only reconciliation confirms the original review' (3/3 runs); passes with the fixture naming original_commit_id/original_line. Control: unfixed fixture plus the pre-BPR-1538 head choice in review_submission_session.cpp passes."}},{"criterion":"AC4","outcome":"pass","revision":"f5d7743b2","evidence":{"kind":"command","command":"ctest --preset dev -j8","detail":"736/739 pass, 1 skipped by design (application_large_review_cache_bytes, Debug), 3 failed only for the worktree's missing MASK .agents/skills (documentation_structure, documentation_issue_graph_contract, issue_id_allocation_contract) and pass with it linked; 9/9 composition contracts, no reseal; clang-format 22.1.8 clean; tidy clean on changed lines; DOCUMENTATION_OK."}}]
---

# BPR-1552: The document-tabs key check honours the tour outline's reorder exception

`qt_document_tabs_scale_{100,125,200}` fail on `main` since [[BPR-1532]]
landed: "assembled keys are unique" finds Alt+Shift+Up bound twice, on the
tour outline's `tourMoveUp` and the window's First change.

[[BPR-1532]] shares the pair on purpose. The tour editor's stop outline
reorders on Alt+Shift+Up and Alt+Shift+Down, widget-scoped, and the window
unbinds First and Last change while focus is in the outline
(`qt_change_end_keys`), so Qt never sees an ambiguous shortcut. That card
narrowed the two "no key is bound twice" checks in the window's scale rows to
skip exactly that pair, but the document-tabs course
([[BPR-1132]]) carries its own copy of the check, which was not narrowed.

The same triage covers `application_label_publication_recovery`, reported
failing on `main` at `32e9bcc0b`: whether it is a regression from the
2026-10-15/16 merges, an older failure or environmental.

## Acceptance

- AC1: `qt_document_tabs_scale_{100,125,200}` pass. Every duplicate-key
  check over the assembled window applies the one documented exception, in
  one place: only `tourMoveUp` and `tourMoveDown`, widget-scoped, on
  Alt+Shift+Up and Alt+Shift+Down. Any other key bound twice still fails, and
  a negative control shows it.
- AC2: `qt_change_end_keys`, `qt_workbench_window_scale_{100,125,200}` and
  `qt_profile_management_window` still pass; the shortcuts themselves are
  unchanged.
- AC3: `application_label_publication_recovery` is classified with evidence,
  and fixed if it is a regression from the day's merges.
- AC4: format, tidy and the documentation gate pass; one full `dev` ctest run
  on the result is recorded with any other failures on `main` named.

## Root cause

**Document tabs.** [[BPR-1532]] (`9b52669b8`, landed `90ff93513` on
2026-10-15) gave First and Last change Alt+Shift+Up and Alt+Shift+Down, the
keys the tour outline already bound widget-scoped as `tourMoveUp` and
`tourMoveDown`. Its `82b3ef02b` narrowed the "no keyboard command is claimed
twice" check in `test_workbench_window_*` and the "no key is bound twice" check
in the zoom course to skip that pair. The document-tabs course
(`test_document_tabs_present_exact_views_and_translate_gestures`, [[BPR-1132]])
has a third copy of the check, "assembled keys are unique", which was left
unnarrowed, and BPR-1532's verification did not run
`qt_document_tabs_scale_*`.

**Label publication recovery.** A regression from [[BPR-1538]]
(`5a09d4f61`, 2026-10-15), not environmental and not older. BPR-1538 holds a
review's line comments to the head the review was written on: a listed
comment matches through `original_commit_id` and `original_line`, and, where
the server names no original line, through `commit_id` and `line` at the
request's head (it had been the session's current head). The test-build
review server (`src/app/src/review_submission_fixture.cpp`) named only
`commit_id`, at its provider's bound head. The `uncertain` branch of the
BPR-1520 navigation-restart course loses the review's answer, restarts with
the head advanced, then presses Check status: the comment is listed at the
new head with no original position, so it never matches and the course fails
at "read-only reconciliation confirms the original review". BPR-1538 ran the
GitHub contract rows only, not this application course. Control: on the
unfixed fixture, putting back the pre-BPR-1538 head choice in
`review_submission_session.cpp` makes the course pass.

## Fix

- `tests/adapters_qt/workbench_window_tests.cpp`: one helper,
  `yields_to_tour_reorder(action, key)`, states the documented exception
  (`tourMoveUp`/`tourMoveDown`, `Qt::WidgetWithChildrenShortcut`, on
  Alt+Shift+Up/Down only); all three duplicate-key checks use it. The second
  check had skipped the two tour actions for any key; it is now held to the
  same pair. No shortcut changes.
- `src/app/src/review_submission_fixture.cpp` (test build only): the pull
  request's comment collection names `original_commit_id` (the head the
  comment's review was submitted at), `original_line` and
  `original_start_line`, as GitHub does after a push. The production match is
  unchanged. The composition contracts pass without a reseal.

## Evidence

Branch `bpr-1552-document-tabs-keys` (worktree
`/Users/jaeger/Documents/GitHub/BetterPullReview/.claude/worktrees/bpr-1552-document-tabs-keys`)
from `main` at `0917321fd`: test fix `06cd859d3`, fixture fix `f5d7743b2`.
Changed: `tests/adapters_qt/workbench_window_tests.cpp`,
`src/app/src/review_submission_fixture.cpp`. macOS arm64, `dev` preset.

| Acceptance | Evidence | Result |
|---|---|---|
| AC1 | `qt_document_tabs_scale_{100,125,200}` fail on `0917321fd` and pass with the helper. Negative controls in the course: a widget-scoped `tourMoveUp` on Ctrl+W fails "assembled keys are unique: Ctrl+W"; a widget-scoped non-tour action on Alt+Shift+Up fails "assembled keys are unique: Alt+Shift+Up". | pass |
| AC2 | `qt_change_end_keys`, `qt_workbench_window_scale_{100,125,200}`, `qt_profile_management_window`: 5/5. | pass |
| AC3 | Fails on `0917321fd` in three runs out of three; passes with the fixture fix. With the unfixed fixture, the pre-BPR-1538 head in `review_submission_session.cpp` passes, which shows BPR-1538 as the trigger. | pass |
| AC4 | Full `ctest --preset dev -j8` (2,859 s): 736 of 739 pass, `application_large_review_cache_bytes` skips in Debug by design, and `documentation_structure`, `documentation_issue_graph_contract` and `issue_id_allocation_contract` fail only because the worktree has no MASK `.agents/skills`; with it linked they pass 3/3. The nine composition contracts pass 9/9 without a reseal. clang-format 22.1.8 is clean on both files. `tidy.py --warnings-as-errors` is clean on the window tests; on the fixture it reports only the file's existing findings (lines 71–576, none in changed lines). `DOCUMENTATION_OK`. | pass |

No other failures on `main` beyond the four this card fixes. No human acceptance
is needed because no shortcut or visible surface changes.
Checkpoint: [handoff](../_checkpoints/BPR-1552/handoff.json).
