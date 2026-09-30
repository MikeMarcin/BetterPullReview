---
schema: "implement-rfc.card.v1"
id: "BPR-1164"
title: "Append manual and structured recipient responses safely"
status: "Done"
rfcs: ["RFC-BPR-0057"]
depends_on: ["BPR-1158", "BPR-1162"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC57-04", "RC57-05", "RC57-08", "RC57-10"], "pack_alias": "PACK-CYCLE-22", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC57-04","outcome":"pass","revision":"b74cbe4140a0af608f281f0d5eb4f2e24e5fad43","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_response through the production runtime and store: a structured response bound to RC-1/F1/digest/I1,I2 rev 1 and two hand-entered replies (one item, whole round) append as separate statements with the observed channel, read back exactly after restart and survive reclaim; the desktop path pastes a reply and opens a response file in the real window, shown beside the note as claims."}},{"criterion":"RC57-05","outcome":"pass","revision":"b74cbe4140a0af608f281f0d5eb4f2e24e5fad43","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Replay of the same content is a no-op, a retried operation replays, identifier reuse with other content is reused_identifier; wrong cycle, round name, unfrozen round, digest, item and item revision refuse and append nothing; output queued before a cycle switch completes cancelled. Mutation skipping the revision check fails the course."}},{"criterion":"RC57-08","outcome":"pass","revision":"b74cbe4140a0af608f281f0d5eb4f2e24e5fad43","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share: prose replies need no JSON or identifier; partial responses cover only named items and unanswered items get no invented response; mixed recipients' statements about one item stand side by side (core and contract courses). BPR-1162 recorded the prose-before-capture share."}},{"criterion":"RC57-10","outcome":"pass","revision":"b74cbe4140a0af608f281f0d5eb4f2e24e5fad43","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Claimed agent/model, test_report and command-like evidence are kept as claims: the canary is never created, the folder is unchanged, no capture, note, round or review mark changes; verified/approved/trust fields are refused as shape; a claimed result resolves only after the reviewer captures it."}},{"criterion":"BPR-1164:boundary","outcome":"pass","revision":"b74cbe4140a0af608f281f0d5eb4f2e24e5fad43","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"94 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails), including architecture_boundaries and test_network_isolation_manifest with the new pin; clang-format clean. Linux and Windows runs owed with the selectors on the card."}}]
---

Pack alias: **PACK-CYCLE-22** (live `BPR-1164`, allocated at import).

# BPR-1164: Append manual and structured recipient responses safely

## Outcome and bounded work

Implement optional bounded response schema/import and manual prose attachment. Bind exact round/digest/item membership and record claimed versus observed author/evidence. Idempotently append reports without editing instructions, source or reviewer state.

## Acceptance and direct evidence

Partial/multi-recipient/prose reports, self-reported model, wrong round digest, unknown item, same report replay, conflicting ID reuse, forged verification flag, executable evidence text and late adapter output.

Criteria: RC57-04, RC57-05, RC57-08, RC57-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `b74cbe414`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_response$|^review_cycle_response$|^sqlite_review_cycle_storage$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` (measure the new `review_cycle_response` pin contained).
