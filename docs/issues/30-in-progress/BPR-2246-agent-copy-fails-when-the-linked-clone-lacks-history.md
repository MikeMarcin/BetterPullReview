---
schema: "implement-rfc.card.v1"
id: "BPR-2246"
title: "The agent's copy fails when the linked clone lacks the head's history, and the failed run shows a Report"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2246--BPR-2246--04684998c7b5.json","related":["BPR-1751","BPR-1873","BPR-2118","BPR-2166","BPR-2245"],"acceptance":["BPR-2246:shallow-clone","BPR-2246:missing-history","BPR-2246:no-report","BPR-2246:remedy"]}
---

# BPR-2246: The agent's copy fails when the linked clone lacks the head's history, and the failed run shows a Report

## Report

The owner, 2026-11-17, from screenshots of the 0.5.0 prerelease (main at
about 0e01b3e9a), on a large private pull request (no repository detail is
recorded here): "Was this addressed? (32 threads)" from the Author tab's
Agent view stopped at once ("Stopped after 0:00"). The banner: "<agent>
stopped before answering. The agent's copy of this pull request could not be
made. Reopen the pull request, then resend. The message is kept." The log:

    error: Could not read <object>
    fatal: Failed to traverse parents of commit <head>

The failed run still showed a "Report" attachment whose page repeated the
error, and the queue was left "Paused: a run failed".

Owner guidance, 2026-11-17: "We want if possible for things to just work."
Repair what is missing and carry on with the run; show an error only when
repair truly cannot succeed, and then an accurate one.

## Scope

- The agent's copy is made when the linked clone holds the head but not
  all of its history (a shallow clone, or a borrowed object store that has
  gone), with no step asked of the reviewer.
- A run that stopped before answering shows no Report attachment.
- The remedy said when the copy truly cannot be made is true.

## Acceptance

- `BPR-2246:shallow-clone`: a native (retained) workspace borrowing a
  shallow linked clone that holds the head is made; its history ends where
  the clone's does.
- `BPR-2246:missing-history`: a retained workspace borrowing a clone whose
  head's parent cannot be read is still made, at the head and the base.
- `BPR-2246:no-report`: a "Was this addressed?" turn that ended without an
  answer shows its error inline and no Report attachment.
- `BPR-2246:remedy`: the copy-failed words no longer tell the reviewer to
  reopen the pull request.
