---
schema: "implement-rfc.card.v1"
id: "BPR-0677"
title: "The comparison stays unavailable after the history is ready"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# The comparison stays unavailable after the history is ready

The owner's 2026-09-07 capture, on the build carrying
[BPR-0667](../40-parked/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md),
shows the pull request loaded, four files listed, participants and
since-my-last-review populated, and this status line:

> This pull request is open. Its comparison is not ready; Refresh to try
> again. The workbench could not be updated. Review history is ready.

So the epoch now resolves, which is what BPR-0667 fixed, and the comparison
still does not reach the workbench. "The workbench could not be updated"
is a separate failure from the one that was repaired, and the reviewer is
told to refresh with no evidence that refreshing helps.

## Scope

- Find out what "the workbench could not be updated" means here, from the
  diagnostics log of that run rather than by guessing: which stage refused,
  and whether it is downstream of the acquire or a separate path.
- Fix it, and if refreshing genuinely cannot help, stop advising it.
- A composed check that a public pull request reaches a comparison, not
  only a review history.
- Owned paths: whatever the diagnosis names.

## Acceptance

- A public pull request opened signed out reaches an exact comparison with
  files that can be selected and read.
- The status line's advice matches what the product can actually do.
