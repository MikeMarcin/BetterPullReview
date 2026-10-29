---
schema: "implement-rfc.card.v1"
id: "BPR-1812"
title: "Accept the observed stable tag and numbered MSI floor"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c62b4ae0ff4a3a031427fff0f3a0e418a4870832","evidence":{"kind":"observation","detail":"Bare or v-prefixed stable base tags bind a numbered MSI to marketing version and exact build-to-MSI mapping. RC tag builds must match installer. Existing schema/digest/ordering/ProductCode/history protections retained. Actual previous stable native MSI0.4.1, build1, ProductCodeACE8ED5B, ALLUSERS1, UpgradeCode18DF84A8 and cfbd8aee digest captured in native-observe-windows-r1/receipt.json."}},{"criterion":"AC2","outcome":"pass","revision":"c62b4ae0ff4a3a031427fff0f3a0e418a4870832","evidence":{"kind":"command","detail":"Exact committed candidate release_build_tests:16 PASS, including direct allocation/freeze for stable0.4.0/build1→0.4.1/build2/MSI0.4.2, wrong tag/build/MSI, rollback/equal-version published identity replacement/no-reservation controls. Native release contracts:63PASS, one native Windows-only SKIP; documentation gate PASS. Raw first real allocator refusal preserved, no N reserved."}}]
---

# Accept the observed stable tag and numbered MSI floor

The 0.4.1 allocator refuses the actual preceding stable observation: bare tag `0.4.0`, release 428060158, with `BetterPullReview-0.4.0-build.1-windows-x64.msi`. Existing validation accepts only a v-prefixed tag and derives the filename directly from that tag. Fresh native observation confirms MSI ProductVersion `0.4.1`, distinct ProductCode, retained UpgradeCode, per-machine scope and the exact published installer digest. No new release number was reserved before this refusal.

## Acceptance

- AC1: Admit the actual base stable tag with its correctly bound numbered installer identity and the existing legacy/numbered RC floors. Preserve exact release/digest fields, number bounds, MSI/marketing ordering, ProductCode safeguards and historical rollback/replacement refusal. Reject mismatched tag/version, installer/version/build and MSI mapping.
- AC2: Exercise direct allocation/freeze with the published stable floor and meaningful negative controls, including native-floor history rollback and replacement. Reconcile the actual fresh MSI/public evidence and complete focused tests/documentation before retrying the real allocator. No unsigned/native installation or scanner acceptance is inferred.

## Verification

Committed fix `c62b4ae0ff4a3a031427fff0f3a0e418a4870832` passes all 16 release-build cases, including the direct allocation/freeze path and negative identity/history controls. Native release contracts pass 63 cases and retain one native Windows-only skip. Documentation and diff checks pass. The real preceding MSI observation and all raw receipts remain in the external run root. No native installation acceptance or scanner pass is claimed; the next step is the real authoritative allocation with the captured configurations.
