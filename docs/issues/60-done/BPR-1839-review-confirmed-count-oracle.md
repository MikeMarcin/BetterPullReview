---
schema: "implement-rfc.card.v1"
id: "BPR-1839"
title: "Comment opt-out status oracle checks specific confirmed submission count"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue": "BPR-1811", "external_state": "/Users/jaeger/Documents/Codex/2026-11-02/task/release-closeout", "frozen_release_source": "d9d4a822dbf603c7bb22c83574f5a64d2bee5645", "scope": "Future-main test-only correction. Frozen N8 packages unchanged."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a2e3bd37fa6fd3a610c95e8fde2b609db8d5d57d","evidence":{"kind":"artifact","detail":"Narrow reviewed test-only diff adopted; frozen D9/N8 source and assets unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"a2e3bd37fa6fd3a610c95e8fde2b609db8d5d57d","evidence":{"kind":"artifact","detail":"Full original Debug scale2 public script PASS at original90s/internalwaits; original17.54s rawFAIL/unchangedReplayFAIL/loggingProbeFAIL retained. Precise product trace legitimate Review submitted with 1 comment; allnavigation/setting flags correct. No product/cohort change."}},{"criterion":"AC3","outcome":"pass","revision":"a2e3bd37fa6fd3a610c95e8fde2b609db8d5d57d","evidence":{"kind":"artifact","detail":"Verified bound native proof hashes; source controls/formatting and documentation gate passed. Original failures retained."}}]
---

# Comment opt-out status oracle checks specific confirmed submission count

## Acceptance

- AC1: Adopt only the reviewed fixture/oracle correction; preserve all other assertions and frozen source/assets.
- AC2: Bind complete native public-path proofs and negative controls, retaining original failures separately.
- AC3: Pass focused source controls, formatting and documentation checks; record evidence accurately.

## Prepared proof

Full original Debug scale2 public script PASS at original90s/internalwaits; original17.54s rawFAIL/unchangedReplayFAIL/loggingProbeFAIL retained. Precise product trace legitimate Review submitted with 1 comment; allnavigation/setting flags correct. No product/cohort change.

Original handoff: `/Users/jaeger/.codex/release-runs/v0.4.1-20261029/main-refresh-r3-pending-authoritative-test-card-allocations-r2.json`.

## Accepted closeout

Prepared source SHA-256: `a9f034e20d5d682bd6ef597ed2775bd0199d90697a64d6b29fa977ea036d8150`. Complete native proofs and negative controls are bound by the retained allocation request; all 44 raw-nonpass proof hashes were verified on 2026-11-02. Original raw failures remain unchanged. Focused source controls and documentation checks passed. No frozen producer source, cohort, package or runtime library changed.
