---
id: BPR-0616
title: Kill review publication across durable recovery boundaries
status: In Progress
rfc: RFC-BPR-0013
depends_on: [BPR-0614, BPR-0615]
---

# Kill review publication across durable recovery boundaries

The existing whole-review restart fixture exits normally. RFC-BPR-0013 also
requires abrupt process termination: destructors and SQLite close/checkpoint
must not be prerequisites for retaining an admitted publication attempt.

## Scope and acceptance

Extend the existing production review-submission session / real SQLite fixture
with deterministic test-only checkpoints after admission, after the durable
transmission witness, after simulated server acceptance, and after durable
receipt resolution. The parent must forcibly kill a still-running child at
each checkpoint, without allowing stack unwinding, then launch a fresh child.

- Retain the exact original request, authority, retry key, baseline and phase.
- Prepared or unobserved attempts remain uncertain, never falsely successful.
- A witnessed attempt with one new exact server review recovers its original
  receipt, including after head movement; a resolved attempt stays resolved.
- Reconciliation and an explicit retry of the original request issue zero
  additional POSTs. Persisted fixture server state independently counts sends.
- Bound child startup/wait/kill/reaping and clean disposable state on failure.
- Run through the network-isolated local CTest lane with no live GitHub,
  credentials, app sign-in, workflow dispatch, push or paid CI.

This is one bounded part of RFC-0013 recovery. It does not claim power-loss
atomicity, interruption inside SQLite internals, Git-fetch or AI-session kill
coverage, GUI recovery, or completion of the RFC's remaining criteria.

## Ownership and verification

Owned paths: `tests/github_contract/live_review_submission_provider_tests.cpp`,
`tests/scripts/review_submission_restart_recovery_tests.py`, their existing
CTest registration if needed, and this card / BPR-0617. Reuse production ports;
no new production pause switches or second recovery implementation.

Build `test_better_pull_review_live_review_submission_provider_tests` locally;
run `live_review_submission_provider` and
`live_review_submission_restart_recovery`. The existing provider/SQLite path
is the direct persisted-state surface. Check scoped formatting, documentation
validation and `git diff --check`. Inspect changed code for simplification.
