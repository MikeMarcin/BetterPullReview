---
schema: "implement-rfc.card.v1"
id: "BPR-1674"
title: "Select qualification from complete inputs and reuse equivalent builds"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1673"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "release-qualification-portfolio", "path": "docs/rfcs/plans/release-qualification-portfolio/milestone-plan.json"}, "mandate_revision": "release-30min-portfolio-r1", "RFC_BASE_SHA": "a1a91fa68"}
---

# BPR-1674: Select qualification from complete inputs and reuse equivalent builds

## Acceptance criteria

- AC1: Implement exact input and evidence identity with fail-closed unknown/change handling and visible fresh/reused/deferred scope.
- AC2: Map all eight composition methods to their assertions and owning configuration before removing duplicate compilation; retain unique generic tests-off, distribution, optimized and native obligations.
- AC3: Prove trigger/identity/failure controls and preserve unresolved material qualification choices as unready. Do not infer an acceptance waiver from the time target.

## Scope and provenance

Implements the owner’s thirty-minute release mandate `release-30min-portfolio-r1`, grounded in the retained Astra design (SHA-256 `ad0157d3693bf0ffda63d71504e7ee76c59a90c4fcb9185cfbff6ee0764812d1`). The [portfolio design](../../development/release-qualification-portfolio.md) and [milestone plan](../../rfcs/plans/release-qualification-portfolio/milestone-plan.json) retain boundaries and open choices. RFC-BPR-0038 remains open.

## Eligible frontier after M1

Start with the retained Astra optimized-qualification addendum and a bounded,
tests-on Windows Release discriminator using the existing runner/preflight. Preserve
its optimized branches and checked-iterator Debug residual; neither configuration
is a semantic superset. Decide the concrete route from measurements before any
full optimized matrix. Complete input closure and the eight-method composition
assertion/build matrix still precede conditional qualification or build removal.
The M1 [runtime ledger](../../verification/BPR-1673-command-owned-correctness.md)
contains current native source/cache and evidence boundaries. macOS/Linux activation,
full stabilized integration and the honest producer phase ledger remain M3 scope.
