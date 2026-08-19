---
id: BPR-0344
title: Resolve the queue composition first head differing by filesystem
status: Blocked
rfc: RFC-BPR-0010
depends_on: []
blocked_by: platform-linux
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

## Portable mechanism analysis — 2026-08-18

A read-only trace of the complete seed path establishes what can be known
without the failing host, and it narrows the card considerably.

**The failure string cannot distinguish wrong from late.**
`first-head-is-exact-and-quiet` has exactly one emitter,
`tests/end_to_end/queue_composition_public_path.cpp`, inside a
`wait_until` whose budget is a hardcoded 20,000 ms spin. The predicate ANDs
row count, exact ref, role, matched rule, exact rule, and the quiet bucket; a
timeout reports the unmet expectation's name whether the state is genuinely
wrong or merely not yet true. The driver's outer `execute_process` budget is
25 s, and the recorded ext4 failure printed the full message rather than being
killed, so connect, open, and rule-add all completed within roughly five
seconds on the failing host before the first-head wait burned its entire
window.

**No scan-order dependence exists on the traced path.** The seed-time head
choice is fully explicit: `github_public_path_fixture.cpp` selects
`HeadOneOid` versus `HeadTwoOid` by a ternary on the `EpochPhase` enum and
serves packs by exact OID string equality, with no readdir, glob, first-match,
or map-iteration decision anywhere on the fixture, epoch, or queue path.
Presentation order is core-owned by `core::ordered_queue` over `(Bucket, Ref)`
with an explicit `pull_request_ref` ordering, permutation-proven at the core
tier (`tests/core/review_queue_attention_tests.cpp`), and both SQLite load
paths use explicit `ORDER BY`. The card's original directory-iteration
hypothesis is therefore unsupported for this course.

**The one measured filesystem asymmetry is durability latency.** Every seed
write crosses real barriers: the shared SQLite connection runs
`journal_mode=WAL` with `synchronous=FULL`
(`src/adapters/sqlite/src/sqlite_connection.cpp`), and the installed-git cache
uses write→`fsync`→rename→`fsync(directory)` chains
(`src/adapters/git/src/installed_git_cache.cpp`). These are near-free on tmpfs
and real device flushes on ext4, which fits the observed root-dependence but
is not yet proof.

**The change in this slice makes the next failure identify itself.** The
timeout report now appends the observed terminal state — row count, remove
enablement, and when a row exists its exact ref, role, matched-rule and
rule-exact bits, and bucket name — so an exact-but-late row names a
durability-latency mechanism while a wrong field names a content mechanism.
Verified on macOS: the full `application_queue_composition_contract` passes,
and a deliberate unseeded `reopen` run exercises the failure path and prints
`first-head-is-exact-and-quiet:rows=0:remove-enabled=0` with exit 1. The
20-second budget itself was deliberately not raised: widening it before the
mechanism is read would be the workaround this card forbids.

**Adjacent fragility filed separately.** `queue_enrollment_register::project`
pairs activity observations to records positionally; production's
`activities()` assembles the span correctly from the register's own record
order, so nothing is wrong today, but the contract invites silent
cross-attribution. That is deliberately independent follow-up work tracked as
[[BPR-0365]], not a dependency of this card.

**What remains is Linux-only.** Rerun the sharpened contract on the stock
Ubuntu 26.04 ext4 host and read the state suffix: exact-but-late, wrong
field, or rows=0 each name a different mechanism, and the fix belongs to
whichever appears. This card moves to `50-blocked/` with
`blocked_by: platform-linux` because no further step can execute here.
