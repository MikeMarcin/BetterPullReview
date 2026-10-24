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

Initial mandate r1 selected pinned-macos-integration (BPR-1758 and BPR-1759), which remains incomplete. Mandate r2 authorizes independent Windows work without waiving any Mac criterion; Windows native allocator acceptance is recorded below. Mandate r3 independently accepts the Linux allocator milestone without waiving Mac criteria. No release acceptance is presumed.

## Closeout checklist

| Criterion | Direct check / evidence | Result | Producing commit |
|---|---|---|---|
| **AC01:** Configure obtains only the pinned v3.5.3 source and licence bytes; offline, substitution and target-collision controls reject wrong inputs. | Provider pin/offline/substitution/collision/extraction controls; [ledger](../../verification/RFC-BPR-0093.md) | Pass | `81ee1b4ef` |
| **AC02:** Every selected shipped BPR executable has the override in normal and distribution graphs; disabled optional families create no phantom requirement, a core-only graph remains allocator-independent, non-distribution baseline selection works, and distribution refuses it. | All product-target attachment, actual minimal distribution/core/baseline graphs and running baseline CLI; ledger | Pass | `81ee1b4ef` |
| **AC03:** Native Apple Silicon and Intel macOS courses demonstrate C and C++ interception, representative Qt/loaded-library interception and safe cross-module allocation/free in the desktop and every selected helper/CLI, including a signed installed candidate. | Seven native developer rows plus clean-worktree local bundle, relocated offscreen assessment and full package audit; ledger | Partial: native Intel and signed installed required | `aadf2d91d` |
| **AC04:** A native Windows 11 x64 course demonstrates the same behavior for the desktop and every selected helper/CLI with the actual CRT and packaged DLL closure; startup, shutdown and cross-module frees remain sound. | Native Windows four-product probes, external installed/CLI pointer and Qt-free observations, actual runtime audit/controls; [ledger](../../verification/RFC-BPR-0093.md#windows-milestone-result--mandate-r2) | Pass: Windows native allocator scope | `0ccf31bc4`, `c5693fd05` |
| **AC05:** An Ubuntu 26.04 x64 course on the selected WSL2/WSLg station demonstrates the same behavior for the desktop and every selected helper/CLI with the installed ELF closure; external child processes do not inherit allocator injection. | Native four-product probes; actual installed GDB/Qt/cross-free/OOM, CLI/MCP/XCB public paths, clean child traces and Runtime audit/controls; ledger | Pass: Linux native allocator scope | `0c40cca40` |
| **AC06:** Focused composed workflows and an allocator-boundary stress course preserve ordinary behavior and ownership; sanitizer/fuzzer builds use a compatible allocator path and allocation failure does not gain abort mode. | Boundary stress/failure probes, 17 composed checks and instrumentation configure controls | Partial: Windows and Linux boundary/composition passed; full sanitizer/fuzzer course pending | `81ee1b4ef`, `aadf2d91d` |
| **AC07:** Installed macOS, Windows and Ubuntu artifacts, standalone CLI where selected, notices, SPDX and the separate versioned corresponding-source asset agree on the pinned source and actual runtime files; existing release verification rejects a missing or extra mimalloc artifact. | Mac local bundle/source-sidecar and Windows Runtime/source audits with missing/extra allocator controls; ledger | Partial: Mac ad-hoc, Windows and Ubuntu Runtime audits passed; final CLI/installer/all-platform closure and trust pending | `aadf2d91d` |
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

## Latest checkpoint — Windows milestone boundary

Run `mimalloc-20261024`, segment `windows-m2-native`, mandate `r2`; assigned leaf
`/root/mimalloc_windows`, source/tracker main at
`/Users/jaeger/Documents/GitHub/BetterPullReview`. The r2 scheduling amendment
(`622368021`) changes only Windows' dependency on full Mac acceptance;
`check-amendment` and the documentation gate passed before the claim. Every
authored outcome, exit and native requirement is preserved.

- **Result/candidates:** Windows milestone accepted; BPR-1760 is complete for
  native allocator behavior. Default Runtime and Debug candidate
  `0ccf31bc4a668a9cc71be93afb6664a0bc70225b`; tests-disabled CLI assessment
  `c5693fd05e9a673381cae5aa76c355db33e39182`. The exact native audit uses the
  final verifier while retaining those product/source identities. The ledger
  records hashes and native scope. M1 and this RFC remain incomplete.
- **Changed paths:** `cmake/BprMimallocProvider.cmake`; `src/app/CMakeLists.txt`;
  `tests/mimalloc/` and allocator-provider tests; Windows runtime/signing-receipt
  inventories in `packaging/`; PE/package/isolation verifiers and their controls;
  architecture/build/verification docs, the scheduling plan/view and live cards.
  Product source has no allocator API in core/host/adapter contracts.
- **Evidence:** seven native allocator rows; no-relink refresh regression;
  installed CDB C/C++/Qt allocation/free and disabled controls; shipped installed
  self-test; installed CLI Cycle course; Debug Cycle/render/MCP composition;
  full runtime audit (31 files/23 binaries/511 compile units), source binding
  and three public omission/extra controls. Focused format/tidy/Python/package/
  documentation checks pass. Existing isolation-manifest failures remain explicit
  in the ledger. There is no full-suite or release-qualification claim.
- **Ownership:** all own commands finished; no task-image process or review
  server remains. Native source/build/runtime/maps/evidence remain under
  `C:/code/bpr-rfc0093-866ec50e723c`. Its Git object base is shared read-only with
  the preserved station checkout; physical source/builds are inside this root.
  No other checkout, owner desktop, signing credential or remote repository was
  mutated. Unrelated MASK/NEXT_ID/BPR-1766 edits on main were excluded.
- **Workflow binding:** original recipe selection is retained under
  `build/mimalloc-windows/selection.json`; the read current revision-2 recipe
  (fingerprint `6a3e2443…`) is in `selection-current.json`. External attendance
  text changed during setup; binding was renewed after native configure stopped.
  Final machine checkpoint/observation and command inventory live in the same
  runtime directory, with this card as the durable handoff.
- **Pending/next:** native Intel and signed installed Mac AC03, Ubuntu AC05,
  final all-platform/standalone CLI/MSI AC07, measurement AC08 and final AC09
  reconciliation remain. Existing BPR-0967/native/vault/security gaps are unchanged.
  Stop at the Windows boundary. The coordinator must reconcile Linux's unchanged
  authored Mac prerequisite before selecting that independent course; no further
  milestone is selected by this leaf. No attended session/cursor/URL exists for
  this completed objective; the earlier Mac candidate stays retained.

## Active checkpoint — Linux mandate r3

Run `mimalloc-20261024`, segment `linux-m3`, mandate `r3`; sole implementer
`/root/mimalloc_linux`, source/tracker main at
`/Users/jaeger/Documents/GitHub/BetterPullReview`. Scheduling amendment
`758b17a1b` removes only Linux's full-Mac prerequisite, with checked unchanged
outcome/exit/work. BPR-1761 is claimed at `af7c84025`; BPR-1758 remains its
accepted provider prerequisite. Mac Intel/signed-installed requirements and
retained candidate remain pending.

Isolated Ubuntu 26.04 x64 WSL2/WSLg root:
`/home/mmarcin/bpr-rfc0093-linux-dfzpprt6`, with source, cache, builds and tools
inside it. The original `/home/mmarcin/BetterPullReview` stays clean and is a
read-only source of Git objects/pinned Qt. Native source `d105f0703` passes the
shortest supervisor plus missing-override course after correcting the real
Linux throwing-new abort: C++ compilation of upstream mimalloc preserves
`std::bad_alloc`. Its full Debug build is active. Current main also contains
`f6adc378a`, the Linux standalone CLI xdiff/loader closure prerequisite, not
yet transferred while that build runs. BPR-1761 and AC05 remain unaccepted.

`build/mimalloc-linux/` holds selection/checkpoint, amendment receipt, native
root, helper scripts and focused logs. Native logs stay under the isolated
root. Next: finish Debug build, exercise all product ownership probes, then
transfer the committed CLI prerequisite while native writers are stopped;
build/install tests-disabled Runtime and TourCLI and observe actual ELF
ownership, child cleanliness and composed public paths. Native tooling was
unpacked only beneath this task root; no system package install, personal data,
signing, push or publication. No review session/server exists. Main's unrelated
MASK/NEXT_ID/BPR-1766 edits are preserved and excluded from commits.

## Latest checkpoint — Linux milestone boundary

Run `mimalloc-20261024`, segment `linux-m3`, mandate `r3`; sole leaf
`/root/mimalloc_linux`, source/tracker main at
`/Users/jaeger/Documents/GitHub/BetterPullReview`. Amendment `758b17a1b`
removes only Linux's full-Mac scheduling prerequisite. Authored outcome/work/
exit and all Mac criteria remain unchanged; BPR-1758 stays the accepted card
prerequisite. Strategy remains `coordinator-implementer` revision 2.

- **Result/candidate:** Linux milestone accepted; BPR-1761 completes AC05 and
  Linux AC06 at `0c40cca40e5d93205d83fd65f76af029ac937347`. Runtime and actual
  TourCLI installs, producer identities and all limits are bound by the
  [ledger](../../verification/RFC-BPR-0093.md#linux-milestone-result--mandate-r3).
  Mac AC03, final AC07/AC08/AC09 and RFC completion remain open.
- **Changed paths:** allocator provider; Linux probe/platform enumeration;
  Runtime and TourCLI SONAME/xdiff/RPATH composition; private library inventory
  and package controls; architecture/build/package/verification docs, plan/view
  and live cards. Existing GCC blockers received a local tray-name rename and
  a ZIP payload-storage rewrite with preserved behavior. No allocator API enters
  core/host/adapter contracts.
- **Verification:** final native 16/16, local core-only build and four ZIP/
  architecture rows, installed GDB ownership/free/OOM/origins in five images,
  installed CLI/MCP/XCB self-test, real Git/gh/agent child traces and injected
  negative control. Full Runtime audit: 27 files, 20 binaries, 528 compile
  units, MIT/SPDX/separate source; omission/extra controls refuse. Focused
  package/security/source/native-release/documentation/format/tidy pass with
  explicit platform skips. Initial true failures and harness corrections remain
  recorded. No full-suite, performance, trust or release-qualification claim.
- **Ownership/commands:** no own command or native task image remains. Native
  source/build/payloads/source ZIP/tools live under
  `/home/mmarcin/bpr-rfc0093-linux-dfzpprt6`. The original station stays clean
  and was read only; no owner desktop, personal data, system package installation,
  signing credential or remote repository was touched. Private Xvfb/mount
  namespaces exited and its auth file was removed. Original candidate files
  retain every recorded hash. Main's unrelated MASK/NEXT_ID/BPR-1766 edits are
  excluded and preserved. Negative package copies were removed.
- **Durable runtime:** `build/mimalloc-linux/` contains selection, amendment
  receipt, final machine checkpoint/observation, local logs and copied native
  evidence under `native/`; complete raw evidence remains in the native root.
  Preserve those candidates. No attended session/cursor/URL/server exists for
  this programmatic milestone; the prior Mac candidate stays retained.
- **Next:** return at the Linux boundary. The coordinator owns subsequent
  scheduling. Distribution/measurement cards still depend on pending Mac work;
  native Intel/signed-installed acceptance is not waived. Existing BPR-0967,
  native/vault/crash/security gaps and release/source/legal obligations remain.


## Active checkpoint — distribution mandate r4

Sole leaf `/root/mimalloc_distribution` owns `distribution-and-measurement`.
The [ledger](../../verification/RFC-BPR-0093.md#distribution-checkpoint--mandate-r4)
binds scheduling/claims, source commits, actual DEB/MSI audits, instrumentation
and matched Linux/Apple-Silicon Release outcomes. Additional allocator memory
failures are recorded as regressions; no limit changed and no BPR-0967 fix is
claimed. BPR-1762/1763 remain In Progress while native standalone CLI validation
and Windows comparisons run. All Mac final acceptance and RFC closeout remain
pending. Runtime scripts/logs are `build/mimalloc-distribution/`; the new native
roots and current command ownership are recorded in the ledger. No review
session/server, signing, publication, push or owner desktop operation exists.
