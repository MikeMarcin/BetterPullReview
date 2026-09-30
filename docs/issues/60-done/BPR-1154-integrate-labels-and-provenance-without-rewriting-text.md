---
schema: "implement-rfc.card.v1"
id: "BPR-1154"
title: "Integrate labels and provenance without rewriting text"
status: "Done"
rfcs: ["RFC-BPR-0055"]
depends_on: ["BPR-1153"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-03", "RC55-08", "RC55-10"], "pack_alias": "PACK-CYCLE-12", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC55-03","outcome":"pass","revision":"f269f88c3a3b308fc2a941047797c8b6f6d1e6ce","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Cycle share. macOS arm64 core_review_cycle_disclosure, review_cycle_feedback and qt_markdown_body_{1_0,1_25,2_0}: all twelve labels, Plain and malformed/quoted/code/unknown/CRLF/emoji/NUL bodies keep exact bytes and digests through store, restart, disclosure and the shared native Markdown reader (label shown as written, copy_source returns the authored bytes); no label implies a suggestion, lifecycle, provenance or state. Compatibility decision: the shared recognizer is BPR-1076 (RFC-BPR-0047, held by the owner's start hold); cycles add no label field or cycle-only set; RFC closeout BPR-1188 judges RC55-03 whole."}},{"criterion":"RC55-08","outcome":"pass","revision":"f269f88c3a3b308fc2a941047797c8b6f6d1e6ce","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Human, adapter-observed, self-reported and unknown revisions stay distinct; a pasted model claim stays self_reported. Disclosure defaults to the note's current wording; replies, observed/pasted answers, superseded wording, private references and adapter/provider/model/effort appear only when named; lineage never travels; archived notes and incomplete histories are refused."}},{"criterion":"RC55-10","outcome":"pass","revision":"f269f88c3a3b308fc2a941047797c8b6f6d1e6ce","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Relabel and undo are edited_from successors in the shared private-thread lineage, survive restart through the production store, and leave the note's suggestion at revision 1 with exact bytes."}},{"criterion":"BPR-1154:boundary","outcome":"pass","revision":"f269f88c3a3b308fc2a941047797c8b6f6d1e6ce","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Named ctest selectors pass on macOS arm64; clang-format and clang-tidy clean on touched units; Linux and Windows runs owed with the ledger command."}}]
---

Pack alias: **PACK-CYCLE-12** (live `BPR-1154`, allocated at import).

# BPR-1154: Integrate labels and provenance without rewriting text

## Outcome and bounded work

Reuse the all-twelve-label/Plain parser and existing Markdown composer. Keep raw body authoritative; record human, observed adapter, self-reported and unknown provenance. Maintain deliberate public/private selection and exact suggestion compatibility.

## Acceptance and direct evidence

All labels plus malformed/plain bodies, model-identity claims, unselected hidden turns, undo/restart, prefix edit preserving suggestion bytes and renderer/source round trip.

Criteria: RC55-03, RC55-08, RC55-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

`core_review_cycle_disclosure`, `review_cycle_feedback` and `qt_markdown_body_1_0`, `_1_25` and `_2_0` pass.

The build is clean under GCC with no warnings. The logs are station records in `build/logs/bpr-station-authoring/` and are not committed.
