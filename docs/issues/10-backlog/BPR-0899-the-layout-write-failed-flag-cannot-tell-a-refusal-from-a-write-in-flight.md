---
schema: "implement-rfc.card.v1"
id: "BPR-0899"
title: "The durable layout port's write-failed flag cannot tell a refusal from a write still in flight"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-win, while instrumenting BPR-0875's three named cleanup sources","found_at":"c1723a3f","run":"run-20260916-stations","dispatch":"dsp-006"}
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
