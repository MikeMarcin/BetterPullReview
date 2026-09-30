---
schema: "implement-rfc.card.v1"
id: "BPR-1148"
title: "Bind local folders, files and Git workspaces safely"
status: "Done"
rfcs: ["RFC-BPR-0054"]
depends_on: ["BPR-1146", "BPR-1147"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-01", "RC54-03", "RC54-10"], "pack_alias": "PACK-CYCLE-06", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC54-01","outcome":"pass","revision":"3fdd425f919a7b94faa069a26d7682cc2bdc0765","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 dev preset, network-isolated workspace_source: local workspace adapter opens a plain folder, a single file (identified by its directory; an atomic-save replacement keeps the root), a Git checkout with no commit and only uncommitted content, and a linked worktree made by the build's Git whose repository identity equals the main checkout's and whose worktree identity differs. No account, remote or network. Linux and Windows native runs owed with the commands in the ledger."}},{"criterion":"RC54-03","outcome":"pass","revision":"3fdd425f919a7b94faa069a26d7682cc2bdc0765","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 workspace_source: '..', absolute, drive, empty/dot segment and .git paths refused as unsafe_path before any open; directory link, file link and a directory swapped for a link after it was read refused as escape (openat/O_NOFOLLOW per component, same-device check); link target kept as data; a root renamed away and replaced at its location reported replaced while the held session keeps reading the original root. Windows reparse-point and held-ancestor branch compiled and run only on the Windows station (owed)."}},{"criterion":"RC54-10","outcome":"pass","revision":"3fdd425f919a7b94faa069a26d7682cc2bdc0765","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core_review_cycle_workspace and workspace_source: same-named checkouts with identical bytes differ by root identity and verify as replaced; rebind needs confirmation, and over captured material also a recorded material check; a moved root keeps identity but still needs a confirmed rebind; captures naming a stale binding version are refused in the core and through the SQLite store (sqlite_review_cycle_storage); imported projects resolve only through an explicit mapping; bindings reload exactly and schema 1 migrates in place."}},{"criterion":"BPR-1148:collisions","outcome":"pass","revision":"3fdd425f919a7b94faa069a26d7682cc2bdc0765","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core_review_cycle_workspace flags case-fold and normalization pairs (Latin, Greek, Cyrillic from Unicode 16 tables) without merging; native row on this APFS volume (case- and normalization-insensitive) verified byte-preserved names only; the two-file native rows are owed on a case-sensitive Linux volume."}},{"criterion":"BPR-1148:boundary","outcome":"pass","revision":"3fdd425f919a7b94faa069a26d7682cc2bdc0765","evidence":{"kind":"command","detail":"ctest -R '^core_review_cycle|^workspace_source$|^sqlite_review_cycle|^review_cycle_dependency|^architecture_boundar|^conditional_branch' passes on macOS arm64; core_review_cycle* pass under core-only; clang-format and clang-tidy --warnings-as-errors clean on touched translation units; verify_documentation DOCUMENTATION_OK."}}]
---

Pack alias: **PACK-CYCLE-06** (live `BPR-1148`, allocated at import).

# BPR-1148: Bind local folders, files and Git workspaces safely

## Outcome and bounded work

Implement deliberate root selection and logical identity for plain folders/files, Git roots and linked worktrees. Resolve descendants with native no-follow/identity checks. Keep root rebind explicit and map imported logical projects without trusting archive paths.

## Acceptance and direct evidence

Disposable roots for symlink/junction escape, replaced root, linked worktree, two same-named checkouts, moved root, nonrepository file and case/normalization collisions. Record each native platform result separately.

Criteria: RC54-01, RC54-03, RC54-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror`, `build/dev` freshly configured at `467254573`, and the fix `5303ab394`. The source volume is ext4 and `/tmp` is tmpfs.

`workspace_source`, `core_review_cycle_workspace`, `core_review_cycle` and `sqlite_review_cycle_storage` pass. The native collision row that was owed here ran on a case- and normalization-sensitive volume and printed `case-sensitive, normalization-sensitive volume; 2 collision(s) flagged`: both the case-fold pair and the normalization pair were written as distinct files and flagged. The link-escape, dirty/linked-worktree and root-replacement rows also ran natively.

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
