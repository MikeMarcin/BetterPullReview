---
schema: "implement-rfc.card.v1"
id: "BPR-1159"
title: "Implement complete Markdown and clipboard/file rendering"
status: "Parked"
rfcs: ["RFC-BPR-0056"]
depends_on: ["BPR-1158"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-03", "RC56-04", "RC56-08"], "pack_alias": "PACK-CYCLE-17", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC56-03","outcome":"pass","revision":"10c3154cd7d2db7428a05ae52ef1e13f56b60822","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 review_cycle_export: clipboard text, saved file and the document rendered from the round read back through a restarted store are identical bytes carrying the canonical digest and item identities; a repeated save writes nothing. Bundle share is BPR-1160; CLI and adapter shares are later cards."}},{"criterion":"RC56-04","outcome":"pending","revision":"10c3154cd7d2db7428a05ae52ef1e13f56b60822","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes (core_review_cycle_round_markdown): the pack's eight-item, two-suggestion round renders to exactly its review.md; items stay outside fences in order; unclosed fences, forged headings, setext headings and HTML comments are contained verbatim; nested fences, empty, invalid, trailing-space and unterminated bytes are explained with exact base64. Owner judgment of human readability without BPR is pending as one attended item."}},{"criterion":"RC56-08","outcome":"pass","revision":"10c3154cd7d2db7428a05ae52ef1e13f56b60822","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"A document over its channel budget is refused with its full size and counts, never truncated; exactly at budget it is complete; oversized selections are already refused with their measure at assembly (BPR-1158)."}},{"criterion":"BPR-1159:save-conflict","outcome":"pass","revision":"10c3154cd7d2db7428a05ae52ef1e13f56b60822","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Other bytes at the destination are a conflict naming their digest; a stale confirmation stays a conflict; confirming the current digest replaces; replace re-checks; directories and links are not_regular; missing folder and relative paths refused; no temporary file left; workspace unchanged; no absolute path in the document. Linux renameat2 and Windows MoveFileExW runs owed."}}]
---

Pack alias: **PACK-CYCLE-17** (live `BPR-1159`, allocated at import).

# BPR-1159: Implement complete Markdown and clipboard/file rendering

## Outcome and bounded work

Render the same round into readable Markdown and plain clipboard/file bytes with stable IDs, useful source excerpts and dynamic fences. Preserve or explicitly encode non-text patch bytes. Enforce output budgets/completeness without silently dropping items.

## Acceptance and direct evidence

Eight-item/two-suggestion export, nested code fences, empty replacement, invalid text, selected context deduplication, sensitive absolute path omission, save conflict and over-limit round. Compare all selected item IDs and exact bytes.

Criteria: RC56-03, RC56-04, RC56-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `10c3154cd`. Owed on the Linux station (GCC 15, `build/dev`, exercises the `renameat2` no-replace move):
`ctest --test-dir build/dev -R "^core_review_cycle_round_markdown$|^review_cycle_export$|^review_cycle_round$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`, exercises `MoveFileExW`): the same selector plus
`|^test_network_isolation_manifest$` for the new `review_cycle_export` pin.

## Pending owner judgment

RC56-04 asks whether a person can understand and act on the saved document
without BPR. It is asked as one Accept/Reject item,
`BPR-1159-readable-round-v001`, in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (added at sequence 642). The candidate is
`/Users/jaeger/bpr-cycles-portable-review.MRsbKJ/RC-17-F1.md` (SHA-256
`79186a22…fc236b3d4`), written by the product renderer at `a5eb785a4` for the
pack's eight-item round, with the same round as `RC-17-F1.bpr-review.zip`
beside it; both are read-only. Every automated share is recorded in the ledger.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `bfc02529a`.

Evidence only; the card stays parked for the owner. `core_review_cycle_round_markdown`, `review_cycle_export` and `review_cycle_round` pass.

The build is clean under GCC with no warnings, and no source change was needed. The logs are station records in `build/logs/bpr-station-rounds/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `bfc02529`, `build/windows-dev` (MSVC 19.51) builds with no warning and no
source change. The portable-rounds rows pass: the core round, markdown,
bundle and import rows; `review_cycle_{round,export,bundle,attach,feedback,suggestion}`;
and `sqlite_review_cycle_storage`. `core_review_cycle_bundle_pack_oracle`
passes after `77c8fa0b`, which gives its Python scratch root the contained
helper. The four new uncontained pins are measured and confirmed. The details,
including Save to file's Windows replace semantics, are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, portable rounds".

No native Windows attended run was made. This entry adds evidence only, and the card stays Parked.
