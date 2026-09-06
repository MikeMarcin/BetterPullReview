---
schema: "implement-rfc.card.v1"
id: "BPR-0625"
title: "Compare large files with bounded ordered unique-line matching"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0623"]
acceptance: [{"criterion":"Exact bounded primary matching","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"Core 10520a43: original matrix path retained; actual map/comparator/scalar work charged before execution with an independent limit. Independent review correction consumed; no remaining scoped blocker."}},{"criterion":"Semantic equivalence and refusal","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"Final dev core_diff_compute passed 1.18s; core-only rebuilt after correction and core_diff_compute/core_diff_semantics passed 1.20s/0.58s. Tests include 1600 reference pairs, normalization, ambiguity, and exact/exhausted budgets."}},{"criterion":"Public stored large-file path","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"application_large_review_benchmark passed 27.30s on source now committed in e9da7abc. Exact 20000-line panes and 20 changes, navigation, idle wheel invariants and final selection under outstanding work passed."}},{"criterion":"Architecture and scope","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"Both architecture gates passed in core-only (15.12s/80.68s) and dev (15.11s/80.66s). git diff --check passed. No network, GitHub Actions, paid CI, credentials or production app used."}}]
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

## Implementation and verification

Core source `10520a43` retains the existing matrix route and adds an exact
ordered unique-line fallback. The original 16,777,216-cell ceiling is unchanged;
a separate 67,108,864-unit budget charges each index operation, comparator call
and normalized scalar comparison before work. Repeated/crossing common lines
and exhausted work return an atomic typed refusal. The stored large comparison
uses 19,126,565 index units and 20,360 matrix cells.

Independent read-only review found that a tree-depth estimate undercounted
actual comparator calls. The final implementation charges the actual calls and
stops immediately on exhaustion; review found no remaining scoped blocker.
Focused unit checks cover 1,600 small sequence pairs against the original matrix,
normalization/source identity, ambiguous refusal, exact/exhausted budgets and a
20,000-line comparison with 20 changes.

On the final source candidate `e9da7abc` (which includes `10520a43`), dev
`core_diff_compute` passes (1.18s) and the composed large-review course passes
(27.30s), including exact displayed blobs/source, navigation, scrolling and
selection supersession. Core-only was rebuilt after the accounting correction;
`core_diff_compute`, `core_diff_semantics`, `architecture_boundaries` and
`architecture_boundary_contract` pass 4/4 (97.58s). These checks ran locally with
denied network access. BPR-0626 owns the separate unavailable-state message;
BPR-0627 owns the measured UI stalls. Neither is claimed fixed here.
