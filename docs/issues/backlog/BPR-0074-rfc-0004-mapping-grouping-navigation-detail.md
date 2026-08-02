---
id: BPR-0074
title: Implement mapping grouping navigation and detail semantics
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0073]
---

# Implement mapping grouping navigation and detail semantics

Derive exact-line, similar-line, opposing-span, opposing-boundary,
move-relation, and unavailable results from the one alignment model. Build
stable deterministic change groups, previous/next ordering, selected semantic
line/change state, and lower line-over-line detail inputs from those same IDs.

Do not create a second detail, ribbon, grouping, or navigation mapping and do
not guess a neighboring exact line for one-sided or weakly paired content.

## Required direct evidence

Exercise every mapping kind, group boundary, navigation end/wrap policy,
selection preservation/invalidation, one-sided empty detail, unpaired
replacement spans, move relations, and identity agreement among group,
navigation, link-map input, and detail projection.
