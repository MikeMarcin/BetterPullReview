---
schema: "implement-rfc.card.v1"
id: "BPR-0899"
title: "The durable layout port's write-failed flag cannot tell a refusal from a write still in flight"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-win, while instrumenting BPR-0875's three named cleanup sources","found_at":"c1723a3f","run":"run-20260916-stations","dispatch":"dsp-006","milestone_plan":{"id":"bpr0899-layout-write","path":"docs/rfcs/plans/bpr0899-layout-write/milestone-plan.json"}}
---

# BPR-0899: The durable layout port's write-failed flag cannot tell a refusal from a write still in flight

## Defect

`profile_window_layout_port::store` sets `WriteFailed` **before** performing the
write and clears it from the result. So `write_failed()` reads true for two
quite different states: a store that actually refused, and a store that is
simply still running.

The shutdown drain reads that flag. A quit that arrives while a layout write is
in flight therefore looks exactly like a quit after a layout write refused —
and only the first of those loses the reviewer's window layout.

Found by `bpr-win` while instrumenting [[BPR-0875]], which is a good
illustration of the cost: the drain could report a cleanup failure without
anyone being able to say whether anything was lost.

## What has already been done, and what has not

[[BPR-0875]]'s naming work **separates the two in the diagnostic** without
changing any behaviour. The port gained `writes_attempted()`,
`writes_completed()` and `last_write_failure()`, and the shutdown term now
reads `durable-layout-write:window=N:attempted=A:completed=C:<reason>`, where
the reason is `in-flight-at-drain` when no failure was ever recorded, and
`cancelled` or `unavailable` when one was.

That is enough to read a log correctly. It is **not** enough to make the
product behave correctly, because `write_failed()` itself still conflates the
two and the drain still acts on it.

## Scope

- Give the flag one meaning. The obvious shape is to set it from the store's
  result rather than before it, with an explicit in-flight count for anyone who
  needs to know a write is outstanding — which the new counters already
  provide.
- Then decide what the drain should do with a write still in flight. Waiting
  briefly, and reporting only a real refusal, is the answer that preserves the
  layout; refusing immediately is the answer that never blocks a quit. Neither
  is obviously right and the card should record which was chosen and why.
- Check whether any other caller reads `write_failed()` and inherits the same
  ambiguity.

## Acceptance

- `write_failed()` means one thing, and a contract row distinguishes a refusal
  from a write in flight.
- The shutdown drain's behaviour on an in-flight write is chosen deliberately
  and recorded.
- [[BPR-0875]]'s `in-flight-at-drain` term still reports truthfully, or is
  retired because the distinction moved into the flag itself.

## Implemented decision — assignment r1, 2026-09-25

The isolated assignment starts at `9a0b44d56ab645de10afffd7e69a4ca0a3fe8891`
and owns only this standalone card. No earlier BPR-0899 plan or handoff was
present; the adopted `bpr0899-layout-write` plan preserves all three criteria.

The card's original diagnosis predates the current lifecycle. At this baseline,
`begin_other_shutdown()` already calls `shutdown_orderly()` for Quit, and
`poll_shutdown()` already waits for the workbench host's exact request terminals
and pending layout to drain before reading the port. That is the only product
reader of `write_failed()`. Thus the port flag was ambiguous, but the current
ordinary drain cannot reach it while that host's store is still running.
No observed layout loss at this baseline is claimed.

Retain that established policy: close ingress, finish the active write and at
most one coalesced latest layout, then inspect the terminal result. A refusal
is terminal and is not retried. SQLite keeps its existing two-second busy
handler. This bounds queued work and database lock waiting, not arbitrary OS
I/O or the entire multi-host shutdown. A new timer would weaken the existing
promise to preserve the last accepted layout; no owner decision is needed to
retain that already implemented contract. Explicit Close and forced host
cancellation keep their existing lifecycle behavior.

The private composition header now owns the same concrete service and port so
a contract can compose that exact port with the real host and SQLite. The port
stores only the latest completed result, maps an exception to `unavailable`
just as the host already does, clears an earlier refusal on success, and counts
in-flight writes independently. `writes_completed()` still counts successes;
its difference from attempts includes refused stores and was never a reliable
in-flight measure. The drain retires `in-flight-at-drain`, which is unreachable
after quiescence, and keeps BPR-0875's named refusal and counters.

### Status contract

| Observation | `write_failed()` | `writes_in_flight()` | Last failure |
|---|---|---|---|
| Before the first store | false | 0 | absent |
| First store held inside the service | false | 1 | absent |
| Store returned cancelled/unavailable | true | 0 | that refusal |
| A retry runs after a refusal | true | 1 | previous completed refusal |
| A later store succeeded | false | 0 | absent |

The host drains both the effect and its serialized terminal before cleanup.
Tests also hold a completed effect's decision terminal and confirm the host
remains non-quiescent until it processes that terminal and the latest layout.
Exception normalization is covered by code review of the no-throw bookkeeping
around the existing host failure mapping; the test does not inject allocation
or mutex failures into the concrete SQLite service.

## Review and verification

Self-review covers the full pinned application change from baseline `9a0b44d5`
through this card's implementation: extraction of the unchanged service, the
port's atomic result/count bookkeeping and exception mapping, and removal of
the unreachable diagnostic alternative. The new private header is added to
both the production digest manifest and exact app-source inventory. There are
no intervening application edits in this branch, and the production mutation
branch digest stays `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
This records an implementer review, not independent or native acceptance.

The new real-storage contract first failed because its data root did not yet
exist; it now creates that root through the existing private-root helper.
A deliberate negative control restores the old ambiguous predicate by treating
an outstanding write as a failure. The contract fails specifically at
"Quit waits for an actual in-flight write without labelling it a refusal";
the mutation is removed before final verification.

Final commands, candidate identity, results and command/writer state are in
[the BPR-0899 checkpoint](../../verification/bpr0899-layout-write.md).
