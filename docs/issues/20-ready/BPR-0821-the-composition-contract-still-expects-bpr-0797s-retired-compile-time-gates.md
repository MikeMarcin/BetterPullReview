---
schema: "implement-rfc.card.v1"
id: "BPR-0821"
title: "The composition contract still expects BPR-0797's retired compile-time gates"
status: "Ready"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0008"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-002 BPR-0819 reseal run","found_at":"af612a050f9f8a8a74b8307d1f237b8dc7e9b04f","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0821: The composition contract still expects BPR-0797's retired compile-time gates

## Defect

With [[BPR-0819]]'s reseal applied, `application_composition_contract` runs
6 tests and fails 2 on macOS (`build/r2-logs/dsp2-0819-reseal-ctest.log`):

1. `test_concrete_github_selection_is_owned_by_composition`
   (`tests/end_to_end/composition_contract_tests.py:563`) requires
   `#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)` in
   `src/app/src/github_application.cpp`. [[BPR-0797]]'s `a78eefcf` replaced
   both occurrences with `#if defined(__APPLE__)` plus the run-time
   automated-test boundary. The CLI consent gates now ask whether this run is
   a suite, not whether the build has tests.
2. `test_tests_off_release_graph_omits_public_path_fixture` (`:840`) requires
   that `BPR_BUILD_TESTING` appear nowhere in
   `src/app/include/bpr/app/github_application.hpp`. [[BPR-0797]]'s
   `b1f89533` added a documentation comment that names the define
   ("it used to ask `BPR_BUILD_TESTING`"). The header has no preprocessor use
   of it; the match is prose.

Both are red because the contract encodes the structure BPR-0797 retired, not
the property it protects. The earlier whole-file digest failure masked them.
They were probably red from `b1f89533` on.

## Scope

- Re-express the first assertion as the property it protects. Only the
  macOS graph selects the GitHub CLI process runner, and a run inside the
  automated test boundary cannot reach it. Match the run-time question
  BPR-0797 introduced; do not re-add the define.
- For the second, keep the rule that the public header's layout cannot depend
  on the tests-enabled define. Check preprocessor use, not prose, or reword
  the comment. Whichever you choose, say why, and keep a negative control: a
  header with `#if defined(BPR_BUILD_TESTING)` must still fail.
- Read [[BPR-0797]]'s critical-review section first, so the new assertions do
  not re-open a gap it closed.

## Acceptance

- `application_composition_contract` passes on macOS.
- A hostile header with a real `BPR_BUILD_TESTING` directive fails the second
  test, and a composition source whose CLI runner is reachable in a suite run
  fails the first, both shown with a temporary negative control.
- `application_review_round_trip_composition_contract` still passes.
