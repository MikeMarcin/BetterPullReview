---
schema: "implement-rfc.card.v1"
id: "BPR-1162"
title: "Attach external rounds with explicit project mapping"
status: "Done"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0057"]
depends_on: ["BPR-1160", "BPR-1146", "BPR-1147"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-05", "RC57-08"], "pack_alias": "PACK-CYCLE-20", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC56-05","outcome":"pass","revision":"51be3f6cbd771371d448ec4762d7b3e2c50d8544","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 core_review_cycle_import and review_cycle_attach: a second profile opens a bundle without account or agent; association needs an explicit project mapping (same alias in two projects, no mapping and a path-like label resolve nothing), a mapping for every sender revision over captured local revisions (a project with no capture is refused) and exactly the named bytes (a mismatched source is named and nothing recorded); repeats change nothing and re-association is refused; forks start unknown-provenance notes at the mapped bytes once; the quarantined record, payload and sender names stay byte-for-byte unchanged; a third profile imports independently; quota failure admits nothing; cycle removal releases association and forks; project folders are never written."}},{"criterion":"RC57-08","outcome":"pass","revision":"51be3f6cbd771371d448ec4762d7b3e2c50d8544","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share: prose-only, partial and mixed-recipient replies (unknown and self_reported turns) attach by hand, R2 is captured with no structured response, unanswered items stay as written, claims stay self-reported and F1 is unchanged. Structured responses are BPR-1164's share."}},{"criterion":"BPR-1162:boundary","outcome":"pass","revision":"51be3f6cbd771371d448ec4762d7b3e2c50d8544","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"33 focused ctest rows pass on macOS arm64 including architecture_boundaries and test_network_isolation_manifest; clang-format and clang-tidy clean; byte-check mutation fails the core course. Linux and Windows runs owed with the ledger command."}}]
---

Pack alias: **PACK-CYCLE-20** (live `BPR-1162`, allocated at import).

# BPR-1162: Attach external rounds with explicit project mapping

## Outcome and bounded work

Allow opening a portable review without an account/agent. Validate immutable foreign IDs and explicit logical-project/source mapping before optional association; edits fork local authoring with provenance. Make readable prose usable without requiring structured return.

## Acceptance and direct evidence

Second local profile/import, same alias in two projects, invalid root hint, mapping to mismatched source, repeat import and read-only failure preservation. A bundle never applies code or overwrites sender identity.

Criteria: RC56-05, RC57-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `51be3f6cb`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_import$|^review_cycle_attach$|^core_review_cycle_feedback$|^review_cycle_feedback$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` for the new `review_cycle_attach` pin.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `bfc02529a`.

The owed Linux command, `ctest --test-dir build/dev -R "^core_review_cycle_import$|^review_cycle_attach$|^core_review_cycle_feedback$|^review_cycle_feedback$" --output-on-failure`, passes 4 of 4.

The build is clean under GCC with no warnings, and no source change was needed. The logs are station records in `build/logs/bpr-station-rounds/` and are not committed.
