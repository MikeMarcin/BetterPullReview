---
schema: "implement-rfc.card.v1"
id: "BPR-0821"
title: "The composition contract still expects BPR-0797's retired compile-time gates"
status: "Done"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0008"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-002 BPR-0819 reseal run","found_at":"af612a050f9f8a8a74b8307d1f237b8dc7e9b04f","run":"run-20260916-stations","dispatch":"dsp-002"}
acceptance: [{"criterion":"application_composition_contract passes on macOS","outcome":"pass","revision":"9f074d481ec9025c4cbe8c9c1fafb40280e525ed plus the test change committed by this finish","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -R application_composition_contract: Passed 160.29 s (6 tests), build/r2-logs/dsp2-0821-ctest.log; before the change 2 of 6 failed (build/r2-logs/dsp2-0819-reseal-ctest.log)."}},{"criterion":"A hostile header with a real BPR_BUILD_TESTING directive fails the second test, and a composition source whose CLI runner is reachable in a suite run fails the first, both shown with a negative control","outcome":"pass","revision":"9f074d481ec9025c4cbe8c9c1fafb40280e525ed plus this finish","evidence":{"kind":"command","detail":"Controls are permanent subtests in the same passing run: the header check detects an appended #if defined(BPR_BUILD_TESTING) and a string literal naming it; cli_consent_gate_findings returns findings for the offer left open inside the boundary (return true), the dropped boundary answer, the restored compile-time gate, and the removed read warning."}},{"criterion":"application_review_round_trip_composition_contract still passes","outcome":"pass","revision":"9f074d481ec9025c4cbe8c9c1fafb40280e525ed plus this finish","evidence":{"kind":"command","detail":"Passed 11.62 s in build/r2-logs/dsp2-0821-ctest.log."}}]
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

## Resolution (dsp-002)

Read against [[BPR-0797]]'s critical review, which found that a
tests-enabled build could compose the real CLI transport with its
confirmation compiled out. The new assertions protect the property directly
rather than a spelling of it.

- **Selection.** `cli_consent_gate_findings` in
  `tests/end_to_end/composition_contract_tests.py` requires three things.
  `cli_read_offered` answers `!inside-boundary` in the tests-enabled macOS
  graph, `true` in the shipped macOS graph and `false` elsewhere.
  `cli_read_offered_here` passes the run's boundary answer (`false` when
  shipped). Each consent, "Confirm GitHub CLI publication" and "Use
  configured GitHub CLI", appears once, under `#if defined(__APPLE__)` and
  inside `if (Profile->cli_read_offered_here())`. Four hostile sources are
  kept as permanent subtests, and each yields findings:
  - the pre-BPR-0797 compile-time gate brought back;
  - the offer left open inside the boundary;
  - the boundary answer dropped;
  - the read warning removed.
- **Header.** The rule still forbids `BPR_BUILD_TESTING` in
  `github_application.hpp`, but reads the header with comments blanked, so
  BPR-0797's documentation comment is not a use. String and character
  literals survive the stripping, and so do C++14 digit separators.
  Permanent controls: a real `#if defined(BPR_BUILD_TESTING)` directive and
  a string literal naming the define are both still detected.

`application_composition_contract` passes 6/6 on macOS in 160.29 s. It is
slower than the failing run because the tests-off release-graph test now
reaches its configure step. `application_review_round_trip_composition_contract`
still passes. Log: `build/r2-logs/dsp2-0821-ctest.log`.
