---
schema: "implement-rfc.card.v1"
id: "BPR-1187"
title: "Verify and close RFC-BPR-0054"
status: "In Progress"
rfcs: ["RFC-BPR-0054"]
depends_on: ["BPR-1186", "BPR-1284"]
closeout_for: ["RFC-BPR-0054"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "closeout", "acceptance": ["RC54-01", "RC54-02", "RC54-03", "RC54-04", "RC54-05", "RC54-06", "RC54-07", "RC54-08", "RC54-09", "RC54-10"], "pack_alias": "PACK-CYCLE-45", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-45** (live `BPR-1187`, allocated at import).

# BPR-1187: Verify and close RFC-BPR-0054

## Outcome and bounded work

Reconcile every criterion of RFC-BPR-0054 against its producing revision and direct evidence. Verify canonical predecessor closure and the imported ownership/remap. Leave unmet native/live/user gates open; do not relax criteria, recycle IDs or alter historical predecessor acceptance to close this RFC.

## Acceptance and direct evidence

Run the live documentation/board gates and record criterion/check/result/revision/environment. Close only this RFC through closeout_for; a complete pack validator or another RFC result is not substitute evidence.

Criteria: RC54-01, RC54-02, RC54-03, RC54-04, RC54-05, RC54-06, RC54-07, RC54-08, RC54-09, RC54-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

RFC base: `8466534f96f254d88fed412a587188b7b45f2580`.

All owned acceptance criteria remain **Not run** at intake.
## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| **RC54-01:** Open Git/linked-worktree/dirty checkout, plain folder and single-file scopes without network/authentication or a prerequisite commit. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: plain folder, single file, Git checkout with no commit, dirty checkout and linked worktree, network-isolated; macOS, Linux, Windows | BPR-1148 `a28ddbb9b` |
| **RC54-02:** Capture changes neither source bytes, user index nor refs and runs no hooks, filters, textconv, lazy fetch or repository scripts. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: hostile hooks, filters, textconv, fsmonitor and promisor never run; tree including `.git` byte-identical; all three; BPR-1181 hostile checkout on macOS and Linux | BPR-1149, BPR-1181 |
| **RC54-03:** Reject root replacement, symlink/junction escape and unsafe descendant paths under native filesystem semantics. | Focused production-path checks and applicable native or service course at closeout | **Open.** Unsafe descendant paths, root replacement (held root pinned on Windows; same name, same bytes, other checkout `replaced` on all three) and symbolic-link escapes pass on all three. No run has made a junction: the Windows junction escape is unobserved, owed to BPR-1284 | BPR-1148 `a28ddbb9b`, BPR-1183, BPR-1244; BPR-1284 owed |
| **RC54-04:** Detected instability retries finitely; capture-quality metadata never asserts global atomicity from ordinary copying or two matching passes. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: one change retried once, a never-stopping writer exhausts three retries as `unstable`, only `observed_stable` produced, settled-after-retry flagged; all three | BPR-1149, BPR-1152 `4ee2f36d5` |
| **RC54-05:** Scope exclusions, sparse/missing content, explicit deletions and new scope versions remain distinguishable in manifests/comparisons. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: scope versions, exclusions with reasons, sparse entries unread, tracked deletion versus unknown, `missing_scope`; all three | BPR-1149 |
| **RC54-06:** CRLF/LF/BOM/no-final-newline, raw hashes, encoding offsets, file mode and path identities are preserved without lossy normalization. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: byte corpus read back exactly with raw SHA-256; CRLF, BOM, no final newline kept; modes 100755/100644 on POSIX, none invented on Windows; all three | BPR-1150 |
| **RC54-07:** Binary, unsupported encoding, LFS/filter-managed, symlink/gitlink and platform-collision cases remain visible with accurate capability limits. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: binary, unsupported encoding, LFS pointer and filter-managed, gitlink boundary, FIFO unsupported, links as target bytes (Windows since BPR-1244); collision pairs written and flagged on Linux | BPR-1150, BPR-1244 |
| **RC54-08:** Large enumeration/capture/diff access is paged/cancellable and measured without UI-thread hashing or unbounded core state. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: 256-path resumable pages, cancellation, effect-lane reads; 100,001-file Release reports on all three, `failures` empty; capture cancellable within 11 ms | BPR-1151, BPR-1184 `397908dec` |
| **RC54-09:** Retained material resolves the exact captured bytes after external edits, deletion or restart; corruption/missing pins are not replaced by live files. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: exact bytes after edit, deletion and restart; corrupt and missing pins report no bytes, never the live file; verified recapture repairs; all three | BPR-1151, BPR-1152 `4ee2f36d5` |
| **RC54-10:** Explicit workspace rebind/import mapping prevents the same path/alias in another checkout from silently becoming the review subject. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: same-name checkout `replaced`; core refuses unconfirmed rebind and requires the material check; imports resolve only by explicit mapping; the product refuses another folder at the location. Limit: no product rebind ([[BPR-1279]]) | BPR-1148 `a28ddbb9b`, BPR-1149, BPR-1162, BPR-1183 |

## Closeout reconciliation (2026-10-04): not closable yet

BPR-1186 closed RFC-BPR-0053 first, so this card's dependency was met, and the
reconciliation ran before promotion. Nine criteria pass on their cards'
recorded evidence, with runs on macOS arm64, `bpr-main-linux` and
`bpr-main-win`; the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1187-rfc-bpr-0054-reconciled-not-closed-2026-10-04)
judges each row. RC54-03 is open: it names junction escapes "under native
filesystem semantics", and no run has made a junction. The Win32 adapter
refuses any reparse point by its attribute, so a junction should take the
branch the symbolic links took. That is a reading of the code, and the pack's
verification contract says a related code path is not an observed pass. No
RC54 row asks for the owner's judgment.

[[BPR-1284]] (Ready, `platforms: ["win"]`) owes the native observation, and
this card now depends on it, so the card stays in Backlog. When BPR-1284 is
Done, promote this card, add its result to RC54-03, rerun the documentation
gate, and close the RFC. The other nine rows need no rerun unless their inputs
change.
