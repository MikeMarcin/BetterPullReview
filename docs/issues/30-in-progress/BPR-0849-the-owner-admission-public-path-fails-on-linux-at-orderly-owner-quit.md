---
schema: "implement-rfc.card.v1"
id: "BPR-0849"
title: "The owner admission public path fails on Linux at the orderly owner quit"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0847-drain new defect 1","found_at":"03dd9ffd0eeda0866c2648859697be7b564dddbc","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0849: The owner admission public path fails on Linux at the orderly owner quit

## Defect

`application_profile_owner_admission_public_path` fails on `bpr-linux` with
`FAILED: orderly owner quit` in 0.74 s at `03dd9ffd`. It passes in 1.34 s
with a private `0700` `BPR_TEST_SOCKET_ROOT`.

The cause is the one [[BPR-0847]] named. The course puts its data root
straight into shared `/tmp` when `BPR_TEST_SOCKET_ROOT` is unset
(`tests/end_to_end/profile_owner_admission_contract.py:23`). An owner creates
its agent workspace neighbourhood only under a parent this user owns and
nobody else can write, so workspace recovery refuses and the owner's quit
exits 1. The macOS runner sets `BPR_TEST_SOCKET_ROOT` to a private folder.
Windows passes `dir=None`, the user's own temporary folder.

Reported by `bpr-linux` in `task-20260917-lnx-0847-drain`, not carded there.

## Scope

- Give the course the owner-only parent BPR-0847 gave the activation course,
  on POSIX when `BPR_TEST_SOCKET_ROOT` is unset. Keep the Windows branch and
  the orderly quit assertion unchanged.
- Check the other courses that take `BPR_TEST_SOCKET_ROOT` with a shared
  fallback, and name any that need the same change.

## Acceptance

- The course passes on Linux, 3 runs in a row, and still passes on macOS.
