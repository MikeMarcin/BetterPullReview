---
schema: "implement-rfc.card.v1"
id: "BPR-1292"
title: "The label-authoring course passes on a card drawn before its own refresh, so the run can end with that refresh's reads unread"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-1083 v004 verification (docs/verification/RFC-BPR-0047-0051.md, 'Unrelated, pre-existing'): 7 of 10 failures on the unchanged base 1d839518c","reservation":"docs/issues/_reservations/BPR-1292--BPR-1292--9d185e949f2c.json","related_cards":["BPR-1083","BPR-1287"],"kind":"bug"}
acceptance: [{"criterion":"BPR-1292:cause","outcome":"pass","revision":"c2d7f485d","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1292-the-label-authoring-course-passes-on-a-card-drawn-before-its-own-refresh.md","detail":"Base 8e6c0a042 (origin/main when the work began): 6 of 10 ctest runs failed with GITHUB_FIXTURE_EVIDENCE:01111110:9:25:6 after LABEL_AUTHORING_OK, status 'Refreshing the pull request.'. A temporary per-request log in the fixture transport (removed) showed three account cycles, open, the application's post-publish refresh and the course's refresh; failing runs stopped the third after ListReviewThreads (25 reads), passing runs read it to the end (31). ListPullRequestTimeline is refused (code 9) in every cycle of every run, so the verdict digit is RecordingConsumed=0, not a replay mismatch."}},{"criterion":"BPR-1292:stable","outcome":"pass","revision":"c2d7f485d","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1292-the-label-authoring-course-passes-on-a-card-drawn-before-its-own-refresh.md","command":"ctest --test-dir build/dev -R '^application_label_authoring_contract$' --repeat until-fail:30","detail":"30 of 30 consecutive passes at the fix before the rebase and again 30 of 30 on the rebased candidate's build (over 68c1fe9b7). application_review_round_trip_composition_contract passes resealed; application_threaded_reply_contract 23 of 24 (one budget expiry in its phase 1, no comparison result, before any refresh: a separate intermittent this change does not touch). macOS arm64 Debug, build/dev."}},{"criterion":"BPR-1292:negative","outcome":"pass","revision":"c2d7f485d","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1292-the-label-authoring-course-passes-on-a-card-drawn-before-its-own-refresh.md","command":"ctest --test-dir build/dev -R '^application_label_authoring_contract$' (10 runs, reverted afterwards)","detail":"With the terminal predicate restored to the card alone: 5 of 10 failed with the original GITHUB_FIXTURE_EVIDENCE:01111110:9:25:6. With the course's refresh->trigger() removed: LABEL_AUTHORING_FAILED, failed=refresh-not-taken status=Pull request ready."}}]
---

# BPR-1292: The label-authoring course passes on a card drawn before its own refresh

## Defect

`application_label_authoring_contract` ([[BPR-1083]]) fails intermittently on
macOS. The course passes (`LABEL_AUTHORING_OK`,
`LABEL_AUTHORING_EVIDENCE:111111111`), and then the run fails on the
fixture's own facts:

```text
PUBLIC_PATH_STATUS:Refreshing the pull request.
GITHUB_FIXTURE_FAILED:conventional-comments-v1
GITHUB_FIXTURE_EVIDENCE:01111110:9:25:6
```

The ledger read this as a recording replay mismatch. It is not one. The
first evidence digit, `RecordingConsumed`, is 0: the account script was not
read to its end. The `9` (`unexpected_operation`) is the timeline read, which
no authenticated contract records and which every cycle of every run refuses,
passing runs included; it is not part of the verdict.

## Cause

Three read cycles run over the account script: the open, the application's
own refresh after Publish (an inline draft's terminal triggers Refresh to take
the server's snapshot), and the course's Refresh. The course's last phase
waited only for the published thread's card. That card was already drawn by
the second cycle, so the phase passed on the next tick, with the third
cycle's reads still in flight, and shutdown cut them off part-way through the
script. Whether the third cycle finished first was a matter of timing. A
logged run showed the failing pattern exactly: 25 account reads, the last
`ListReviewThreads`, with `ListIssueComments`, `ListChecks` and
`SearchPullRequests` never asked for.

## Fix

The course's terminal phase judges the published thread only after its own
refresh has answered: the pull request reads "Pull request ready." again and
Refresh is released. Right after `trigger()` the course also checks that the
core took the refresh (Refresh held, "Refreshing the pull request."), so a
refused refresh fails with `refresh-not-taken` and is not mistaken for an
answered one. The checks are unchanged, there are no retries, and the fixture
transport stays strict.

## Acceptance

- `BPR-1292:cause`: the card names the cause, with the runs that showed it.
- `BPR-1292:stable`: 30 consecutive passes of the row at the fix.
- `BPR-1292:negative`: restoring the old terminal predicate brings back the
  original failure, and a course that never refreshes fails on
  `refresh-not-taken`.

## Evidence

macOS arm64 Debug, `build/dev`, worktree off `origin/main`.

| Run | Result |
|---|---|
| Base, 10 runs | 6 failed, all `01111110:9:25:6`, all after `LABEL_AUTHORING_OK` |
| Fix, 30 runs (`--repeat until-fail:30`) | 30 passed |
| Fix rebased over `68c1fe9b7`, 30 runs | 30 passed |
| Old terminal predicate restored, 10 runs | 5 failed, the same evidence |
| Course refresh not triggered | `failed=refresh-not-taken status=Pull request ready.` |

`application_review_round_trip_composition_contract` pins
`github_application.cpp` and the `src/app` manifest, so both are resealed
(note in the file). The manifest pin was already failing on `origin/main`:
BPR-1291's tests-on comparison-end checks in `main.cpp` landed at `68c1fe9b7`
without a reseal, and this reseal takes them in too. Format (clang-format
23.1.1) is clean; the project tidy on `github_application.cpp` reports only
warnings that were there before, none in the changed lines.

## Left alone

- `application_threaded_reply_contract` failed once in 24 runs, a budget
  expiry in its phase 1 (no comparison result), before any publish or
  refresh. It is a different stall and is not carded here.
- Linux and Windows did not run this row for this card. The change is a
  wait in the same tests-on course on every platform.
