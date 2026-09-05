---
schema: "implement-rfc.card.v1"
id: "BPR-0616"
title: "Kill review publication across durable recovery boundaries"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0614","BPR-0615"]
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

## Completion evidence — 2026-09-05

The existing test transport and real SQLite port now expose fixture-only
post-operation checkpoints. The Python parent sends SIGKILL while the child is
still alive with stdin and its SQLite store open, and requires the signal exit
status. Five courses cover prepared admission, durable witness before send,
simulated publication before reconciliation, resolved receipt before close,
and publication followed by a moved head at recovery. A file-backed fixture
server journal survives the child and independently detects additional sends.

Each recovery opens production storage before any independent integrity check,
compares the full persisted record, discovers the original pending request,
reconciles, retries that exact request, then opens another process to verify
the resulting receipt or uncertainty is durable. Both retries issue zero POSTs.
SQLite integrity and foreign-key checks pass after all five courses.

At the implementation committed after planning `c8840b65`:

- Local focused target build passed with strict compiler warnings.
- `ctest --test-dir build/dev --output-on-failure -R
  '^(live_review_submission_provider|live_review_submission_restart_recovery)$'`:
  **2/2 passed, 0.96 seconds**, including all five forced-kill courses and the
  retained orderly restart cases. macOS arm64, network-deny sandbox, zero
  loopback ports; no native Windows/Linux result is claimed.
- Scoped clang-format, code-simplifier inspection, `git diff --check`, root
  documentation validation and all **62 documentation tests** passed.

No production source or behavior changed; the persisted-state path is the
selected direct surface. No application launch, credential operation, live
request, GitHub Actions, push or paid CI occurred. Every child was reaped;
contained temporary fixture roots were removed automatically. No separate
build root or retained evidence package was created. Full matrices and tidy
were unnecessary for this two-file fixture change. Remaining RFC-0013 kill,
performance and fuzz coverage stays open in BPR-0617.
