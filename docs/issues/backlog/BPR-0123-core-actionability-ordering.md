---
id: BPR-0123
title: Core actionability policy, deterministic ordering, and explanation
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0122]
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
