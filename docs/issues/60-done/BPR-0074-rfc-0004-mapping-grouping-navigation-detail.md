---
schema: "implement-rfc.card.v1"
id: "BPR-0074"
title: "Implement mapping grouping navigation and detail semantics"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0073"]
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

## Implementation

- Added one Qt/provider/effect-free `diff_semantics` core boundary over the
  validated canonical result. Equal lines and accepted exact/similar pairs map
  to their exact counterpart; unpaired replacement lines expose the complete
  opposing span; one-sided lines expose the exact empty-side boundary.
- Kept exact move correspondence as optional secondary metadata. It never
  replaces deletion/insertion boundary mapping or creates an alternative
  group/navigation path.
- Projected the canonical ordered group vector and exact segment slices for
  later link-map consumption. Previous/next navigation consumes the validated
  diff directly, implements explicit stop/wrap outcomes, and rejects unknown
  group or enum values without ordinal guesses.
- Added line/group semantic selection, deterministic preferred group lines,
  exact replacement reconciliation, and lower-detail inputs that re-resolve
  canonical mapping identity. Nontext, stale comparison, invalid, and
  fabricated mapping/selection inputs fail closed.
- Strengthened retained-result validation so equal segments, exact pairs, and
  move relations must still contain normalized-equal source text. Mutated
  valid-UTF-8 bytes cannot preserve a false exact semantic relation.

## Direct verification

- Clean `core-only` build and focused matrix: 7/7 passed in 35.48 seconds
  (`core_diff_semantics`, `core_diff_compute`, `core_diff_model`, both core
  dependency/boundary gates, and both architecture boundary gates).
- Clean `dev` build and the same matrix: 7/7 passed in 36.06 seconds.
- Darwin ASan/UBSan build and the same matrix: 7/7 passed in 37.22 seconds
  with fail-fast runtime options. Leak detection remained disabled because
  Darwin AddressSanitizer does not support that mode; address and undefined-
  behavior instrumentation remained active.
- `core_diff_semantics` covers exact, similar, opposing-span, opposing-
  boundary, unavailable, and separate move results; insertion/deletion detail;
  unpaired and 4:7 tails; canonical projection/group traversal; stop/wrap and
  invalid navigation; line/group selection and reconciliation; lower-detail
  identity agreement; nontext, mutated, out-of-range, and fabricated inputs;
  and a generated small-file/options corpus.
- `core_diff_compute` mutation cases prove equal-segment, exact-pair, and move
  metadata cannot outlive normalized source equality.

## Review, simplification, and deferral

The exclusive writer remained within its five owned code/build/test files.
Root reviewed, formatted, simplified, strengthened, documented, and verified
the slice. Independent read-only review first found navigation trusting a
mutable projection, move correspondence lacking retained-text proof, and a
fabricated mapping forwarding caller diagnostics. Navigation now consumes the
canonical diff; moves validate normalized content; invalid detail is rebuilt
from canonical diagnostics. The remediation audit then found equal segments
and exact pairs lacked the same retained-text invariant. Their validator and
mutation cases close that final high finding; the final re-review reports no
remaining blocker or high correctness issue.

The simplification pass retained explicit tagged mapping/detail states and
selection reconciliation because they encode fail-closed semantics. Shared
helpers remove duplicate normalized-line comparison. Repeated whole-result
validation and linear segment/pair/group/move lookup are nonblocking scale
work explicitly retained by BPR-0080, together with bounded/indexed move
discovery and sub-block refinement.

## Documentation disposition

The diff architecture, system allocation, build guide, fixture catalog,
RFC-BPR-0004 ledger, issue board, and BPR-0080 scale card describe the live
semantic boundary and its proof limits. No Qt, host, provider, operator,
packaging, credential, GitHub mutation, or agent-instruction surface changed.
Windows execution remains intentionally open under the repository's target-
native evidence policy and does not block this core slice.

## Result

BPR-0074 is complete. Mapping, group projection/navigation, semantic
selection, lower detail, and move annotation now resolve from one validated
alignment identity without fake lines or a competing policy. BPR-0075 is
ready to implement exact host execution, caching, cancellation, and stale-
result rejection.
