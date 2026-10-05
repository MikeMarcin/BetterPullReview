---
schema: "implement-rfc.card.v1"
id: "BPR-1075"
title: "Import after both tour packs and reconcile the live contracts"
status: "Done"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050", "RFC-BPR-0051"]
depends_on: []
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-01","milestone":"intake","acceptance":["R19","R39","R47"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"35e0312b62a11c2f38df847f06c4b5bb8e7323cb","evidence":{"kind":"artifact","path":"docs/verification/RFC-BPR-0047-0051.intake-audit.json","detail":"Import-committed id-map (27 unique aliases to BPR-1075..1101), reservation receipt docs/issues/_reservations/BPR-1075--BPR-1101--243d1ef0e225.json and counter docs/issues/NEXT_ID (BPR-1286) re-verified; every live card's edges/RFCs/milestone/acceptance IDs equal its pack card; RFC IDs 0047-0051 unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"35e0312b62a11c2f38df847f06c4b5bb8e7323cb","evidence":{"kind":"artifact","path":"docs/verification/RFC-BPR-0047-0051.intake-audit.json","detail":"Plan work covers each of the 27 cards exactly once; all 8 authored milestones equal after alias substitution; installed discover/bind resolve. Three delivered archives byte-identical to retained copies; predecessor digests b4f45e7e..78d6 and 3cdc6a07..6782 equal the sidecars; predecessor ranges BPR-0991..1016 and BPR-1052..1071 and RFC IDs 0039-0046 verified unremapped."}},{"criterion":"AC3","outcome":"pass","revision":"35e0312b62a11c2f38df847f06c4b5bb8e7323cb","evidence":{"kind":"artifact","path":"docs/verification/RFC-BPR-0047-0051.md","detail":"Ledger tables name Current versus Deferred/Absent promotion and placement paths and the exact inherited owners: map_tour_stop Narrative and parse_review_permalink_body segments, shared_draft_application_host and review_tour_host drafts, bpr executable and owner channel v3, four writing hooks with settings/CLI owners, absent feedback/workshop hook, pr-tour candidate (BPR-1067 Parked)."}},{"criterion":"intake:exit:1","outcome":"pass","revision":"35e0312b62a11c2f38df847f06c4b5bb8e7323cb","evidence":{"kind":"artifact","path":"docs/verification/RFC-BPR-0047-0051.intake-audit.json","detail":"All new IDs and external predecessor references (RFC-BPR-0044-0046 in RFC-BPR-0051 depends_on) are mapped to live identities."}},{"criterion":"intake:exit:2","outcome":"pass","revision":"35e0312b62a11c2f38df847f06c4b5bb8e7323cb","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.intake-checkpoint.json","detail":"milestone_checkpoint.py bind/check over the adopted eight-milestone plan: definition and prerequisite fingerprints unchanged; check returned fresh-context toward semantics."}}]
---

# BPR-1075: Import after both tour packs and reconcile the live contracts

Pack alias: **PACK-CC-01** (live `BPR-1075`, allocated at import).

## Outcome and work

Verify this archive. Read actual predecessor manifests/import maps, live HEAD, source drift and the installed import interface. Allocate the five RFCs and 27 cards without touching settled identities. Record the tour body-segment, shared draft, headless owner and hook ownership seams. Adopt the eight milestones; document only necessary representation changes.

## Acceptance

- The complete RFC/issue map and returned allocator receipt/counter paths are committed.
- The imported plan covers each live card exactly once; predecessor hashes/ranges are verified or explicitly qualified.
- Current versus deferred promotion/placement paths and exact inherited hook/CLI owners are identified.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Intake evidence, 2026-10-05

Owner release 2026-10-05; run `bpr-conventional-comments-20261005`, execution base
`1cd828ba8114ea3a2e639c671a8e47f5edf22fb5`, historical import base
`b5cb3b43bb948b3e8e971a967b6772ef150c1d5a`. The full record is the
[execution ledger](../../verification/RFC-BPR-0047-0051.md), its
[audit](../../verification/RFC-BPR-0047-0051.intake-audit.json) and its
[binding](../../verification/RFC-BPR-0047-0051.intake-checkpoint.json).

- **Map, receipt and counter.** These were committed at import. The audit confirms that the 27 aliases are unique, equal the receipt `docs/issues/_reservations/BPR-1075--BPR-1101--243d1ef0e225.json`, and sit below `NEXT_ID` (`docs/issues/NEXT_ID`, BPR-1286). It also confirms that every live card's edges, RFCs, milestone and acceptance IDs equal its pack card. RFC IDs 0047–0051 are unchanged.
- **Plan and predecessors.** The plan covers each live card exactly once, and all eight authored milestones are equal after alias substitution. The installed helper discovers the plan and binds this milestone. The three delivered archives are byte-identical to the retained copies. The review-tour (`b4f45e7e…78d6`) and pr-tour CLI (`3cdc6a07…6782`) digests equal the pack's sidecars. Predecessor RFC IDs 0039–0046 are unremapped, and the predecessor card ranges are BPR-0991–1016 (all Done) and BPR-1052–1071 (1052–1066 Done, 1067 Parked, 1068–1071 Backlog).
- **Seams and owners.** The ledger names each owner exactly. The tour stop segment is `core::map_tour_stop` `Narrative`; the permalink prose is `parse_review_permalink_body`. The ordinary comment draft is `shared_draft_application_host`, and the tour draft with its headless owner is `review_tour_host`, `bpr` and owner channel v3. The four writing hooks are in the workbench settings store and `bpr tour hooks`; no feedback or workshop hook exists. The ledger also separates Current from Deferred promotion and placement paths.

Limitations: these are intake checks only. No product code exists or was built, and
no GitHub service, model or skill installation was used. RFC-level dependencies on
RFC-BPR-0021 (Deferred) and on 0036, 0037 and 0044–0046 (Proposed) remain open for
the closeout cards.
