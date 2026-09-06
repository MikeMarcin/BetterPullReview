---
schema: "implement-rfc.card.v1"
id: "BPR-0625"
title: "Compare large files with bounded ordered unique-line matching"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0623"]
---

# Compare large files with bounded ordered unique-line matching

BPR-0624's composed course loads the full 515-file/1,000-thread workload,
then reaches an unavailable large-file result. The primary LCS matrix for
20,000 lines per side exceeds the existing 16,777,216-cell budget before
matching the 20 separated changes. Do not raise that matrix ceiling.

## Bounded acceptance and ownership

- Preserve the current matrix algorithm whenever it fits its existing budget.
  When it does not, allow a separately work-bounded ordered index only when
  every common normalized line occurs once on each side and their positions
  are strictly increasing. These matches constitute the complete unique LCS;
  repeated or reordered common lines must refuse rather than choose new ties.
- Keep normalization, source bytes, secondary pairing, stable identity,
  coverage, intraline refinement and optional move policy unchanged. Record
  primary-index work separately from matrix cells and bound index operations
  before performing them. Bound memory by the existing admitted line counts.
- Verify exact-budget and exhausted-budget behavior, repeated/reordered
  refusal, normalization and insertion/deletion cases, and equivalence to the
  existing matrix result for representable inputs. Exercise the actual stored
  20,000-line comparison through BPR-0624's composed application course.
- Own `src/core/{include/bpr/core/diff_compute.hpp,src/diff_compute.cpp}`,
  focused core diff tests, and these cards/closeout. Run focused dev/core-only
  diff tests, both architecture gates and the composed benchmark. Keep all
  work local; no GitHub Actions, network, pushes, paid CI or live credentials.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
This is a bounded exact fast path, not a general replacement diff algorithm.
BPR-0624 remains open until its full interaction acceptance passes. The
existing unavailable-result UI saying zero changes is a separate diagnostic
follow-up; this card does not claim that behavior is fixed.
