---
schema: "implement-rfc.card.v1"
id: "BPR-1153"
title: "Add source-attached private feedback and related locations"
status: "Done"
rfcs: ["RFC-BPR-0055"]
depends_on: ["BPR-1145", "BPR-1151"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-01", "RC55-02", "RC55-10"], "pack_alias": "PACK-CYCLE-11", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC55-01","outcome":"pass","revision":"e5688006cfe3c9e7e0d0be85fd47383ee558062c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_feedback and core_review_cycle_feedback: through the production runtime and store, one discussion anchors a range in src/parser.cpp with a related range in src/lexer.cpp from full-source retained bytes with only a body; file, module and cycle notes need no range or line; notes on an older revision and on unchanged code are accepted; no title, label, severity, recipient or model is required. Card share; native authoring surface is BPR-1156/1157."}},{"criterion":"RC55-02","outcome":"pass","revision":"e5688006cfe3c9e7e0d0be85fd47383ee558062c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Original revision/path/digest/byte range/bytes/context never rewritten; against later captures: exact_material, exact_mapped (moved, 49-61), ambiguous (two copies, no range chosen), deleted, unavailable for unknown/out-of-scope/unread; rebinding appends a reference. Live-workspace-only bytes, another file's digest, a missing path and forged bytes refused; workspace bytes unchanged by authoring. Mutation that picks the first repeated snippet fails the core test."}},{"criterion":"RC55-10","outcome":"pass","revision":"e5688006cfe3c9e7e0d0be85fd47383ee558062c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Cycle threads use private_thread_lineage (the provider-independent slice of the landed RFC-BPR-0023 component: header rules, completion, derivation, digests, bounds, four provenance classes); core_private_review passes on the refactored pull-request thread code. No pull-request scope, store, promotion or composition changed. 70 threads/74 revisions persist, reopen and page within 64 entries and 512 KiB (schema 3, migrating 1 and 2)."}},{"criterion":"BPR-1153:boundary","outcome":"pass","revision":"e5688006cfe3c9e7e0d0be85fd47383ee558062c","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"ctest review-cycle, private-review, architecture-boundary and network-isolation-manifest selectors pass on macOS arm64; clang-format and clang-tidy clean on touched units. Linux and Windows runs owed with the ledger commands."}}]
---

Pack alias: **PACK-CYCLE-11** (live `BPR-1153`, allocated at import).

# BPR-1153: Add source-attached private feedback and related locations

## Outcome and bounded work

Implement versioned local feedback threads with immutable primary/related source anchors and range/file/module/cycle subjects. Reuse or extract the narrow private-thread seam from RFC0023 without activating its whole deferred program.

## Acceptance and direct evidence

One multi-file discussion, file/module notes without fake line zero, historical/unchanged anchors, repeated source text, private lineage and paged thread persistence; prove shared records rather than parallel schemas.

Criteria: RC55-01, RC55-02, RC55-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

`core_review_cycle_feedback`, `core_private_review`, `review_cycle_feedback` and `sqlite_review_cycle_storage` pass.

The build is clean under GCC with no warnings. The logs are station records in `build/logs/bpr-station-authoring/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `2631bcf3`, with `f42d4e3a` and the Linux station's `384525fb`,
`build/windows-dev` (MSVC 19.51) builds with no warning. `f42d4e3a` renamed
two locals in `suggestion_editor.cpp` for C4458. `384525fb` fixed the
course's read of an emptied optional in `review_cycle_workbench`, the same
fix this station had made. The authoring rows pass offscreen: the six core rows,
`sqlite_review_cycle_storage`, `review_cycle_{feedback,suggestion,workbench}`,
`qt_suggestion_editor_*`, `qt_review_cycle_view_*` and
`application_review_cycle_desktop_public_path`. The four new uncontained pins
are measured and confirmed. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, authoring".
