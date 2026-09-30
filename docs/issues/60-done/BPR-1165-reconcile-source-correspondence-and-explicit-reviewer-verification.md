---
schema: "implement-rfc.card.v1"
id: "BPR-1165"
title: "Reconcile source correspondence and explicit reviewer verification"
status: "Done"
rfcs: ["RFC-BPR-0057"]
depends_on: ["BPR-1153", "BPR-1163", "BPR-1164"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC57-03", "RC57-06", "RC57-07"], "pack_alias": "PACK-CYCLE-23", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC57-03","outcome":"pass","revision":"48412c586386f4d8c0aae862d9236febbf2d7fb9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_verification and core_review_cycle_verification: against R2 an exactly applied suggestion's note is unmapped with the result already_applied, a deleted file is deleted, a copied snippet ambiguous with two candidates, an illustrative fix unknown, a module note without correspondence; none resolves its note (addressed ones only need verification, a dispute is reported). Moved/exact correspondence rows are BPR-1153's."}},{"criterion":"RC57-06","outcome":"pass","revision":"48412c586386f4d8c0aae862d9236febbf2d7fb9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Claims and decisions are separate; verified/needs_work/withdrawn only by the reviewer's gesture (real window decision menu), bound to checked revision, current wording and material digest; unrelated change keeps it current, a relevant change gives recheck_needed with the R2 decision kept, amending gives item_amended; uncaptured revision and unknown note refused; clearing returns to the claim. Mutation keeping verifications current fails the course."}},{"criterion":"RC57-07","outcome":"pass","revision":"48412c586386f4d8c0aae862d9236febbf2d7fb9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The next round's draft carries F1's action, goal and constraints against the latest capture, the four unresolved items at current wording and suggestion revisions, leaves the withdrawn note out and adds the new note; no replies, old wordings or context; saved as a draft only, nothing frozen or sent until the reviewer freezes F2; the desktop Next round saves the same shape. Sharing is BPR-1161's surface."}},{"criterion":"BPR-1165:boundary","outcome":"pass","revision":"48412c586386f4d8c0aae862d9236febbf2d7fb9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"96 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails), including architecture_boundaries and test_network_isolation_manifest with the new pin; clang-format clean. Linux and Windows runs owed with the selectors on the card."}}]
---

Pack alias: **PACK-CYCLE-23** (live `BPR-1165`, allocated at import).

# BPR-1165: Reconcile source correspondence and explicit reviewer verification

## Outcome and bounded work

Observe current anchors against result material with explicit uncertainty, preserving original code. Separate reported addressed from reviewer verified, material-bound verification history and recheck-needed. Create next drafts from explicit carry-forward selection, not automatic history dumps.

## Acceptance and direct evidence

Repeated/moved/deleted source, illustrative fixes, already-present exact replacement, relevant material change after verification, missing source and amended item revision. No code disappearance or test claim automatically verifies.

Criteria: RC57-03, RC57-06, RC57-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `48412c586`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_verification$|^review_cycle_verification$|^sqlite_review_cycle_storage$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` (measure the new `review_cycle_verification` pin contained).
