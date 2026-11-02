---
schema: "implement-rfc.card.v1"
id: "BPR-1840"
title: "Tests-enabled public-preview driver waits for action readiness before latching Opened"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue": "BPR-1811", "external_state": "/Users/jaeger/Documents/Codex/2026-11-02/task/release-closeout", "frozen_release_source": "d9d4a822dbf603c7bb22c83574f5a64d2bee5645", "scope": "Future-main test-only correction. Frozen N8 packages unchanged."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"634e92ca457e56f18c736e3efb88360ff96e431d","evidence":{"kind":"artifact","detail":"Narrow reviewed test-only diff adopted; frozen D9/N8 source and assets unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"634e92ca457e56f18c736e3efb88360ff96e431d","evidence":{"kind":"artifact","detail":"Both full original40s courses PASS with allsource/inputpostseals/owneddrain, no performance/raworiginalPASScredit. All portable history/control subcases retained, sixordinaryreads/recreation/identity checks retained."}},{"criterion":"AC3","outcome":"pass","revision":"634e92ca457e56f18c736e3efb88360ff96e431d","evidence":{"kind":"artifact","detail":"Verified bound native proof hashes; source controls/formatting and documentation gate passed. Original failures retained."}}]
---

# Tests-enabled public-preview driver waits for action readiness before latching Opened

## Acceptance

- AC1: Adopt only the reviewed fixture/oracle correction; preserve all other assertions and frozen source/assets.
- AC2: Bind complete native public-path proofs and negative controls, retaining original failures separately.
- AC3: Pass focused source controls, formatting and documentation checks; record evidence accurately.

## Prepared proof

Both full original40s courses PASS with allsource/inputpostseals/owneddrain, no performance/raworiginalPASScredit. All portable history/control subcases retained, sixordinaryreads/recreation/identity checks retained.

Original handoff: `/Users/jaeger/.codex/release-runs/v0.4.1-20261029/main-refresh-r3-pending-authoritative-test-card-allocations-r2.json`.

## Accepted closeout

Prepared source SHA-256: `c576b275ead12e5efdeb511bf9362b4b76145a8ef4fc860e294d8642f25b338b`. Complete native proofs and negative controls are bound by the retained allocation request; all 44 raw-nonpass proof hashes were verified on 2026-11-02. Original raw failures remain unchanged. The prospective named expiry diagnostic has source controls and formatting evidence only; no native compilation claim for that reporting-only addition. Focused source controls and documentation checks passed. No frozen producer source, cohort, package or runtime library changed.
