---
schema: "implement-rfc.card.v1"
id: "BPR-0847"
title: "The profile activation public path fails on Linux at the owner's orderly drain"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-epoch-and-startup new defect 2","found_at":"98262aee0ac0897985e97784ebfc74a7c6225a3a","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0847: The profile activation public path fails on Linux at the owner's orderly drain

## Defect

`application_profile_activation_public_path` fails on `bpr-linux` at
"owner orderly drain". It failed twice, including with `src/app/main.cpp`
at `98262aee`, before [[BPR-0843]]'s change.

Reported by `bpr-linux` in `task-20260917-lnx-epoch-and-startup`, not carded
there.

## Scope

- Capture the failing step's state on Linux, and whether macOS passes the
  same course at the same head.
- Fix the cause, whether it is in the course or in the product's owner
  shutdown.

## Acceptance

- The course passes on Linux, 3 runs in a row, and still passes on macOS. The
  cause is named here.
