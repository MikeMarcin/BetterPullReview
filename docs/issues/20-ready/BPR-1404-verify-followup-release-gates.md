---
schema: "implement-rfc.card.v1"
id: "BPR-1404"
title: "Repair timing and finish Windows and Ubuntu follow-up release gates"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"release-v019-20261012-macos-r3","reservation":"docs/issues/_reservations/BPR-1404--BPR-1404--ef22c2dc4a83.json","milestone_plan":{"id":"release-v019-gate-repairs","path":"docs/rfcs/plans/release-v019-gate-repairs/milestone-plan.json"}}
---

# Follow-up release gates

The owner's 2026-10-12 amendment makes v0.1.9 macOS-only and accepts its
current timing misses. This card preserves the original outstanding work for
an unversioned follow-up patch. The same implementer resumes after the root
coordinator freezes the v0.1.9 source; do not mutate its clean release clone or
protected candidate. Choose no new version without an owner/coordinator mandate.

## Acceptance

- AC1: Diagnose and repair the unchanged 100 ms Mac PR-load input-gap failures without weakened budgets or assertions. Preserve original 111.602/107.923 ms and isolated 104.440/101.098 ms failures and bounded profiling evidence from BPR-1403.
- AC2: Synchronize one clean committed follow-up candidate to the native Windows and Ubuntu 26.04 producers, pass relevant normal native gates and affected contracts, and account for configured platform skips and all actual remaining failures before package handoff.
- AC3: Preserve intended full Windows/Ubuntu package/source/SPDX/digest/trust obligations and broader RFC0038 limits. Complete agent-owned repairs and upkeep; delegate no source work outside the assigned implementer and obtain publication/version authority separately.

## Frontier

Windows coordinator-owned dev producer at `c9a416475` is still rebuilding;
no normal native test result is supplied yet. Ubuntu configured but no gate
started. Prior source repairs and Mac passes are in BPR-1403 and its ledger.
`build/v0.1.9-repairs-timing-repro.log` preserves the isolated misses;
`build/v0.1.9-repairs-gui-profile-test.log` identifies adjacent GUI MetaCall
work at about 90 and 42 ms during load. Instrumented timings are diagnostic,
not acceptance. Exact pre-instrument source was restored. Release version
remains 0.1.9 until the follow-up is separately selected.
