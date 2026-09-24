---
schema: "implement-rfc.card.v1"
id: "BPR-1099"
title: "Verify and close RFC-BPR-0049"
status: "Backlog"
rfcs: ["RFC-BPR-0049"]
depends_on: ["BPR-1098"]
closeout_for: ["RFC-BPR-0049"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-25","milestone":"closeout","acceptance":["R20","R21","R22","R23","R24","R25","R26","R27","R28","R29","R44","R48"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1099: Verify and close RFC-BPR-0049

Pack title: "Verify and close label counters and settings".

Pack alias: **PACK-CC-25** (live `BPR-1099`, allocated at import).

RFC base: `b5cb3b43bb948b3e8e971a967b6772ef150c1d5a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| R20: Settings can select/order dedicated counters for every supported label independently of support or authoring mode. | Not run; import only | No producing revision |
| R21: Dedicated buckets and catchall form an exact disjoint partition for every selected subset and declared scope. | Not run; import only | No producing revision |
| R22: One logical conversation counts once by root identity; replies, pages and links cannot inflate it. | Not run; import only | No producing revision |
| R23: Unresolved is the initial primary mode; including resolved retains a discoverable resolution distinction. | Not run; import only | No producing revision |
| R24: Unknown/non-applicable resolution and incomplete classification/collection coverage are explicit, never false clean zeros. | Not run; import only | No producing revision |
| R25: File, folder and PR scopes preserve exact attribution, unanchored remainder, tour roots and identity deduplication. | Not run; import only | No producing revision |
| R26: Website edits, deletes, reopens, partial paging and stale observations update only the accepted relevant population. | Not run; import only | No producing revision |
| R27: Counter drill-down and sorting use the same numeric population and snapshot semantics as the displayed counts. | Not run; import only | No producing revision |
| R28: Settings persist through the existing typed authority with scoped reset, cross-window propagation and profile isolation. | Not run; import only | No producing revision |
| R29: Cached classification and bounded incremental projection avoid per-paint parsing, per-label fetches and unnecessary resets. | Not run; import only | No producing revision |
| R44 (cross-cutting): Measured performance uses representative existing and larger bounded synthetic workloads without invented guarantees. | Not run; import only | No producing revision |
| R48 (cross-cutting): Each formal closeout has truthful per-criterion evidence and follows the live canonical order. | Not run; import only | No producing revision |

## Outcome and work

Reconcile Overview accounting, partial-state clarity, exact drill-down, settings and performance evidence. Check all selected-subset tests plus actual application behavior; follow canonical order.

## Acceptance

- Counters conserve the population for all label subsets and state scopes.
- Unknown/partial data, profile sharing and settings failure are presented honestly.
- Measured claims are bounded to actual targets/workloads and producing revisions.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
