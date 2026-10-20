---
schema: "implement-rfc.card.v1"
id: "BPR-1665"
title: "Upgrade the pinned Qt runtime to 6.12.0"
status: "Done"
rfcs: ["RFC-BPR-0088"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"qt-6-12-v1","path":"docs/rfcs/plans/qt-6-12-v1/milestone-plan.json"},"mandate_revision":"qt-6-12-r2","assigned_implementer":"/root/qt612","reservation":"docs/issues/_reservations/BPR-1665--BPR-1665--78dea174f1c7.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b433d515f5a3a513bcecc0f520d8d04c38454c6a","evidence":{"kind":"observation","detail":"Official Qt6.12.0 provider fresh-cache public path, supplied root, offline cache success, empty offline/wrong6.11.2/missingWebP refusal controls pass; actual installed frameworks and pinned Windows installer metadata verified. Evidence: docs/verification/RFC-BPR-0088.md; docs/issues/_checkpoints/BPR-1665/handoff.md; external /tmp/bpr-qt612-run/full-suite-r2/timings.json."}},{"criterion":"AC2","outcome":"pass","revision":"b433d515f5a3a513bcecc0f520d8d04c38454c6a","evidence":{"kind":"observation","detail":"Downloaded QtBase/Svg/ImageFormats and official Linux ICU archive match published and computed SHA256; raw ICU73.2 trio unchanged; producer SPDX/55 notice files inspected. Package29 cases (1native-Linux skip), source23 and security18 contracts pass; native packaging remains release scope. Evidence: docs/verification/RFC-BPR-0088.md; docs/issues/_checkpoints/BPR-1665/handoff.md; external /tmp/bpr-qt612-run/full-suite-r2/timings.json."}},{"criterion":"AC3","outcome":"pass","revision":"b433d515f5a3a513bcecc0f520d8d04c38454c6a","evidence":{"kind":"observation","detail":"Full macOS dev build and public app/composed runtime checks pass. Maintained full suite complete=true:810passed,1configured Debug cache-bytes skip,0failures across811 rows, exit0 in4407.875s. Native/Release optimized-memory obligations remain explicit; no v0.3.0 exception reused. Evidence: docs/verification/RFC-BPR-0088.md; docs/issues/_checkpoints/BPR-1665/handoff.md; external /tmp/bpr-qt612-run/full-suite-r2/timings.json."}},{"criterion":"AC4","outcome":"pass","revision":"b433d515f5a3a513bcecc0f520d8d04c38454c6a","evidence":{"kind":"observation","detail":"Active version/mac14.4 policy agrees with verified inputs; historical v0.3.0 evidence preserved. Final documentation gate and67 documentation controls pass; self-review and diff check pass. Durable checkpoint/ledger bind source, commands, skips, native/release gaps and command quiescence. Evidence: docs/verification/RFC-BPR-0088.md; docs/issues/_checkpoints/BPR-1665/handoff.md; external /tmp/bpr-qt612-run/full-suite-r2/timings.json."}}]
---

# BPR-1665: Upgrade the pinned Qt runtime to 6.12.0

Implement [RFC-BPR-0088](../../rfcs/RFC-BPR-0088-qt-6-12.md) in one bounded
milestone. No release publication or push. Native platform and release gaps
remain explicit. The verification ledger binds the candidate and actual checks.

## Acceptance

- AC1: Satisfy RFC AC01, preserving acquisition/version/module refusal gates.
- AC2: Satisfy RFC AC02 with actual source/ICU download hashes and focused package/source/security checks.
- AC3: Satisfy RFC AC03 through the available macOS app and composed offscreen checks using Qt6.12.0; retain unavailable native evidence.
- AC4: Satisfy RFC AC04, documentation checks and a durable command-quiescent checkpoint.

## Verification and execution record

The accepted source candidate is
`b433d515f5a3a513bcecc0f520d8d04c38454c6a`. Source/ICU hashes, actual pinned
provider success/refusal controls, package/source/security contracts, full
developer build and composed runtime paths pass. The maintained full macOS
suite completed all811 rows:810 passed, one configured Debug optimized-memory
skip, zero failures; exit0 in4407.875s. The skip must be discharged with a
tests-enabled Release build during v0.3.1 qualification. Native Windows/Linux,
Intel, trust, vulnerability and release evidence remains outside this bounded
upgrade. The separate Threads scrolling/flicker repair remains a release
blocker. See [verification](../../verification/RFC-BPR-0088.md) and the
[checkpoint](../_checkpoints/BPR-1665/handoff.md). No release or push occurred.
