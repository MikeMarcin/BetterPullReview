---
schema: "implement-rfc.card.v1"
id: "BPR-0561"
title: "Plan and import RFC-BPR-0022 semantic source-line comments"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: []
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e4c946bc2551fd9e36fdfdb74e64598b411bd2ba","evidence":{"kind":"observation","detail":"28 retained payloads match PACK-MANIFEST; immutable LIVE_IMPORT retains all 15 ID mappings."}},{"criterion":"AC2","outcome":"pass","revision":"e4c946bc2551fd9e36fdfdb74e64598b411bd2ba","evidence":{"kind":"observation","detail":"RFC-0008 unchanged from assignment source revision."}},{"criterion":"AC3","outcome":"pass","revision":"e4c946bc2551fd9e36fdfdb74e64598b411bd2ba","evidence":{"kind":"observation","detail":"All fifteen canonical RFC acceptance criteria remain in the live verification matrix."}},{"criterion":"AC4","outcome":"pass","revision":"e4c946bc2551fd9e36fdfdb74e64598b411bd2ba","evidence":{"kind":"observation","detail":"33 source/test paths exist; recovery schema 3, current CTest inventory and open external/canonical obligations recorded in source-audit.json."}},{"criterion":"AC5","outcome":"pass","revision":"e4c946bc2551fd9e36fdfdb74e64598b411bd2ba","evidence":{"kind":"observation","detail":"verify_documentation.py and git diff --check pass."}}]
---

# Plan and import RFC-BPR-0022 semantic source-line comments

Reconcile the pack with the live BetterPullReview tree, allocate all live issue IDs, add the RFC only to the post-pack index, create the live verification ledger, and record the architecture allocation and RFC base SHA.

## Acceptance

- The retained pack and `LIVE_IMPORT` record bind the exact imported bytes and ID mapping.
- RFC-BPR-0008 is not edited to absorb the new requirements.
- All fifteen canonical acceptance criteria have one live verification row.
- Source paths, current schema version, CTest selectors, and existing open dependencies are re-audited from live state.
- `python3 scripts/verify_documentation.py --root .` and `git diff --check` pass.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

The pack retention, identity allocation, RFC/index import, and initial source
reconciliation are complete in the import slice. The 2026-09-26 readiness audit completes this slice; feature criteria remain
pending in the live verification ledger.

## Readiness acceptance — 2026-09-26

Authored milestones A–D are adopted without changing the retained source.
All 28 pack payloads match their manifest and all 33 inventoried source/test
paths exist. The RFC baseline remains `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.
The [source audit](../_checkpoints/BPR-0575/source-audit.json) records live
CTest names and three material changes since import: recovery schema 3 includes
whole-review recovery; replies can originate on the left; normal tests-disabled
Publish uses its own one-use request grant. These are preservation obligations.
RFC-0004, 0005 and 0008 are Auto-Verified. RFC-0022 expressly permits development
without 0021, and the current mandate activates it. Canonical closeout ordering
and designated native/fallback evidence remain pending. No RFC-0008 bytes change.

Architecture allocation remains the live RFC's core/host/GitHub/SQLite/Qt table.
The shortest existing public course is `application_review_round_trip_composition_contract`;
future semantic and wire work must extend this selected application path.
Documentation and whitespace gates pass for the readiness candidate.
