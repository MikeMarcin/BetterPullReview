---
id: BPR-0344
title: Resolve the queue composition first head differing by filesystem
status: Done
rfc: RFC-BPR-0010
completed: 2026-08-18
depends_on: []
---

# Resolve the queue composition first head differing by filesystem

`application_queue_composition_contract` fails on Linux with:

```
queue seed failed: queue-composition-failed:first-head-is-exact-and-quiet
```

The mechanism is not yet identified. What is established is that the outcome
depends on which filesystem the application data root is on, and on nothing
else that was varied.

## Reproduction

```sh
Q=build/dev/tests/end_to_end/better_pull_review_queue_composition_public_path
QT_QPA_PLATFORM=offscreen QT_STYLE_OVERRIDE=Windows "$Q" seed <root>
```

| root | result |
|---|---|
| `/tmp/...` (tmpfs) | `QUEUE_COMPOSITION_SEED_OK` |
| `/home/mmarcin/...` (ext4) | `first-head-is-exact-and-quiet` |
| the registered build-tree root (ext4) | `first-head-is-exact-and-quiet` |

Unlike BPR-0341 this does not depend on `TMPDIR`. A tmpfs root passes with
`TMPDIR` on ext4, and an ext4 root fails with `TMPDIR` on ext4, so the two
findings have different causes and should not be merged.

The test sandbox is not involved; the failures above are direct runs.

## What was ruled out

Timestamp granularity and small-directory iteration order were compared
between the two filesystems and matched: both report nanosecond `mtime`, and
both returned the same order for a three-entry directory. Neither is therefore
the explanation, though ext4's hashed ordering only diverges on larger
directories and has not been excluded at the sizes this contract creates.

## Required final state

- The mechanism is identified rather than worked around. A queue whose first
  head depends on the filesystem is either reading an order the filesystem is
  not required to provide, or depending on a metadata property that varies.
- The contract passes with its registered build-tree root on a stock Ubuntu
  26.04 host.
- Whatever ordering the queue requires is established by the core and asserted,
  not inherited from directory iteration.

## Result — 2026-08-18

### The card offered two explanations and the answer is neither

The required final state expected "reading an order the filesystem is not
required to provide, or depending on a metadata property that varies". Both
were checked and both are wrong:

- The enrollment query is explicitly ordered —
  `ORDER BY host, account, repository, pull_request`
  (`queue_enrollment_database.cpp:566-570`), and the observer-rule query is
  ordered by `ordinal`. Neither depends on SQLite's physical row order.
- `core::ordered_queue` (`review_queue_attention.cpp:94-113`) sorts the
  projection explicitly by bucket then ref, so arrival order cannot survive
  into what is presented.
- `core::bucket_for` (`review_queue_attention.cpp:49-92`) reads no clock and no
  filesystem at all; its own header says so and says why. There is no timestamp
  in it to have a granularity.
- No `directory_iterator` or `readdir` site feeds queue or epoch ordering.

The real cause is a lost update, and the filesystem only decides which of two
things arrives first.

### What actually happens

The failing conjunct was never identified before because the assertion is a
five-part `wait_until` that reports one string on timeout. Instrumented, it is
unambiguous: `remove_enabled=1 rows=0 has_item=0`. The rule was added; the
queue is simply empty. Extending the wait from 20 seconds to 120 changes
nothing, so the row is lost, not late.

The sequence, from the two hosts side by side:

```
tmpfs                                ext4
accept -> refresh dispatched         accept -> refresh dispatched
completion applied, rules=0          rule add, lastobs=0        <-- reordered
rule add, lastobs=1  -> ENROLLS      completion applied, rules=0
                                     rule settles, lastobs=1    -> nothing
```

Two independent judgements have to agree and neither can see the other:

- A refresh completion is judged against the rules as they stood when it was
  dispatched (`review_queue_enrollment.cpp:707`). Dispatched before the rule
  existed, it matches nothing — correctly, on its own terms.
- A rule mutation adopts enrollments computed when its intent was accepted
  (`:496`), against `LastObservation` as it was then. Accepted before the
  observation arrived, it enrolls nothing — also correctly.

When the two overlap, the pair is never evaluated together. `finish_rule` had
the recovery for exactly one half of this: `if (!LastObservation.has_value())
refresh_last();`. An observation that arrived *during* the mutation is present,
so that guard is false and the else does nothing at all. `reconcile_last()`
already handles both cases and `restore()` already calls it; `finish_rule` now
calls it too.

The tmpfs pass was never the healthy case. It passed because that host's epoch
pipeline failed and retried, producing a *second* `accept` whose refresh landed
after the rule — the enrollment arrived as a side effect of an unrelated
failure. The ext4 run, which materializes the repository and retains a live
epoch, gets one `accept` and loses the enrollment. The host doing more work
correctly was the one reporting the defect.

This is a product defect, not a harness one. A reviewer who adds an observer
rule while the pull request they are looking at is still loading does not get
that pull request in their queue, and no later refresh repairs it.

## Evidence

- `application_queue_composition_contract` passes with its registered
  build-tree root on ext4: ten consecutive runs, ten passes. The complete queue
  matrix passes 8/8, including `sqlite_queue_enrollment_storage`,
  `qt_review_queue`, and the core attention tests.
- The seed course now succeeds on every root that used to divide: an ext4 root
  under `/home`, an ext4 root under `/var/tmp`, and a tmpfs root under `/tmp`.
  Before the repair the two ext4 roots failed 3/3 each and tmpfs passed 3/3.
- A focused test,
  `test_a_rule_added_during_a_refresh_still_enrolls`, reproduces the
  interleaving deterministically through the manual executors, with no
  filesystem, no clock, and no Qt. It drives both requests into flight and
  completes the refresh first. On the prior behavior it fails at the exact step
  that used to do nothing — `committing the rule enrolls the observation it now
  matches` — and it asserts the durable row, not only the projection.
- The instrumentation used to find this was temporary and is not committed. The
  committed evidence is the focused test, which fails without the fix.

### What is still worth doing, and is not done here

The assertion that found this reports one string for five conditions, and that
is why the card could describe the symptom for a week without naming the
failing conjunct. Making that failure name what it saw is a real improvement
and a real change to a shared harness, so it belongs to its own card rather
than to this one.
