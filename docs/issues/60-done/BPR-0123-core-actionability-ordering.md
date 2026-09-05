---
schema: "implement-rfc.card.v1"
id: "BPR-0123"
title: "Core actionability policy, deterministic ordering, and explanation"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0122"]
---

# Core actionability policy, deterministic ordering, and explanation

The domain derives an ordered actionable queue from GitHub state, local
receipts, defect state, and the current filters, and the UI must be able to
explain why an item is next.

Determinism is the sharp requirement. Ranking the same inputs twice must give
the same order, including ties: a tie broken by hash-map iteration order or by
insertion order is not deterministic even when it looks stable on one run.
Break every tie on a stated identity.

The explanation is a first-class output, not a formatted string built by the
adapter. The core names the deciding facts; the adapter renders them.

## Required direct evidence

- Ranking a fixture queue twice yields an identical order, and the fixture
  contains at least one tie that only an explicit rule can break.
- The explanation names the facts that decided the rank, and changing one of
  those facts changes both the rank and the explanation.
- Filters change the queue without changing the ranking rule.
- Each assertion is shown to fail when the behaviour it constrains is neutered.

## Outcome

`reason_for` decides why one file is actionable and `rank_actionable` orders a
bounded input set. The core sees only the declared `actionability_input`, so
the ranking cannot depend on a fact it did not declare.

### The reason is the explanation

`actionability_reason` is a value, not a formatted string. An adapter that
composed its own reason text would be deciding policy; here it renders what the
core named. The enumerator order *is* the priority order and the ranking reads
it directly rather than restating it in a comparison function — two statements
of one order drift apart.

The test asserts the explanation tracks the deciding fact in both directions:
adding a defect to a file that was actionable for an unread conversation
changes the reason, and removing it again restores the previous one. A reason
that only ever moved one way would pass a weaker test.

### Determinism needed an explicit tie-break

Ordering is by reason and then by file identity. The fixture deliberately puts
two files at the same top reason and supplies them in an order the identity
tie-break must overturn. Without that, `stable_sort` preserves the caller's
order, which looks stable on one run and is not deterministic. Ranking the same
set supplied in reverse is asserted to give an identical result.

### Exclusion leaves the queue rather than competing in it

`excluded` sits outside the actionable range and is checked before every other
reason, so an excluded file with a defect and three unread conversations is
still excluded. Ordering exclusion among the actionable reasons would have put
it in the queue it exists to leave. The default queue omits it; a filter admits
it without changing why it is what it is.

Filters change membership only. The test ranks a set, then filters it, and
asserts the surviving items keep the order they already had.

### Non-vacuity

Two behaviours were broken separately and each failed its own assertion, then
restored to green:

- The identity tie-break removed — "the tie between two defects is broken on
  file identity".
- The exclusion check removed so exclusion competes — "exclusion outranks every
  actionable reason rather than competing".
