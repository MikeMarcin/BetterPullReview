---
schema: "implement-rfc.card.v1"
id: "BPR-1168"
title: "Share cycle ownership across desktop and CLI"
status: "Done"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1145", "BPR-1167"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-05", "RC58-06"], "pack_alias": "PACK-CYCLE-26", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC58-05","outcome":"pass","revision":"7e5a093cc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64: application_profile_cycle_handoff_public_path runs a real profile desktop owner and the shipped bpr as separate processes: CLI-alone open/note; with the desktop open the lease is held and show, note, working-folder lookup, review, freeze and export are forwarded over the owner channel (protocol 3) and land in the owner's store, its window showing them; four concurrent callers get their own answers; after the owner is killed (socket left stale) the next command runs standalone, a replacement owner is reached in turn; a protocol-2 stand-in is answered unsupported_version and receives no byte; an owner without a channel is capability_unavailable and nothing lands. sqlite_tour_owner_channel checks the bounded cycle codec (registry options only) and, on Windows, the named pipe. Windows pipe compiled for syntax only (mingw); MSVC build and Windows/Linux runs owed, not gating; second-OS-user refusal not exercised."}},{"criterion":"RC58-06","outcome":"pass","revision":"7e5a093cc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Same course: the window's unsaved note survives terminal writes with focus kept and is saved beside them; terminal draft edits reach the window and stale ones are stale_revision naming the current revision; a window reversal reaches the terminal (no undo in Review Cycles: reversal under the revision read); a typed-not-entered goal is saved before a terminal freeze, which is stale_revision, and review reads it; IME composition makes review/freeze draft_busy while a note is served, then the committed text is frozen into F1; a repeated --operation-id returns the first answer and applies once, another request under it is conflict. review_cycle_workbench: a window's note kept beside another interface's writes, a refused goal kept unsaved and saved again, reload on change. Four negative controls fail (guard off, no refresh, no remembered answers, no note retry)."}},{"criterion":"BPR-1168:boundary","outcome":"pass","revision":"7e5a093cc","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"80 of 80 rows in build/dev-tour-cli incl. cycle and tour CLI, both handoff courses, owner channel, activation, admission, desktop, architecture_boundaries and test_network_isolation_manifest; round-trip contract resealed and passing; core-only 23 of 23 core_review_cycle rows; tests-disabled installed bpr passes cycle_cli_tests; clang-format clean; clang-tidy clean on changed units (only existing review_tour_codec.hpp findings); DOCUMENTATION_OK. One unexplained 63 s failure of the handoff course under -j6 load in nine runs is recorded."}}]
---

Pack alias: **PACK-CYCLE-26** (live `BPR-1168`, allocated at import).

# BPR-1168: Share cycle ownership across desktop and CLI

## Outcome and bounded work

Extend the tour owner service with finite cycle operations, exact profile/peer identity, operation deduplication, expected revisions and safe dirty-editor flush. Implement and qualify missing native transport rather than opening two writers or building an HTTP server.

## Acceptance and direct evidence

CLI-alone and desktop-open exchange, owner death/replacement, stale endpoint, wrong user, protocol mismatch, duplicate request, IME busy, out-of-order reply and cross-interface undo. Record platform-specific evidence.

Criteria: RC58-05, RC58-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-02

At `7e5a093cc`. While another BetterPullReview process owns a profile, every
`bpr cycle` command is forwarded over the owner command channel (protocol 3,
one `cycle` operation) and run by the owner on its store with the same command
code; the core's finite registry decides what may travel and whether a window's
unsaved or composing draft text holds a `review` or `freeze`. `--operation-id`
makes repeats return, never reapply. Windows gets the channel as an
owner-private named pipe. Owned paths, compatibility decisions (the RFC-0045
channel extended; no undo in Review Cycles, so reversal; the goal as the
draft's unsaved text), the 80-row pass, the real two-process course, four
negative controls and the station commands are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under "BPR-1168:
one owner across the desktop and `bpr cycle`"; usage is in
[the developer page](../../development/cycle-cli.md).

Earlier on the way: BPR-1167's eight `src/app/src` files were registered in the
round-trip inventory (`4b5589378`), which had been red since `2a79d08eb`.

## Owed platform runs

Not gating. The Windows pipe has been compiled only for syntax (mingw-w64);
its MSVC build and runs, and Linux runs, are owed with the commands in the
ledger. `application_profile_cycle_handoff_public_path` and
`sqlite_tour_owner_channel` are registered contained; a measured refusal inside
the container is pinned `WINDOWS_UNCONTAINED` with its line, not guessed.

## Linux: bpr-main-linux, 2026-10-02

At `1eab89550` (includes `7e5a093cc`), on WSL2 (i9-10900KF, 20 logical processors)
with GCC 15.2 and `build/dev-tour-cli` (`BPR_BUILD_TOUR_CLI=ON`). The
configure and build exit 0 with 0 `warning:` lines. The requested `ctest -j4`
selection passes **10 of 10** in 33.27 s:

| Row | Time |
|---|---|
| `application_profile_cycle_handoff_public_path` | 6.85 s |
| `application_profile_tour_handoff_public_path` | 2.96 s |
| `cycle_cli_public_path` | 7.84 s |
| `sqlite_tour_owner_channel` | 5.76 s |
| `core_review_cycle_command` | 0.00 s |
| `review_cycle_workbench` | 33.25 s |
| `application_review_cycle_desktop_public_path` | 2.59 s |
| `application_review_round_trip_composition_contract` | 5.03 s |
| `architecture_boundaries` | 23.07 s |
| `test_network_isolation_manifest` | 1.64 s |

The Mac saw the new handoff course flake once in 9 runs, so it was repeated
here. These are additional runs, not reruns of a failure; none failed.

- `--repeat until-fail:15`, alone: 15 of 15 pass.
- `--repeat until-fail:10` at `-j6`, beside `review_cycle_workbench`,
  `architecture_boundaries`, the tour handoff, `cycle_cli_public_path` and
  `sqlite_tour_owner_channel`: 10 of 10 pass. Load average ran from 4.5 down
  to 2.7.

So no flake reproduced on Linux in 25 runs. No remote mutation.
