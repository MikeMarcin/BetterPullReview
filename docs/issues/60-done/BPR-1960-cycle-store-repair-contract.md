---
schema: "implement-rfc.card.v1"
id: "BPR-1960"
title: "Review Cycle security course verifies accepted private-store repair"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "frozen43Mac qualification / latest-main source audit", "reservation": "docs/issues/_reservations/BPR-1959--BPR-1961--64e658d8c3db.json", "milestone_plan": {"id": "bpr-1959-1961-mac-release-contracts", "path": "docs/rfcs/plans/bpr-1959-1961-mac-release-contracts/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Accepted1870same-owner store0644→0600 repair checked via show success/exact mode/current owner/single-link regular/device+inode equality; Windows/domain/root/privacy oracles retained.","artifact_root":"candidate43/implementer-contract-fixes"}},{"criterion":"AC2","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Complete normal Review Cycle securityPASS22.73s including formerly unreached checks; owning SQLite storage privacy controlPASS0.39s; raw43refusal expectation retained as stale-contract negative.","artifact_root":"candidate43/implementer-contract-fixes"}},{"criterion":"AC3","outcome":"pass","revision":"d84cde4d80164207d388ce41c63857a309eadff6","evidence":{"kind":"observation","detail":"Format22/focused tidy22, documentation and scoped board gatesPASS. Proven source intentionally integrated and ACTUALLY pushed authoritative main d84cde4d80164207d388ce41c63857a309eadff6. All protected producers unchanged, native/diagnostic commands ended/source copies restored. Final release/native trust and Windows new-wrapper measurement/pin remain coordinator/sibling obligations, not waived.","artifact_root":"candidate43/implementer-contract-fixes"}}]
---

# BPR-1960: Review Cycle security course verifies accepted private-store repair

## Acceptance

- AC1. For POSIX own single-link store0644, require show success, exact0600 current-owner regular file and unchanged device/inode; preserve Windows behavior, foreign/link/root/domain/privacy oracles.
- AC2. Old refusal assertion fails against accepted BPR-1870; normal security course and owning storage privacy control pass with unchanged budgets.
- AC3. Format/docs/board and intentional latest-main integration pass; retain raw43failures, exact proof scope and pending final native qualification.

## Mandate

Coordinator approved this exact test-contract scope. Frozen producer source/builds remain untouched; all source fixes land authoritative main.

## Focused verification

Original frozen43 expected-storage_unavailable failure is retained. Accepted BPR-1870 explicitly narrows same-owner adapter-owned store files, so source2ce97e9a8 requires actual show success, exact0600/current-owner/single-link regular file and unchanged device/inode. Full normal security course PASS22.73s, including all previously unreached protocol/privacy checks; sqlite_storage_root_contract PASS0.39s preserves linked/foreign-object refusal. Windows branch and original root/domain/oracles/budgets remain unchanged. No storage policy waiver or production change.

Evidence: external candidate43/implementer-contract-fixes. All frozen producer source/builds stay untouched. Full common-source native qualification remains release-coordinator work.
