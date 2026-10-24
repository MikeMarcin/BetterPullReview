---
schema: "implement-rfc.card.v1"
id: "BPR-1764"
title: "Verify and close RFC-BPR-0093"
status: "Backlog"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1759", "BPR-1760", "BPR-1761", "BPR-1762", "BPR-1763"]
closeout_for: ["RFC-BPR-0093"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# BPR-1764: Verify and close RFC-BPR-0093

## Scope and acceptance

Reconcile AC01–AC09 against exact native, public-path, package/source and
measurement evidence. Update architecture, build, package and operator
documentation (AC09), run the required documentation gate and focused
integration check, and record remaining platform/release limits. Do not close
with link-only or developer-build evidence in place of native interception.

## Implementation checkpoint

RFC base: `e82537ad15b32871a8375b6143abe08e16a68b87`.

Mandate r1: implement the supplied pack, including the owner-confirmed process-wide scope. Current milestone: pinned-macos-integration (BPR-1758 and BPR-1759). No native or package acceptance is presumed.

## Closeout checklist

| Criterion | Direct check / evidence | Result | Producing commit |
|---|---|---|---|
| **AC01:** Configure obtains only the pinned v3.5.3 source and licence bytes; offline, substitution and target-collision controls reject wrong inputs. | Provider pin/offline/substitution/collision/extraction controls; [ledger](../../verification/RFC-BPR-0093.md) | Pass | `81ee1b4ef` |
| **AC02:** Every selected shipped BPR executable has the override in normal and distribution graphs; disabled optional families create no phantom requirement, a core-only graph remains allocator-independent, non-distribution baseline selection works, and distribution refuses it. | All product-target attachment, actual minimal distribution/core/baseline graphs and running baseline CLI; ledger | Pass | `81ee1b4ef` |
| **AC03:** Native Apple Silicon and Intel macOS courses demonstrate C and C++ interception, representative Qt/loaded-library interception and safe cross-module allocation/free in the desktop and every selected helper/CLI, including a signed installed candidate. | Seven native developer rows plus clean-worktree local bundle, relocated offscreen assessment and full package audit; ledger | Partial: native Intel and signed installed required | `aadf2d91d` |
| **AC04:** A native Windows 11 x64 course demonstrates the same behavior for the desktop and every selected helper/CLI with the actual CRT and packaged DLL closure; startup, shutdown and cross-module frees remain sound. | Native/public-path/package course required | Pending | — |
| **AC05:** An Ubuntu 26.04 x64 course on the selected WSL2/WSLg station demonstrates the same behavior for the desktop and every selected helper/CLI with the installed ELF closure; external child processes do not inherit allocator injection. | Native/public-path/package course required | Pending | — |
| **AC06:** Focused composed workflows and an allocator-boundary stress course preserve ordinary behavior and ownership; sanitizer/fuzzer builds use a compatible allocator path and allocation failure does not gain abort mode. | Boundary stress/failure probes, 17 composed checks and instrumentation configure controls | Partial: full sanitizer/fuzzer and other native courses pending | `81ee1b4ef`, `aadf2d91d` |
| **AC07:** Installed macOS, Windows and Ubuntu artifacts, standalone CLI where selected, notices, SPDX and the separate versioned corresponding-source asset agree on the pinned source and actual runtime files; existing release verification rejects a missing or extra mimalloc artifact. | Mac local bundle/source-sidecar audits and missing/extra allocator controls; ledger | Partial: Mac ad-hoc local artifact; all-platform final closure and trust pending | `aadf2d91d` |
| **AC08:** Same-workload Release comparisons and the unchanged large-review memory course record candidate identity, native platform, measurements and failures without attributing pre-existing BPR-0967 limits to mimalloc. | Native/public-path/package course required | Pending | — |
| **AC09:** Architecture, build, package and operator documentation state the actual override scope and limits; focused public-path and documentation checks pass, with all native and release claims tied to their evidence. | Architecture/build documentation, documentation gate/tests and focused integration | Partial: final operator/package/native evidence reconciliation pending | `aadf2d91d` |

## Durable handoff — pinned-macos-integration

Run `mimalloc-20261024`, segment `macos-m1-candidate`, mandate `r1`;
recipe `coordinator-implementer` revision 2. Assigned leaf
`/root/mimalloc_m1`, source/tracker root
`/Users/jaeger/Documents/GitHub/BetterPullReview`, branch `main`.
The coordinator authorized the small macOS candidate prerequisite amendment;
the owner's process-wide scope and every authored exit remain unchanged.

- **Result:** BPR-1758 Done (`086feda22`), BPR-1759 blocked pending native
  Intel/signed installed candidate. This milestone and RFC are incomplete.
- **Candidates/commits:** import `5829f354e`, provider `b2c4f7f1d`, corrected
  probes `81ee1b4ef`, Mac package prerequisites `aadf2d91d`. Latest product
  source is `aadf2d91d`; later tracker-only commits reuse unchanged evidence.
- **Changed paths:** root CMake; `cmake/BprMimallocProvider.cmake` and
  `BprDependencies.cmake`; MCP entrypoint documentation; `tests/mimalloc/`,
  `tests/scripts/mimalloc_provider_tests.py` and package tests;
  `packaging/Install.cmake`, dependency/runtime inventories, `tools/release.sh`,
  `third_party/mimalloc/`; the imported pack/plan, cards, RFC/index, architecture,
  build guide and verification ledger. The imported packet remains immutable.
- **Commands/evidence:** `build/mimalloc-m1/` retains selection/checkpoint JSON,
  configure/build/native logs, `integration.log` (17/17), `tidy.log` (corrected
  finding) and `tidy-fixed.log`, package/source tests, baseline graph and run,
  final documentation results and the failed prepare preflight. All commands
  owned by this leaf are finished; no review server or task worktree exists.
  The worktree inventory is unchanged. No owner desktop was used.
- **Other writer:** unrelated NEXT_ID and BPR-1765 reservation changes are
  preserved, excluded from our commits and block clean-source preparation.
- **Next action:** reconcile that writer's allocation, then resume this same
  milestone with clean source and retry `tools/release.sh --version 0.3.2
  --prepare-only --skip-tests --output-dir build/mimalloc-m1/local-candidate`.
  Complete the installed runtime course and obtain the explicitly authorized
  signed-candidate and native Intel observations. An ad-hoc local candidate is
  insufficient. The successor milestones retain their authored prerequisites.
- **Review handoff:** no attended session is ready: no installed candidate was
  produced and native evidence is outstanding. No authenticated URL is stored.
  Final AC07, performance/memory AC08 and existing BPR-0967 limits remain open;
  no release or security acceptance is claimed.

## Latest checkpoint — clean worktree

Supersedes the dirty-source stopping point above for the task-owned branch.
Product source `eb28c0187a49143f5470e4400c2f1b3d0cb03de0` produced a local universal tests-disabled bundle,
ZIP, SPDX and separate source ZIP under
`/Users/jaeger/.codex/worktrees/mimalloc-macos-candidate/BetterPullReview/build/mimalloc-m1/local-candidate/`.
The [ledger](../../verification/RFC-BPR-0093.md) binds all artifact hashes,
commands and acceptance limits. Run `mimalloc-20261024`, segment
`macos-m1-clean-worktree`, mandate `r1`; same authored milestone and exits.

- **Passed:** credential-free prepare-only course; relocated offscreen assessment
  with original bundle unchanged; full package audit (52 files, 19 binaries,
  519 first-party units); exact source-sidecar audit; missing/extra allocator
  refusal through the public package auditor. Scoped LLVM 22 resolves a broken
  global LLVM 23 tool dependency. No product source fix was needed.
- **Still pending:** native Intel ownership, actual authorized signed-installed
  acceptance and delivered native Cocoa course. BPR-1759 remains Blocked and
  the milestone remains unaccepted. Final AC07 and AC08 are not closed.
- **Ownership:** retained branch `codex/mimalloc-macos-candidate`, worktree root
  `/Users/jaeger/.codex/worktrees/mimalloc-macos-candidate/BetterPullReview`. No other writer owns this branch. All own commands finished;
  no review server/session exists. Main's unrelated NEXT_ID and reservation
  are untouched. No push, publication, Developer ID or Keychain operation.
- **Continuation:** preserve the worktree/build/candidate and runtime checkpoint
  at `build/mimalloc-m1/checkpoint-final.json` plus `observation-final.json`.
  Resume the same milestone when native Intel and signing authority/courses
  can supply the remaining evidence. Integrate this branch's checkpoint only
  after reconciling main's other writer. The local ad-hoc seal grants no trust.
