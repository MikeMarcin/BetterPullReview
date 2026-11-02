---
schema: "implement-rfc.card.v1"
id: "BPR-1838"
title: "Minimal CPack runtime fixture supplies release cohort JSON default"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue": "BPR-1811", "external_state": "/Users/jaeger/Documents/Codex/2026-11-02/task/release-closeout", "frozen_release_source": "d9d4a822dbf603c7bb22c83574f5a64d2bee5645", "scope": "Future-main test-only correction. Frozen N8 packages unchanged."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"5478c1c3fe6e3a489f4af4669abd9e42edb30815","evidence":{"kind":"artifact","detail":"Narrow reviewed test-only diff adopted; frozen D9/N8 source and assets unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"5478c1c3fe6e3a489f4af4669abd9e42edb30815","evidence":{"kind":"artifact","detail":"Both Release/Debug full32tests PASS; separate wrong-type true controls fail exact True is not None. Original JSONDecodeError retained. No R/producer/package edit."}},{"criterion":"AC3","outcome":"pass","revision":"5478c1c3fe6e3a489f4af4669abd9e42edb30815","evidence":{"kind":"artifact","detail":"Verified bound native proof hashes; source controls/formatting and documentation gate passed. Original failures retained."}}]
---

# Minimal CPack runtime fixture supplies release cohort JSON default

## Acceptance

- AC1: Adopt only the reviewed fixture/oracle correction; preserve all other assertions and frozen source/assets.
- AC2: Bind complete native public-path proofs and negative controls, retaining original failures separately.
- AC3: Pass focused source controls, formatting and documentation checks; record evidence accurately.

## Prepared proof

Both Release/Debug full32tests PASS; separate wrong-type true controls fail exact True is not None. Original JSONDecodeError retained. No R/producer/package edit.

Original handoff: `/Users/jaeger/.codex/release-runs/v0.4.1-20261029/main-refresh-r3-pending-authoritative-test-card-allocations-r2.json`.

## Accepted closeout

Prepared source SHA-256: `399634697c5b1403a317fb6e2171595c73dde7476281329e1aab740161bb8ceb`. Complete native proofs and negative controls are bound by the retained allocation request; all 44 raw-nonpass proof hashes were verified on 2026-11-02. Original raw failures remain unchanged. Focused source controls and documentation checks passed. No frozen producer source, cohort, package or runtime library changed.
