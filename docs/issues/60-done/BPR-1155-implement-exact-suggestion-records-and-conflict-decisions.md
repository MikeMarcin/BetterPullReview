---
schema: "implement-rfc.card.v1"
id: "BPR-1155"
title: "Implement exact suggestion records and conflict decisions"
status: "Done"
rfcs: ["RFC-BPR-0055"]
depends_on: ["BPR-1153", "BPR-1150"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-04", "RC55-05", "RC55-06"], "pack_alias": "PACK-CYCLE-13", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC55-04","outcome":"pass","revision":"29c8262dcd34121571477a74af39e1fd8e7d3e48","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_suggestion and core_review_cycle_suggestion: a retained selection with CRLF, tab and a two-byte scalar becomes a suggestion whose original/replacement bytes, byte range and intent read back exactly after reopen; insertion and deletion are the empty sides; a no-op exact edit is refused; the in-memory preview gives the whole after file and its before/after lines from the retained original; the base64 wire form round-trips NUL/CRLF/tab/UTF-8 and refuses tampered lengths, hashes, ranges and fields."}},{"criterion":"RC55-05","outcome":"pass","revision":"29c8262dcd34121571477a74af39e1fd8e7d3e48","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Out-of-order edits of different lengths apply in original coordinates (ascending-order mutation fails); overlapping, contained and identical ranges, two insertions at one boundary and insertions on a replaced range's boundary conflict and apply nothing; a changed file whose range still matches is stale_blob (precondition mutation fails); two files in one set refused; revalidation only onto the exact original bytes."}},{"criterion":"RC55-06","outcome":"pass","revision":"29c8262dcd34121571477a74af39e1fd8e7d3e48","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Suggesting, previewing and preflighting leave the workspace snapshot and retained-object inventory unchanged; a similar later edit is unknown and refused as an application (mutation fails); the exact result is recorded observed_already_applied with its revision, preflight reports already_applied instead of applying twice, and no further revision is accepted; illustrative edits are never applied."}},{"criterion":"BPR-1155:boundary","outcome":"pass","revision":"29c8262dcd34121571477a74af39e1fd8e7d3e48","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"ctest review-cycle suggestion/feedback/storage/core selectors 13/13 on macOS arm64; clang-format and clang-tidy clean on touched units. Linux and Windows runs owed with the ledger commands."}}]
---

Pack alias: **PACK-CYCLE-13** (live `BPR-1155`, allocated at import).

# BPR-1155: Implement exact suggestion records and conflict decisions

## Outcome and bounded work

Add byte-based proposal records and before/after material. Support insertion/deletion, exact versus illustrative intent and observed already-applied edits. Implement pure all-edit preflight with whole-blob preconditions and conservative overlap/same-boundary rejection.

## Acceptance and direct evidence

Original-coordinate multiple edits, overlapping ranges, two insertions, stale whole blob with locally matching text, CRLF/tab/Unicode preservation, illustrative non-apply and already-present no-double-apply cases.

Criteria: RC55-04, RC55-05, RC55-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

`core_review_cycle_suggestion` and `review_cycle_suggestion` pass.

The build is clean under GCC with no warnings. The logs are station records in `build/logs/bpr-station-authoring/` and are not committed.
