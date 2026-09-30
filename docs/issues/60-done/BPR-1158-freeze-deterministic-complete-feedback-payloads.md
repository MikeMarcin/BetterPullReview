---
schema: "implement-rfc.card.v1"
id: "BPR-1158"
title: "Freeze deterministic complete feedback payloads"
status: "Done"
rfcs: ["RFC-BPR-0056"]
depends_on: ["BPR-1153", "BPR-1154", "BPR-1155"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-01", "RC56-02", "RC56-06", "RC56-09"], "pack_alias": "PACK-CYCLE-16", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC56-01","outcome":"pass","revision":"09e3ce3c3beb469fa6cd84983c5a0efef967ff4a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 core_review_cycle_round and review_cycle_round: a stored draft selection freezes through the production runtime and SQLite store into one bpr.cycle.round.v1 payload holding every selected note's wording, primary and included related locations, exact CRLF suggestion bytes, selected source bytes, explicit action and the chosen order, with counts and instruction_set_complete; the pack's round.json canonical digest 061dd800...8d86acd is reproduced and an assembled course digest is pinned."}},{"criterion":"RC56-02","outcome":"pass","revision":"09e3ce3c3beb469fa6cd84983c5a0efef967ff4a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Identical inputs give identical bytes; reordering or an action change changes the digest; repeating a freeze of one draft revision reuses F1 and a retried operation replays; an edited note is refused stale_wording and the new selection freezes F2; F1 reads back byte for byte after reopening the store."}},{"criterion":"RC56-06","outcome":"pass","revision":"09e3ce3c3beb469fa6cd84983c5a0efef967ff4a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The canonical payload bytes contain no unselected note, reply, pasted answer, superseded wording, private related location, adapter/provider/model name, workspace absolute location or store root (core and store courses)."}},{"criterion":"RC56-09","outcome":"pass","revision":"09e3ce3c3beb469fa6cd84983c5a0efef967ff4a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"A cancelled freeze leaves no round; a payload installed before a simulated crash is only an unreferenced object and the retried freeze adopts the same bytes with nothing left unreferenced; retry replays and a new request reuses, leaving exactly one logical round. BPR-1145's SIGKILL-at-COMMIT course for freeze is unchanged."}},{"criterion":"BPR-1158:boundary","outcome":"pass","revision":"09e3ce3c3beb469fa6cd84983c5a0efef967ff4a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"19 focused ctest rows pass on macOS arm64 including architecture_boundaries and test_network_isolation_manifest; clang-format and clang-tidy clean on touched units; stale-wording mutation fails the core course. Linux and Windows runs owed with the ledger command."}}]
---

Pack alias: **PACK-CYCLE-16** (live `BPR-1158`, allocated at import).

# BPR-1158: Freeze deterministic complete feedback payloads

## Outcome and bounded work

Implement one canonical round assembler using versioned selections, exact suggestions, deduplicated explicit source context and assignment/order. Freeze after safe editor flush; keep immutable digest and successor amendments. No required AI summarization or second prompt.

## Acceptance and direct evidence

Golden canonical JSON/digests, identical re-freeze reuse, reordered items change digest, CRLF/base64 exactness, hidden-turn exclusion, IME/busy/stale draft and interrupted freeze recovery through production services.

Criteria: RC56-01, RC56-02, RC56-06, RC56-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `09e3ce3c3`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_round$|^review_cycle_round$|^core_review_cycle_suggestion$|^review_cycle_suggestion$|^sqlite_review_cycle_storage$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` for the new `review_cycle_round` pin.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `bfc02529a`.

`core_review_cycle_round`, `core_review_cycle_suggestion`, `review_cycle_round`, `review_cycle_suggestion` and `sqlite_review_cycle_storage` pass.

The build is clean under GCC with no warnings, and no source change was needed. The logs are station records in `build/logs/bpr-station-rounds/` and are not committed.
