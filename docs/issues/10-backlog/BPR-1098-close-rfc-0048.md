---
schema: "implement-rfc.card.v1"
id: "BPR-1098"
title: "Verify and close RFC-BPR-0048"
status: "Backlog"
rfcs: ["RFC-BPR-0048"]
depends_on: ["BPR-1097", "BPR-1095"]
closeout_for: ["RFC-BPR-0048"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-24","milestone":"closeout","acceptance":["R09","R10","R11","R12","R13","R14","R15","R16","R17","R18","R19","R42","R45","R48"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1098: Verify and close RFC-BPR-0048

Pack title: "Verify and close native feedback authoring integration".

Pack alias: **PACK-CC-24** (live `BPR-1098`, allocated at import).

RFC base: `b5cb3b43bb948b3e8e971a967b6772ef150c1d5a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| R09: Ordinary and conventional human-only authoring work without any agent or mandatory label. | Not run; import only | No producing revision |
| R10: Controls and Markdown share one revisioned draft; undo, stale edits and repeated prefix edits are lossless. | Not run; import only | No producing revision |
| R11: Preview, saves and classification publish nothing; explicit existing publication authorization remains required. | Not run; import only | No producing revision |
| R12: Native suggestion bodies survive all unrelated transformations, including empty and multiline replacements. | Not run; import only | No producing revision |
| R13: Applicability follows exact native capability/anchor facts; fallback content never pretends to be an applicable patch. | Not run; import only | No producing revision |
| R14: All three native review decisions remain explicit and use existing native submission/recovery paths. | Not run; import only | No producing revision |
| R15: Preflight is useful, scoped, draft-aware and uncertainty-aware, without automatic verdicts or invented merge enforcement. | Not run; import only | No producing revision |
| R16: Defect tracking and verification remain explicit; public labels and remote resolution do not drive private lifecycle transitions. | Not run; import only | No producing revision |
| R17: Private severity/category/provenance do not leak into public comment text or public counters. | Not run; import only | No producing revision |
| R18: Available private-note/finding promotion paths compose through the shared draft; absent deferred paths are not fabricated. | Not run; import only | No producing revision |
| R19: Tour preparation, normal comments, ordering, native links and independent hook purposes are preserved. | Not run; import only | No producing revision |
| R42: Crashes, conflicting edits and indeterminate transmission cannot duplicate publication or bind a revised body to an old action. | Not run; import only | No producing revision |
| R45 (cross-cutting): Native website interoperability is verified only on an explicitly authorized safe target; absent evidence remains open. | Not run; import only | No producing revision |
| R48 (cross-cutting): Each formal closeout has truthful per-criterion evidence and follows the live canonical order. | Not run; import only | No producing revision |

## Outcome and work

Close the actual authoring/native integration only after required shared-flow and authorized website evidence. Distinguish compatibility boundaries for absent deferred note/placement paths from implemented behavior.

## Acceptance

- All native decisions and suggestions retain correct public semantics and explicit action authority.
- Private lifecycle/provenance and tour coexistence are proved on the implemented paths.
- No absent live evidence or deferred interface is called completed by implication.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
