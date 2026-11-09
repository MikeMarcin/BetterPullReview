---
schema: "implement-rfc.card.v1"
id: "BPR-1963"
title: "Tour fixture owns the source it copies while growing its stop vector"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "Windows43 diagnosis", "reservation": "docs/issues/_reservations/BPR-1963--BPR-1963--2a07c806134b.json", "milestone_plan": {"id": "bpr-1963-tour-fixture-value-lifetime", "path": "docs/rfcs/plans/bpr-1963-tour-fixture-value-lifetime/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Own initial source template value before vector growth; same stops/content/gestures/oracles, no product change.","artifact_root":"candidate43/implementer-contract-fixes"}},{"criterion":"AC2","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"ASan original actualexit250 heap-use-after-free in whole_tour/vector reallocation, corrected compile/link/run0/no finding; initial ASan Fortify macro compiler1 retained and corrected with effective originalFortify3. Normal ReleasePASS0.75s/DebugPASS0.78s; no Windows causal claim.","artifact_root":"candidate43/implementer-contract-fixes"}},{"criterion":"AC3","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Format22/focused tidy22, documentation and scoped board gatesPASS. Proven source intentionally integrated and ACTUALLY pushed authoritative main d84cde4d80164207d388ce41c63857a309eadff6. All protected producers unchanged, native/diagnostic commands ended/source copies restored. Final release/native trust and Windows new-wrapper measurement/pin remain coordinator/sibling obligations, not waived.","artifact_root":"candidate43/implementer-contract-fixes"}}]
---

# BPR-1963: Tour fixture owns its source across vector growth

## Acceptance

- AC1. Copy the initial Original source by value before pushing more stops; all constructed content, gestures, oracles and production behavior remain.
- AC2. Isolated one-TU AddressSanitizer negative/positive proof and normal Release/Debug registered qt_tour_agent_session_view pass after correction; retain exact negatives and make no unsupported Windows causal claim.
- AC3. Focused format/tidy/docs/board and intentional latest-main integration pass; exact source/binary/command ownership and pending full native qualification are explicit.

## Diagnosis

whole_tour holds a reference into tour.Stops.front().Original, then pushes three more stops. The first vector reallocation invalidates that reference and the next copy uses freed storage. The source template must be an owned value. Coordinator assigned only this test file; frozen producers remain untouched.

## Focused verification

Sourcef992a1d1b copies the template by value before vector growth; no content/oracle/gesture changes. Isolated one-TU ASan negative compiles/links0 then actual contained exit250 with heap-use-after-free READ147 in whole_tour, freed by vector<tour_stop>::__emplace_back_slow_path. Corrected source compiles/links/runs0 with tour-agent-session-view:pass and no ASan finding. First instrumentation compile1 (ASan builtin Fortify0 versus original Fortify3) is retained; r2 undefines only the builtin immediately before the unchanged original Fortify3 define, preserving effective hardening3, optimization, Werror, SDK and architecture. Runtime retains exact configured environment/envmods/cwd/networkdeny/30s; only diagnostic image/output paths differ. Engineering proof only, no Windows causal attribution. Final normal Release course PASS0.75s and Debug PASS0.78s. Format22/tidy22 pass.

Evidence: external candidate43/implementer-contract-fixes. All frozen producer source/builds stay untouched. Full common-source native qualification remains release-coordinator work.
