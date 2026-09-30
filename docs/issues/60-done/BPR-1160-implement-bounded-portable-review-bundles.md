---
schema: "implement-rfc.card.v1"
id: "BPR-1160"
title: "Implement bounded portable review bundles"
status: "Done"
rfcs: ["RFC-BPR-0056"]
depends_on: ["BPR-1158"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-05"], "pack_alias": "PACK-CYCLE-18", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC56-05","outcome":"pass","revision":"674889e6c92437f5e08cfb56174f8d2f9fc5ad12","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 core_review_cycle_bundle, core_review_cycle_bundle_pack_oracle, review_cycle_bundle and sqlite_review_cycle_storage: the pack's deflated bundle opens with its digest and document; a rebuilt bundle is deterministic and accepted by the pack's reference oracle; traversal/drive/UNC/NUL/encoded path corpus, duplicate/case-fold/NFC-NFD aliases, symlink and directory entries, encryption and unsupported methods, 100:1 and over-declared inflation, count and total bounds, overlapping members, CRC, manifest, invalid base64/UTF-8, duplicate JSON keys, version and hash mismatches, edited review.md and stray or wrong attachments are refused before admission; a valid bundle opens into another profile's quarantine with sender identities kept; repeats and retries find one record; a damaged bundle installs nothing; nothing is extracted to any folder."}},{"criterion":"BPR-1160:boundary","outcome":"pass","revision":"674889e6c92437f5e08cfb56174f8d2f9fc5ad12","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"30 focused ctest rows pass on macOS arm64 including architecture_boundaries and test_network_isolation_manifest; clang-format and clang-tidy clean; overlap mutation fails the core course. Linux and Windows runs owed with the ledger command."}}]
---

Pack alias: **PACK-CYCLE-18** (live `BPR-1160`, allocated at import).

# BPR-1160: Implement bounded portable review bundles

## Outcome and bounded work

Create/read .bpr-review.zip with readable document, authoritative round envelope and bounded content-addressed attachments. Verify manifest paths/hashes/lengths and admit only complete immutable imported material to private quarantine.

## Acceptance and direct evidence

Traversal/drive/UNC/NUL/encoded path corpus, duplicate/casefold/normalization aliases, symlink entry, excessive ratio/size/count, invalid base64/UTF-8, duplicate JSON keys and hash mismatch; prove no extraction to source roots.

Criteria: RC56-05.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `674889e6c`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_bundle|^review_cycle_bundle$|^sqlite_review_cycle_storage$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` for the new `review_cycle_bundle` pin
(exercises the `CreateFileW` chosen-file read).

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `bfc02529a`.

`core_review_cycle_bundle`, `core_review_cycle_bundle_pack_oracle` (`round-bundle-oracle:pass`, Python 3 on this station), `review_cycle_bundle` and `sqlite_review_cycle_storage` pass. The from-scratch ZIP reader and writer agree with the oracle on Linux.

The build is clean under GCC with no warnings, and no source change was needed. The logs are station records in `build/logs/bpr-station-rounds/` and are not committed.
