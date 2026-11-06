---
schema: "implement-rfc.card.v1"
id: "BPR-1897"
title: "Decouple Overview people and merge assessment"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1895"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1897--BPR-1897--d081652bf43f.json","milestone_plan":{"id":"responsiveness-latency-v10","path":"docs/rfcs/plans/responsiveness-latency-v10/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance; retain existing Status sections."}
---

# BPR-1897: Decouple Overview people and merge assessment

The owner requests independent Overview reviewers, merge rules and progress,
working directly, preserving other behavior and landing each verified round on
main with a card and one journal line.

Merge assessment arrives before reviewer requests, and the complete reviewer
roster arrives before branch rules/protection. Present those read-only sections
at those boundaries without reordering or adding requests. Keep rules and local
progress explicitly loading until their own data is ready.

## Acceptance

- AC1: Observed authenticated first opens publish merge assessment and the
  complete people/request roster independently, before remaining policy reads;
  checks retain their earlier publication. Refresh/signed-out/unobserved paths,
  request ordering/counts, shared budgets and final validation stay unchanged.
- AC2: Admit each bounded observation once after metadata for the exact current
  read; validate records, identities, shape and text bounds and refuse invalid,
  stale, mixed, cancelled or terminal observations. Grant no accepted snapshot,
  epoch, mutation or link authority.
- AC3: In the composed offscreen app, held reviewer/rule reads leave completed
  Status sections readable with clear remaining loading. Disclosure and check
  selection survive later sections and final acceptance; replacement/failure
  clears provisional content. Old-reader controls fail the early-content claim.
- AC4: Measure optimized before/after section visibility; pass focused provider,
  core/host, Qt/public-path, architecture, coding and documentation checks,
  record one journal line and land on main.

## Plan

Reuse the existing observation channel, bounded core admission, shared result
policies and Status sections. Publish compact participant facts without copying
comment bodies. Keep unavailable data distinct from still-loading data. Apply
frontend-design guidance to the existing desktop surface, with no redesign.

## Implementation and direct evidence

The first authenticated open publishes merge assessment after its read, then the
complete participant/request roster after review-request pagination and fallback.
Checks remain earlier. The remaining branch rules/protection and local progress
are explicitly loading. No request is added or reordered; final snapshot
validation and authority remain unchanged. Compact roster observations omit
conversation bodies, and request-consistency validation now copies only authors.

The host validates bounded records and identities, binds the viewer to the pending
account, rejects mixed/stale/terminal data and posts each independent section at
most once. Core admission changes no accepted snapshot, epoch or action authority.
The existing Status sections preserve disclosures and check selection; replacement,
failure and cancellation clear provisional content.

Direct implementation applies frontend-design desktop guidance and code-simplifier
guidance to the existing surface. The offscreen composed course holds reviewers
and rules independently, clicks a check, opens the sections and verifies completion.
The inspected render is `sections-render/status-people-before-rules.png`; no owner
desktop is accessed. The old-reader control fails the intended early-merge
assertion (`sections-old-reader-control.log`).

Evidence root:
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`.

## Measurements

Three serialized optimized Release fixture samples per side, macOS arm64,
AppleClang 21, Qt 6.12, two files and one check, 1.0x scale, 330 ms per simulated
read. No build, lint or other test was started concurrently with these samples.

| Median | Previous reader | Independent sections |
|---|---:|---:|
| First PR: merge assessment visible | 6,381 ms | 3,676 ms |
| First PR: people visible | 6,381 ms | 4,342 ms |
| First PR: full acceptance | 6,381 ms | 6,350 ms |
| First PR: requests | 19 | 19 |
| Second PR: merge assessment visible | 6,028 ms | 3,691 ms |
| Second PR: people visible | 6,028 ms | 4,356 ms |
| Second PR: full acceptance | 6,028 ms | 6,029 ms |
| Second PR: requests | 21 | 21 |

These show earlier useful sections, not faster total acquisition or live-network
latency. Checks remain at about 3.35 seconds. Commands and all samples are in
`sections-before-commands.json`, `sections-final-commands.json` and
`sections-timing-summary.json`. The second opening retains its existing extra
history reads. No Debug timing course is run or credited.

## Verification

All eleven focused Release courses pass (`sections-functional-final.log`): core
window reads, application read host, provider paging, overview policy/widget,
composed window at 1.0x/1.25x/2.0x, overview chips at 1.25x/2.0x, and the progressive
open public path. The strict rendered course also passes. Provider evidence
compares the entire final snapshot and wire-operation sequence with an unobserved
read; refresh observation restrictions and cancellation at each boundary pass.

Initial failures remain in external logs: two compile corrections (the actor
validator's required-login argument and explicit string construction) and a test
that incorrectly used the essential-read page counter to count separately budgeted
policy reads. The test now records actual wire operations. Those runs are not
passing evidence. The initial documentation run caught newer reservations on main;
board reconciliation and the final gate are required before landing.

Core-only configure/build and `core_github_window_read` pass
(`sections-core-{configure,build,functional}.log`). Both architecture courses pass
(`sections-architecture.log`, 2/2), clang-format 22 passes all 20 touched C++ files
(`sections-format.log`), and all 67 documentation regression tests pass
(`sections-doc-tests.log`). The direct simplification review preserves shared
policies and restricts changes to the independent sections.

Full clang-tidy 22 passes all thirteen changed translation units
(`sections-tidy.log`). Final review also preserves the early merge assessment's
colors when the appearance changes; the affected page passes focused full lint
and formatting (`sections-palette-{tidy,format}.log`).

Rewritten main through `2d714bdc62f97fff5f29b75494651351eb8be196` is merged without
conflicts, bringing the shared reservations current. The rebuilt combined tree
passes all five affected UI/integration courses: overview widget, composed window
at all three scales, and progressive opening (`sections-integrated-functional.log`).
Unchanged provider/core/host and timing evidence is reused. Documentation and
scoped board validation pass (`sections-docs-integrated.log`,
`sections-board-precommit.json`); one journal line records the improvement.

The two milestones remain direct work with no delegated agents or global host
setting changes. The explicit milestone binding is retained in
`round-ten-binding.json`; the existing global checkpoint-discovery limitation
on unrelated amended plans remains outside this scope. Source landing and guarded
card completion follow this verified candidate.
