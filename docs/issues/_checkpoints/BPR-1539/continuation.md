# BPR-1539 milestone handoff

Assignment: owner bug report 2026-10-15, dead Previous/Next file and change
arrows; coordinator-dispatched `rfc-implementer`, branch
`bpr-nav-dead-buttons` in its own worktree, base main `33a89d7d7`
(allocation commit). The coordinator integrates the branch.

## Result

Root cause and fix are recorded on the
[card](../../60-done/BPR-1539-navigation-arrows-dead-after-opening-another-pull-request.md).
Product candidate `6ea8f221f` changes `src/app/src/github_application.cpp`
(`TabDestination`, `adopt_tab_destination`, the two refusal call sites),
`src/adapters/qt/src/workbench_window.cpp` and its header
(`refuse_review_navigation`), `tests/end_to_end/review_navigation_entry_public_path_tests.cpp`
(PR-switch course, refusal assertion) and `docs/ux/workbench-specification.md`.
It does not touch `perform_review_navigation` or shortcut bindings, which
BPR-1532 edits.

## Verification

[Recorded evidence](verification.json): navigation entry and document tabs
restoration courses pass at `6ea8f221f`. The profile window restoration
course passed (183 s) on the identical tree. Negative control fails on the
old reset condition. clang-format 22.1.8, clang-tidy 22 on the touched
translation units and the documentation gate pass. No required human,
native or independent acceptance remains; the fix is proven by the
composed course.

## Commands and ownership

All task-owned builds and tests have finished; no server or review session
is running. Build tree `build/dev` in this worktree (debug, about 2 GB) and an
ignored `.agents/skills/implement-rfc` link are task-owned and may be
removed once the coordinator no longer needs them.
