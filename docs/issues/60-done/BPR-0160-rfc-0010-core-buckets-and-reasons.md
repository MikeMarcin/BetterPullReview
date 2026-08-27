---
id: BPR-0160
title: "Core: attention buckets, reasons, and queue navigation"
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Core: attention buckets, reasons, and queue navigation

Seven buckets, derived from GitHub activity and local review state: initial
review requested, changed since my review, new comments or author replies,
fixes awaiting verification, authored reviews awaiting reviewer action,
observed with new activity, and quiet or current.

Each item carries the reason it is where it is. A reviewer looking at an
unfamiliar pull request in their own queue is owed that.

## Required direct evidence

- Every bucket is reached from a bounded observation set, and the boundaries
  between adjacent buckets are exercised rather than assumed.
- The reason travels with the item rather than being recomputed by whoever
  presents it, so two surfaces cannot disagree about why something is there.
- Replaying one observation set twice produces identical buckets and identical
  ordering — not merely equivalent.
- Nothing in this slice reads a clock or schedules a timer. It builds and runs
  under `core-only`.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`core::review_queue_attention` holds `attention_bucket`,
`queue_activity_observation`, `queue_item`, `bucket_for`, and `ordered_queue`,
with a `name` and an `explanation` for every bucket. It builds and its suite
runs under `core-only`.

### The ordering was wrong, and a break found it

The first implementation ordered by bucket with `stable_sort`, on the reasoning
that nothing here can say which of two equally urgent reviews matters more, so
items sharing a bucket should keep the order the host supplied.

Replacing `stable_sort` with `sort` **did not fail the test**. Six items is
small enough that libc++ sorts them with insertion sort, which is stable in
practice, so the assertion passed for a reason that had nothing to do with what
it claimed. That is a vacuous assertion, and it was hiding a real defect: the
queue's order depended on the host's supply order, which nothing had said would
be the same twice. A queue that reshuffles between refreshes is one a reviewer
cannot keep their place in.

`queue_item` now carries a `pull_request_ref` and the order is total: bucket,
then identity. Identity is a tiebreak and not a ranking — it claims nothing
about which review matters more, only that the answer is the same every time.
The rewritten test supplies 64 items in ascending, descending, and rotated
order and requires one queue, then checks every adjacent pair is ordered, so no
two items are left unranked. Removing the tiebreak now fails it.

`pull_request_ref` also replaces the four loose identity fields on
`pull_request_observation` from BPR-0159. One identity type rather than two,
and it is what BPR-0163's model will diff by.

### Decisions worth stating

**The enumeration order is the order of attention.** A reviewer scanning from
the top is scanning by urgency. If the enumeration and the queue disagreed,
every surface reading one would present a different order from the other.

**An unanswered request outranks everything.** It is the only state where
somebody is blocked on this reviewer having done nothing at all. The test drives
it against every other signal set at once.

**Authored comes after the reviewer-facing buckets.** Waiting on somebody else
is not work this reviewer can do, so it must not displace work they can.

**`observed` is only for items no role brought in.** Labelling an assigned
review "observed" would hide that they were asked.

**There is no timestamp in the observation.** "New since I last looked" is a
comparison against what the ledger records this reviewer has seen. Deriving it
from a wall clock would make the answer depend on when the question was asked.

### Evidence

Eight tests. Five breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| authored outranks unread comments | `each bucket is reached by the state it is named for` |
| an assigned item may read as observed | `an item with a GitHub role is not, even when a rule also matched` |
| the request no longer outranks | `each bucket is reached by the state it is named for` |
| bucket only, no identity tiebreak | `the same items in a different supply order give the same queue` |
| two buckets share an explanation | `two buckets never share an explanation` |
