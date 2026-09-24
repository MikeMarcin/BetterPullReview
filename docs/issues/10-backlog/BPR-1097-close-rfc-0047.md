---
schema: "implement-rfc.card.v1"
id: "BPR-1097"
title: "Verify and close RFC-BPR-0047"
status: "Backlog"
rfcs: ["RFC-BPR-0047"]
depends_on: ["BPR-1096"]
closeout_for: ["RFC-BPR-0047"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-23","milestone":"closeout","acceptance":["R01","R02","R03","R04","R05","R06","R07","R08","R40","R43","R46","R47","R48"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1097: Verify and close RFC-BPR-0047

Pack title: "Verify and close conventional-comment semantics".

Pack alias: **PACK-CC-23** (live `BPR-1097`, allocated at import).

RFC base: `b5cb3b43bb948b3e8e971a967b6772ef150c1d5a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| R01: All twelve stable labels are supported in every advertised feature; no aliases replace the three additional labels. | Not run; import only | No producing revision |
| R02: Original body is authoritative; plain and unknown-label bodies remain readable and unchanged. | Not run; import only | No producing revision |
| R03: The parser accepts the documented start-of-body subset and refuses quoted/code/HTML/later-paragraph false positives. | Not run; import only | No producing revision |
| R04: Standard and custom decorations survive losslessly; conditional and contradictory expectations remain distinct. | Not run; import only | No producing revision |
| R05: Recognition and source/preview/copy cycles preserve bytes; explicit header edits leave unrelated body spans intact. | Not run; import only | No producing revision |
| R06: Public intent, label guidance, explicit expectation, shared resolution, local defect lifecycle and review verdict remain independent. | Not run; import only | No producing revision |
| R07: Root identity determines a conversation's counting label; reply labels and missing-root cases cannot hijack it. | Not run; import only | No producing revision |
| R08: Website edits invalidate the right derived state without mutating GitHub or unrelated scopes. | Not run; import only | No producing revision |
| R40: Bounded untrusted parsing causes no resource access, execution, unsafe Markdown activation or silent truncation. | Not run; import only | No producing revision |
| R43 (cross-cutting): Fixture coverage includes all labels, syntax negatives, identity/coverage cases and real C++/composition tests. | Not run; import only | No producing revision |
| R46 (cross-cutting): Current architecture/user/skill documentation is reconciled and duplicate paths removed without rewriting settled history. | Not run; import only | No producing revision |
| R47 (cross-cutting): Import follows both tour packs with collision-aware RFC/issue mapping and adopted authored milestones. | Not run; import only | No producing revision |
| R48 (cross-cutting): Each formal closeout has truthful per-criterion evidence and follows the live canonical order. | Not run; import only | No producing revision |

## Outcome and work

Reconcile every RFC0047 acceptance row against exact core/host evidence and current source. Follow live canonical order; retain any unsupported/limited grammar behavior explicitly. Do not use pack self-validation as product proof.

## Acceptance

- All owned requirements have direct current evidence or remain open.
- Live aliases are mapped and source/identity invariants are independently reviewed.
- Status changes obey existing predecessor/closeout rules without historical edits.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
