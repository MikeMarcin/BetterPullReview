---
schema: "implement-rfc.card.v1"
id: "BPR-1101"
title: "Verify and close RFC-BPR-0051"
status: "Backlog"
rfcs: ["RFC-BPR-0051"]
depends_on: ["BPR-1100"]
closeout_for: ["RFC-BPR-0051"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-27","milestone":"closeout","acceptance":["R34","R35","R36","R37","R38","R39","R41","R48"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1101: Verify and close RFC-BPR-0051

Pack title: "Verify and close convention-aware hooks and CLI parity".

Pack alias: **PACK-CC-27** (live `BPR-1101`, allocated at import).

RFC base: `b5cb3b43bb948b3e8e971a967b6772ef150c1d5a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| R34: Defaults teach the complete convention when relevant, while every core feature works with no AI configured. | Not run; import only | No producing revision |
| R35: Effective task-specific hooks, overrides and default upgrades are inspectable and preserved; tour hooks keep their distinct purposes. | Not run; import only | No producing revision |
| R36: No background inference, nested model launch, automatic publication or new AI/network authority is introduced. | Not run; import only | No producing revision |
| R37: AI edits remain previewable draft candidates, preserve uncertainty and patches, and use actual private provenance when available. | Not run; import only | No producing revision |
| R38: CLI and desktop use identical core/application semantics with additive documented output and explicit machine formatting. | Not run; import only | No producing revision |
| R39: Headless operations reuse imported shared draft/owner/recovery services and do not launch the GUI or create a second publisher. | Not run; import only | No producing revision |
| R41: Migration and cross-interface/window concurrency preserve user settings, drafts and exact revision ownership. | Not run; import only | No producing revision |
| R48 (cross-cutting): Each formal closeout has truthful per-criterion evidence and follows the live canonical order. | Not run; import only | No producing revision |

## Outcome and work

Reconcile effective default/override hook behavior and shared headless semantics through actual composition. Verify no required core operation needs a model, no nested agent/publisher appeared, and inherited tour paths remain intact.

## Acceptance

- Default/custom/disabled/no-agent modes and cross-interface conflicts pass their actual boundaries.
- Published CLI grammar/schema and canonical skill source match the shipped implementation.
- Final report names exact remaining limitations and only records earned status transitions.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
