---
id: BPR-0607
title: Observe the epoch-preparation diagnostic on macOS
status: Parked
rfc: RFC-BPR-0008
depends_on: [BPR-0606]
parked_on: awaiting-owner-verdict
---

# Observe the epoch-preparation diagnostic on macOS

The [prepared diagnostic checkpoint](../../verification/reviews/epoch-preparation-diagnostic-v001/operator-packet.md)
binds the tests-disabled Release build at `986eb8cd` and one sign-in/PR-read
course as MikeMarcin. It supplies no publication authority. The owner requested
plain continuation instead of repeated digest statements; preserve the actual
instruction when the course is started, and record observed results truthfully.
No new live run or response is claimed by preparation.

## Required final state

- One exact-build run reaches a terminal sign-in and history result for
  MikeMarcin/BPR-Test#1 at the designated head, without publication.
- Record the first typed history failure or successful exact comparison from
  the new diagnostics and safe UI observation; no root-cause guess or raw logs.
- Explicit Sign out, quit and process absence are observed. Any deviation stays
  in the result and is not converted into a clean acceptance claim.
- A reproducible defect becomes a bounded repair before BPR-0256 prepares its
  next publication candidate; failure to observe remains a truthful blocker.

## Preparation evidence

BPR-0606's Dev/Release builds, five focused checks (49.33 seconds), three
integrator checks (5.94 seconds), scoped tidy/format and documentation gate pass.
The executable hash/length were read after build and match the packet. Reuse
these unchanged results; no full matrix, platform expansion or new code is
needed for this checkpoint. Existing ordinary Release output is the pending
consumer's candidate; no separate build tree or worktree was created.
