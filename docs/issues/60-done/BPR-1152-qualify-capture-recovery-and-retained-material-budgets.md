---
schema: "implement-rfc.card.v1"
id: "BPR-1152"
title: "Qualify capture recovery and retained-material budgets"
status: "Done"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0054"]
depends_on: ["BPR-1149", "BPR-1150"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC53-04", "RC54-04", "RC54-09"], "pack_alias": "PACK-CYCLE-10", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC53-04","outcome":"pass","revision":"4ee2f36d5be749c6b68c3ae3524378fa97e54f00","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_recovery: SIGKILL before and after the commit of the content install, manifest install and revision record (six kills) leaves the reopened cycle exactly as before or holding the whole revision; the earlier revision reads intact; the same operation retried completes once or replays; reclaim leaves no unreferenced candidate. Draft/freeze kills were BPR-1145's. Capture share of RC53-04; closeout BPR-1186 judges the row whole."}},{"criterion":"RC54-04","outcome":"pass","revision":"4ee2f36d5be749c6b68c3ae3524378fa97e54f00","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_recovery and core_review_cycle: explain_capture marks retried, partial or unavailable captures for attention and raises nothing for an ordinary stable capture; no capture claims atomicity (only immutable_source could, and none is produced); retry exhaustion remains unstable with the moving paths (BPR-1149)."}},{"criterion":"RC54-09","outcome":"pass","revision":"4ee2f36d5be749c6b68c3ae3524378fa97e54f00","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_recovery: a corrupted pin reads corrupt with no bytes and is never replaced by the live file; recapturing the same bytes repairs it with bytes that hash to its identity; damage no capture re-verifies stays reported (with BPR-1151's edit/delete/restart rows)."}},{"criterion":"BPR-1152:budgets","outcome":"pass","revision":"4ee2f36d5be749c6b68c3ae3524378fa97e54f00","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_recovery: a file-size limit at the store's size (full disk to SQLite) refuses the capture as a store failure with the cycle unchanged, then succeeds once space returns; a 64 KiB budget refuses a 128 KiB addition with the inventory, reclaim keeps every pin, the refusal repeats rather than evicting, and an explicitly larger budget admits it; a missing scope path is unavailable, not deleted."}},{"criterion":"BPR-1152:boundary","outcome":"pass","revision":"4ee2f36d5be749c6b68c3ae3524378fa97e54f00","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"ctest -R '^core_review_cycle|^review_cycle|^sqlite_review_cycle|^workspace_source$|network_isolation|^architecture_boundar' 23/23 on macOS arm64; tidy and clang-format clean; DOCUMENTATION_OK. Kill and full-disk courses are POSIX-only; Linux and Windows runs owed with the ledger commands."}}]
---

Pack alias: **PACK-CYCLE-10** (live `BPR-1152`, allocated at import).

# BPR-1152: Qualify capture recovery and retained-material budgets

## Outcome and bounded work

Complete source-store install/recovery, pin accounting and bounded retention/capture failure paths. Add stable-capture diagnostics which explain observed quality without an every-round warning wizard.

## Acceptance and direct evidence

Kill/reopen at every candidate/install/catalog boundary, disk full, corrupted blob, protected quota exhaustion, retry exhaustion and missing-scope material. Confirm no lost existing cycle or invented atomicity.

Criteria: RC53-04, RC54-04, RC54-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror`, `build/dev` freshly configured at `467254573`, and the fix `5303ab394`. The source volume is ext4 and `/tmp` is tmpfs.

`review_cycle_recovery`, `core_review_cycle` and `workspace_source` pass. The POSIX courses ran natively: SIGKILL before and after the commit of the content install, manifest install and revision record (six kills), each recovered; the file-size limit at the store's size refused the capture and lost nothing; a corrupted blob was reported, and a verified recapture repaired it.

The logs are station records in `build/logs/bpr-station-snapshots/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `46725457`, `build/windows-dev` (MSVC 19.51) builds with no warning and
no source change, including the workspace adapter's Win32 branch.
`workspace_source`, `review_cycle_capture`, `review_cycle_comparison`,
`review_cycle_scale`, `review_cycle_recovery`, the six `core_review_cycle`
rows and `sqlite_review_cycle_storage` pass. Contained, the five workspace
rows each fail before their first course completes, so their
`WINDOWS_UNCONTAINED` pins are confirmed. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, snapshots".
