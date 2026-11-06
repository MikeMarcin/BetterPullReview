---
schema: "implement-rfc.card.v1"
id: "BPR-1882"
title: "Project review comment parents without repeated ancestry walks"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1882--BPR-1883--c940b7c58a89.json","milestone_plan":{"id":"responsiveness-latency-v1","path":"docs/rfcs/plans/responsiveness-latency-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2531aae697f5b71b477fc3a49b4579dd32c2af3d","evidence":{"kind":"observation","detail":"Iterative memoized ancestry retains exact immediate parent, order and missing/cyclic fallback; exhaustive independent four-comment graph oracle and large reversed threads pass."}},{"criterion":"AC2","outcome":"pass","revision":"2531aae697f5b71b477fc3a49b4579dd32c2af3d","evidence":{"kind":"observation","detail":"Release CTest application_github_projection_mapping and application_github_host pass; both also passed against unchanged product baseline. Production architecture_boundaries and core_dependency_boundary pass."}},{"criterion":"AC3","outcome":"pass","revision":"2531aae697f5b71b477fc3a49b4579dd32c2af3d","evidence":{"kind":"observation","detail":"Same Release harness: 10,000-comment chain median 1330.330 to 1.408 ms, cycle 2832.872 to 1.469 ms; one-line journal committed. Format/tidy 22, documentation gate, 67 documentation regressions and diff check pass. Inherited architecture contract fixture failure recorded with baseline reproduction."}}]
---

# BPR-1882: Project review comment parents without repeated ancestry walks

The owner requested simplification and optimization focused on responsiveness
and latency on 2026-11-06, preserving other behavior and recording each notable
improvement in a one-line journal. Work directly, without delegated agents.

`project_github_pull_request` runs on the serialized decision lane. Every reply
currently walks its ancestors again to detect a cycle, making a deep thread
quadratic even though its parent relationships are immutable for the projection.

## Acceptance

- AC1: Resolve ancestry once per thread; preserve comment order, immediate parent
  identities, missing-parent fallback, and cycle/descendant fallback.
- AC2: Exercise reordered, branched, missing and cyclic relationships, plus
  large threads and the existing application-host projection path.
- AC3: Record before/after optimized measurements, focused checks and a one-line
  journal entry; preserve the existing authority and projection boundary.

## Evidence

`review_comment_parents` indexes a thread once and memoizes whether each
ancestry reaches a root or a cycle. Its iterative path keeps long threads off
the call stack. Comment order and immediate-parent identities stay unchanged;
missing parents and cycles keep their existing fallback. The projection also
reserves room for all comments and public-preview replies before appending,
avoiding vector growth that repeatedly moves the existing rows.

Release, Apple M4 Pro arm64, macOS 26.6.2, AppleClang 21.0.0, default test
allocator; median of five projections in milliseconds. Fixture construction is
outside the measurement. Baseline product source is `6ec53ca3c`; both runs use
the same `github_projection_mapping --measure` executable source and build
configuration, with no concurrent task-owned build or test run.

| Comments | Chain before | Chain after | Cycle before | Cycle after |
|---|---:|---:|---:|---:|
| 1,000 | 8.461 | 0.220 | 18.036 | 0.209 |
| 5,000 | 287.550 | 1.024 | 630.837 | 0.976 |
| 10,000 | 1,330.330 | 1.408 | 2,832.872 | 1.469 |

These are synthetic projection measurements on this machine, not a complete
application latency or cross-platform timing guarantee. No Debug timing gate
was run or claimed.

- `application_github_projection_mapping` passes: every four-comment graph
  (1,296 combinations) in forward and reverse order, mixed branches/missing
  parents/cycles, and 10,000-comment chains and cycles. The independent
  reachability oracle checks exact output parent identities and retained bodies.
- `application_github_host` passes through the accepted snapshot's ordinary
  application-host projection, including thread nesting and metadata.
- Both functional courses also passed against the pre-change product source.
- clang-format 22.1.8 and clang-tidy 22.1.8 pass on both changed translation
  units; `git diff --check`, the documentation gate and its 67 regression tests
  pass. `architecture_boundaries` and `core_dependency_boundary` pass.

The broader `architecture_boundary_contract` course fails on its unchanged
GitHub CLI factory fixture. Its positive
`test_designated_github_effect_sources_are_allowed` control reproduces the same
factory-ownership refusal on an export of baseline `6ec53ca3c`; this is not
passing boundary-contract evidence. BPR-1883 records the composed integration
checks and the separate pre-existing network-manifest failure.

The [journal](../../development/responsiveness-journal.md) records the improvement.
