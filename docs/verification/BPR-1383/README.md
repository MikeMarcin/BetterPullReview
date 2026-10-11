# BPR-1383 — Discuss to inline Review

Mandate: owner chat, 2026-10-10; file and work the missing Discuss-to-inline-Review navigation. Strategy: coordinator-implementer, recipe revision 2; engineering owner `/root/discuss_inline`, nondelegating leaf. Mandate revision `owner-20261010-discuss-inline`. The selection is preserved in `selection.json`; the authored single-milestone plan is `docs/rfcs/plans/bpr1383-discuss-inline-review/milestone-plan.json`.

## Candidate and implementation

Candidate: `5484b3fb514cb0f05565e2647aac6b2ca9e725ac`. Filed at `dccc706c6`; claimed at `f5986877f`. Its parent includes the separately owned BPR-1382 commit `c783b232e`; that application's source hash matches the input compiled for this course.

Discuss's existing line button and context menu now use the exact pending thread pick already used by Threads → Review. Review opens the Comments drawer, selects Comments even when Notes was selected, selects the thread's file, and opens its first post at the source line once that comparison is ready. The tooltip and accessible name say “Open in Review at path:line”; the menu says “Open in Review”. The compact filename-and-line button remains keyboard accessible. PR-wide and unavailable code anchors retain their disabled menu action and explanation.

UX work used the frontend-design skill at `/Users/jaeger/Documents/GitHub/AgentSkills/skills/frontend-design/SKILL.md` and its desktop guidance. This repair keeps the existing native controls and core route. No provider, persistence or remote-mutation authority changed.

## Acceptance evidence

- AC1: the real composed Discuss line-button gesture enters Review, opens Comments from collapsed, and selects Comments from Notes.
- AC2: the full application course loads the selected Git/cache comparison and opens the exact thread and first post at its source line. The focused workbench course also holds the exact pick while a different file's comparison is pending, then opens it when its own file arrives.
- AC3: the same-file Discuss context-menu action opens the exact thread immediately. Card tests cover the destination's tooltip/accessibility and the disabled action for PR-wide and missing-current-line anchors.
- AC4: fail-before/pass-after evidence, focused Qt checks, core/composition checks, rendered application inspection, formatting and documentation gates below.

Fail-before: with the new full-composition course against the unchanged Qt implementation at `f5986877f` (plus contemporary unrelated BPR-1382 application input), `BPR_SETUP_ARTIFACT_ROOT=/tmp/bpr-discuss-inline/before ctest --preset dev -R '^application_diff_zoom_public_path$' --output-on-failure` failed at “Discuss Open in Review opens the Comments drawer and its Comments tab”. The malformed archive encountered in a later shared `build/dev` compile came from overlapping build writers; final checks used the isolated build below.

Pass-after commands on macOS, Debug, Qt offscreen:

```sh
cmake --preset dev -B build/bpr1383-discuss-inline
cmake --build build/bpr1383-discuss-inline --target test_diff_zoom_public_path test_better_pull_review_qt_workbench_window_tests test_better_pull_review_qt_conversation_cards_view_tests test_better_pull_review_qt_github_conversation_panel_tests test_better_pull_review_qt_github_tests -j 4
BPR_SETUP_ARTIFACT_ROOT=/tmp/bpr-discuss-inline/after ctest --test-dir build/bpr1383-discuss-inline -R '^(application_diff_zoom_public_path|qt_conversation_cards_view)$' --output-on-failure
ctest --test-dir build/bpr1383-discuss-inline -R '^(qt_workbench_conversation_reveal|qt_workbench_window_scale_(100|125|200)|qt_composed_github_scale_100|qt_github_conversation_panel|core_workbench_interaction|architecture_boundaries|application_workbench_composition_contract)$' --output-on-failure
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/src/workbench_window.cpp src/adapters/qt/src/conversation_cards_view.cpp src/adapters/qt/src/conversation_open_card.cpp tests/adapters_qt/conversation_cards_view_tests.cpp tests/adapters_qt/workbench_window_tests.cpp tests/end_to_end/diff_zoom_public_path_tests.cpp
git diff --check
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

The public course and card checks passed. The integration result is 9/9 passed, including the full workbench course at 100%, 125% and 200% scale. Exact clang-format 22.1.8 passed for the six changed C++ files. The documentation gate printed `DOCUMENTATION_OK`; its 67-test suite passed. The focused workbench entry includes the different-file and same-file Discuss cases so it independently reaches them as well as the full window course.

Agent inspected `/tmp/bpr-discuss-inline/after/discuss-open-in-review.png`: Review active, `conversation_cards_view.cpp` selected, Comments tab active, the chosen thread and first post open beside line 38. This offscreen image is local inspection evidence; it does not claim native appearance, screen-reader or other-platform acceptance. The existing broader RFC native/human criteria remain separate. This bounded card has no outstanding human criterion.

## Continuation and ownership

Final candidate file digests are in `source-hashes.json`. Final plan binding is in `checkpoint.json`; `observation.json` and `checkpoint-result.json` record completion and quiescence. The task-owned build is isolated from `build/dev` and is removed after verification. No review server or protected candidate was created. No live desktop or browser-profile data was accessed. Unrelated `docs/proposals/` is preserved. No task-owned command or writer remains at handoff.

Self-review: the new gesture reuses the shared route instead of duplicating drawer/file state; the application course exercises the actual selected Git/cache comparison and rendered inline anchor; exact identity is checked before and after asynchronous loading; existing unavailable actions remain disabled. Required pending obligations: none. Next action: report the completed card to the owner; no RFC closeout is asserted.
