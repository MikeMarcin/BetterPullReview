---
schema: "implement-rfc.card.v1"
id: "BPR-0752"
title: "Import the authored plan and reconcile the live source"
status: "Done"
rfcs: ["RFC-BPR-0030", "RFC-BPR-0031"]
depends_on: []
metadata: {"pack":"bpr-dif-integration-v1","milestone":"intake-contract","acceptance":["A8","B12"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Live source, IDs and baseline","outcome":"pass","revision":"3e9065b5e577e92113b8e4cd57fcda01be6e6c32","evidence":{"kind":"observation","detail":"Live import record binds pre-import HEAD 9ce46e3, 26-path comparison, RFC mapping and committed BPR-0752–0769 reservation; implementation intake hashes of the two changed composition seams are on the card."}},{"criterion":"Authored plan and skill ownership","outcome":"pass","revision":"0c3523d07520a1ccab28271a2b870b39b5f159eb","evidence":{"kind":"command","detail":"Installed milestone_plan.py target-only discover found valid live six-milestone plan; all 18 cards map to the adopted plan. Bundled updater remains archived, consumer context is MASK-managed."}},{"criterion":"Pack integrity and bounded scope","outcome":"pass","revision":"0c3523d07520a1ccab28271a2b870b39b5f159eb","evidence":{"kind":"command","detail":"sha256sum -c SHA256SUMS passed all payloads; verify_pack.py passed on a temporary exact copy excluding only two unmanaged .DS_Store files. Direct run rejected those extra files. Documentation gate returned DOCUMENTATION_OK. No art or application acceptance claimed."}}]
---

# BPR-0752: Import the authored plan and reconcile the live source

## Outcome

Verify the pack; resolve live RFC and issue allocations, immutable baseline, current path/ownership drift and exact eligibility. Import/remap the supplied six milestones rather than repartitioning the work.

## Acceptance

- Record live HEAD, snapshot difference, RFC ID mapping and all allocated issue-ID receipts.
- Every card belongs to the remapped authored plan; no retired shared skill is restored.
- Keep technical readiness separate from formal closeout; no unrelated predecessor work is added.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.

## Execution record

Implementation intake on `main` began from `3e9065b5e577e92113b8e4cd57fcda01be6e6c32`.
The [live import record](../../rfcs/assets/bpr-dif-integration-v1.LIVE_IMPORT.md)
binds pre-import HEAD `9ce46e3576a44441cfcd932fca128091275e0068`,
the 26-path snapshot comparison (24 exact matches and two named changed
composition paths), RFC-0030/0031, all eighteen mapped cards, and the committed
`BPR-0752`–`BPR-0769` allocation receipt. At this intake the two composition
paths had changed again: `workbench_window.cpp` SHA-256
`a0e43f3f45984d1f8d8edba719effa66e0a0d9b9d1078974174552ef5d2f06c6`
and `github_application.cpp` SHA-256
`b11ff53cabf8e4c9b90caa905cb94917f491ad3f5f88c28284537b55dfc51b82`.
Later app work must inspect those current seams; the imported overlay must not
replace them.

The installed plan helper's target-only discovery found the valid live
six-milestone plan. `sha256sum -c SHA256SUMS` matched every archived payload.
The pack verifier passed against a temporary byte-for-byte copy excluding only
two unmanaged `.DS_Store` files introduced within the archived tree; its direct
run correctly refused that extra inventory. This is a pack integrity and intake
result, not production-art or application acceptance. The bundled updater zip
remains archival; the consumer context stays under `.mask/skill-config/`.
