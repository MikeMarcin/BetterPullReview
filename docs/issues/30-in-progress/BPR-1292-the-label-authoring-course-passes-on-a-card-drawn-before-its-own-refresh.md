---
schema: "implement-rfc.card.v1"
id: "BPR-1292"
title: "The label-authoring course passes on a card drawn before its own refresh, so the run can end with that refresh's reads unread"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-1083 v004 verification (docs/verification/RFC-BPR-0047-0051.md, 'Unrelated, pre-existing'): 7 of 10 failures on the unchanged base 1d839518c","reservation":"docs/issues/_reservations/BPR-1292--BPR-1292--9d185e949f2c.json","related_cards":["BPR-1083","BPR-1287"],"kind":"bug"}
acceptance: []
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
