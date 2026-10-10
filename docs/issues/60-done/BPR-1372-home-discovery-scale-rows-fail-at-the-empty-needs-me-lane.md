---
schema: "implement-rfc.card.v1"
id: "BPR-1372"
title: "The Home discovery scale rows fail at the empty Needs me lane because their queue is not empty"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["qt_workbench_home_discovery_scale_* (100, and probably 125 and 200) has failed on main on every platform for a long time.","find the commit that broke it (bisect, building only that target) and the cause, and fix the product or the test, whichever is wrong"]},"reservation":"docs/issues/_reservations/BPR-1372--BPR-1372--7c32862f76fa.json","culprit":{"commit":"52e2883dc99787e4061c77d8fc23ad84da10dcbe","card":"BPR-1310"},"related_cards":["BPR-1310","BPR-0990","BPR-1338","BPR-1335","BPR-1354","BPR-1345","BPR-1365"],"filed_by":"rfc-implementer, at the coordinator's request","acceptance":["BPR-1372:culprit","BPR-1372:cause","BPR-1372:rows"]}
acceptance: [{"criterion":"BPR-1372:culprit","outcome":"pass","revision":"25fb7d2d9","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1372-home-discovery-scale-rows-fail-at-the-empty-needs-me-lane.md#bisect","detail":"git bisect run between 2cbbfc0f0 (good) and 39941d671 (bad), building only test_better_pull_review_qt_workbench_window_tests in build/dev and running qt_workbench_home_discovery_scale_100: first bad commit 52e2883dc (BPR-1310 Manage repositories in place on Settings, like Accounts). Its parent 2cbbfc0f0 passes; every bad step failed at the same assertion. One step, 125fb6219, was skipped because ranlib could not write the archive on a 99%-full disk."}},{"criterion":"BPR-1372:cause","outcome":"pass","revision":"25fb7d2d9","evidence":{"kind":"command","path":"tests/adapters_qt/workbench_window_tests.cpp","detail":"A temporary print at the failing assertion on 39941d671 read onboarding=1 needsPanel=1 sentence=0 cycles=1. The test was wrong: discovery_projection keeps two_account_projection's review request for octo-reviewer, so Needs me held a row. The workbench specification (First repository) says a row enrolled without a saved repository shows in its lane as usual. Only an empty queue loses the lane. home_list_tests already pins that row case."}},{"criterion":"BPR-1372:rows","outcome":"pass","revision":"25fb7d2d9","evidence":{"kind":"command","path":"tests/adapters_qt/CMakeLists.txt","detail":"macOS build/dev Debug at 25fb7d2d9: qt_workbench_home_discovery_scale_100, _125 and _200 pass (3/3). Mutation check: with home_list's `&& !Onboarding` removed, scale_100 fails at the empty-lane assertion again. coding-format-check passes with clang-format 22.1.8. The round-trip composition pins print unchanged values, because only a test file changed. verify_documentation.py prints DOCUMENTATION_OK."}}]
---

# BPR-1372: The Home discovery scale rows fail at the empty Needs me lane

## Observation

`qt_workbench_home_discovery_scale_{100,125,200}` (`--home-discovery`) has
failed on main on every platform at:

> the empty Needs me lane is not shown while nothing is saved; Review Cycles
> stays

[[BPR-1338]] recorded it as a pre-existing failure, and later cards listed it
as known, among them [[BPR-1335]], [[BPR-1354]], [[BPR-1345]] and
[[BPR-1365]].

## Bisect

The scratch worktree built only
`test_better_pull_review_qt_workbench_window_tests` in `build/dev` (macOS
arm64, Debug) and ran `qt_workbench_home_discovery_scale_100` at each step.

- `52e2883dc` fails at the assertion above. It is the commit that added the
  assertion.
- Its parent `2cbbfc0f0` passes.
- `git bisect run` from `2cbbfc0f0` (good) to `39941d671` (bad) named
  `52e2883dc` as the first bad commit. That is
  [[BPR-1310]]'s "Manage repositories in place on Settings, like Accounts", the
  main-line rebase of the owner-accepted candidate `873bd4b0b`. Every bad step
  failed at the same assertion. One step (`125fb6219`) was skipped because
  `ranlib` could not write the archive on a nearly full disk.

The Home commits the request suspected ([[BPR-1342]], [[BPR-1357]],
[[BPR-1364]], [[BPR-1365]]) all came after `52e2883dc` and are not involved.
BPR-1310's evidence lists "qt_workbench_window x3", which are the
`qt_workbench_window_scale_*` rows. The full run stops at
[[BPR-0940]]'s epoch-row assertion and never reaches this course, which runs
only under `--home-discovery`. So the new assertion was never run before it
landed.

## Cause

The test was wrong; the product was right. A temporary print at the
assertion read `onboarding=1 needsPanel=1 sentence=0 cycles=1`. The first
three of the four conditions behaved as intended, and the Needs me panel was
visible because it held a row. `discovery_projection` clears only the saved
repositories. It keeps `two_account_projection`'s items, and one of those is
`octo-org/app#101`, a review request for the session's own account
`octo-reviewer`. The workbench specification's **First repository** rule
says: "While nothing is saved and the queue is empty, Needs me is not shown:
without a saved repository only an observer rule or a follow can enroll a
row, and when either does its lane shows as usual." `home_list::refresh`
does exactly that. It suppresses only the empty lane's sentence while
onboarding, so a lane with rows still shows.
`home_list_tests` already pins "a row enrolled without a saved repository
still shows".

## Fix

`test_home_discovery_glyph_status_and_pull_request_menu` now checks both
halves of the rule:

1. With the nothing-saved projection as published, holding the row, the
   onboarding flag is set and Needs me shows its row.
2. With the same projection's items cleared, Needs me and its sentence are
   hidden and Review Cycles stays. This is the assertion BPR-1310 meant.

No product source changed.

## Verification

- macOS `build/dev` Debug at `25fb7d2d9`:
  `qt_workbench_home_discovery_scale_100`, `_125` and `_200` pass, 3/3.
- Mutation check: removing `&& !Onboarding` from `home_list::refresh` makes
  `scale_100` fail at the empty-lane assertion again, so the fixed test still
  guards the rule.
- `coding-format-check` passes with clang-format 22.1.8.
- The round-trip composition pins
  (`application_review_round_trip_composition_contract.cmake`) print
  unchanged values, because only a test file changed.
- `verify_documentation.py` prints `DOCUMENTATION_OK`.
- Windows and Linux were not run. The fix is a platform-neutral test change,
  and the failure was the same assertion on every platform.
