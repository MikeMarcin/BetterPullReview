---
schema: "implement-rfc.card.v1"
id: "BPR-1764"
title: "Verify and close RFC-BPR-0093"
status: "In Progress"
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

## Current signed continuation — authorized round r9

The [r9 ledger](../../verification/RFC-BPR-0093.md#signed-local-macos-acceptance--authorized-round-r9)
now binds actual Developer-ID/notarized app and DMG, native trust, universal
static proof, private relocated installed closure, source/notices/SPDX, negative
allocator controls and headless public rendering at producer `0f42035a1`.
Production/provider/package paths remain identical to `df6e8be48`; native pointer,
Windows/Ubuntu, CLI, instrumented and measurement records keep their original
producers. No version change or publication; skipped broad suites are a local
validation limitation, and all resource/security/legal gaps remain explicit.

The delivered Cocoa judgment is ready in fresh session
`3a0425eb3756874560105a4cff2b0b64` at external state
`/Users/jaeger/.codex/attended-review/bpr-mimalloc-signed-r9-k9f02pev`, candidate
`BPR-1759-signed-cocoa-r9`, consumed cursor 3. BPR-1759/BPR-1762 remain pending
until actual intake and next-round engineering reconciliation. This closeout
card stays Backlog and RFC-BPR-0093 stays In Progress.

## Historical mandate r5

The owner explicitly answered “Amend Intel criterion” on 2026-10-24: verified
universal slices/static x86_64 proof plus native arm64 observations replace the
native Intel run requirement. No Intel execution is claimed. BPR-1759 and
BPR-1762 remain Blocked on actual Developer-ID signed-installed and delivered
native Cocoa acceptance; this card remains Backlog and the RFC In Progress.
The [r5 ledger](../../verification/RFC-BPR-0093.md#intel-acceptance-amendment--mandate-r5)
and final checkpoint below control continuation. Earlier r1–r4 checkpoints
preserve history; r5 supersedes only their Intel-pending statements.

## Implementation checkpoint

RFC base: `e82537ad15b32871a8375b6143abe08e16a68b87`.

Initial mandate r1 selected pinned-macos-integration (BPR-1758 and BPR-1759), which remains incomplete. Mandate r2 authorizes independent Windows work without waiving any Mac criterion; Windows native allocator acceptance is recorded below. Mandate r3 independently accepts the Linux allocator milestone without waiving Mac criteria. No release acceptance is presumed.

## Closeout checklist

| Criterion | Direct check / evidence | Result | Producing commit |
|---|---|---|---|
| **AC01:** Configure obtains only the pinned v3.5.3 source and licence bytes; offline, substitution and target-collision controls reject wrong inputs. | Provider pin/offline/substitution/collision/extraction controls; [ledger](../../verification/RFC-BPR-0093.md) | Pass | `81ee1b4ef` |
| **AC02:** Every selected shipped BPR executable has the override in normal and distribution graphs; disabled optional families create no phantom requirement, a core-only graph remains allocator-independent, non-distribution baseline selection works, and distribution refuses it. | All product-target attachment, actual minimal distribution/core/baseline graphs and running baseline CLI; ledger | Pass | `81ee1b4ef` |
| **AC03:** Native Apple Silicon macOS courses demonstrate C and C++ interception, representative Qt/loaded-library interception and safe cross-module allocation/free in the desktop and every selected helper/CLI, including a signed installed candidate. Universal arm64/x86_64 slices and static override evidence are verified; native Intel execution is not required or claimed (owner amendment r5). | r9 signed/notarized universal app, native trust and matching relocated closure/public rendering; prior unchanged-implementation pointer and CLI course; r5 architecture amendment; delivered Cocoa session pending | Partial: signed local trust/closure and amended architecture coverage pass; delivered native Cocoa judgment pending in r9 session | `0f42035a1` signed app; `df6e8be48` CLI |
| **AC04:** A native Windows 11 x64 course demonstrates the same behavior for the desktop and every selected helper/CLI with the actual CRT and packaged DLL closure; startup, shutdown and cross-module frees remain sound. | Native Windows four-product probes, external installed/CLI pointer and Qt-free observations, actual runtime audit/controls; [ledger](../../verification/RFC-BPR-0093.md#windows-milestone-result--mandate-r2) | Pass: Windows native allocator scope | `0ccf31bc4`, `c5693fd05` |
| **AC05:** An Ubuntu 26.04 x64 course on the selected WSL2/WSLg station demonstrates the same behavior for the desktop and every selected helper/CLI with the installed ELF closure; external child processes do not inherit allocator injection. | Native four-product probes; actual installed GDB/Qt/cross-free/OOM, CLI/MCP/XCB public paths, clean child traces and Runtime audit/controls; ledger | Pass: Linux native allocator scope | `0c40cca40` |
| **AC06:** Focused composed workflows and an allocator-boundary stress course preserve ordinary behavior and ownership; sanitizer/fuzzer builds use a compatible allocator path and allocation failure does not gain abort mode. | Native boundary/failure rows, fresh-configure executable regression on all three stations, actual ASan/UBSan/libFuzzer corpus/public CLI and paired desktop prune/delete; [final ledger](../../verification/RFC-BPR-0093.md#distribution-result--mandate-r4) | Pass: focused engineering scope | `b449356dd`, `df6e8be48` |
| **AC07:** Installed macOS, Windows and Ubuntu artifacts, standalone CLI where selected, notices, SPDX and the separate versioned corresponding-source asset agree on the pinned source and actual runtime files; existing release verification rejects a missing or extra mimalloc artifact. | r9 signed Mac ZIP/DMG/relocated install and full source/negative audit; actual DEB/MSI and all three native CLI archives retain their exact public closure/source/removal evidence; ledger | Partial: signed Mac installed/source closure now passes; delivered Cocoa and final reconciliation pending on BPR-1759/1762 | `0f42035a1`, `c5693fd05`, `0c40cca40`, `df6e8be48` |
| **AC08:** Same-workload Release comparisons and the unchanged large-review memory course record candidate identity, native platform, measurements and failures without attributing pre-existing BPR-0967 limits to mimalloc. | Corrected native matched Release six-row courses, exact hashes/conditions, raw reports and supplemental nongating close diagnostic; ledger | Pass: reporting complete; memory regressions retained, no performance approval or BPR-0967 fix | `571f96d45`, `b449356dd` |
| **AC09:** Architecture, build, package and operator documentation state the actual override scope and limits; focused public-path and documentation checks pass, with all native and release claims tied to their evidence. | Current architecture/operator and criterion ledger; documentation, hostile composition inventory and exact signed headless public-path gates | Partial: delivered Cocoa intake and final criterion reconciliation pending | `0f42035a1` |

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

## Latest checkpoint — distribution boundary, mandate r4

Sole leaf `/root/mimalloc_distribution`, run `mimalloc-20261024`, segment
`distribution-m4`, recipe `coordinator-implementer` revision 2. The checked
amendment `540b21fa9` removes the full-Mac scheduling gate from BPR-1762/1763
and explicitly retains `pinned-macos-integration` at final closeout. No authored
outcome, work, exit, AC07/AC08 requirement or BPR-1764 dependency changed.

- **Result:** BPR-1763 completes AC06/AC08's focused checks and truthful report.
  BPR-1762 is Blocked on `native-macos-candidate`; distribution milestone and
  RFC remain incomplete. BPR-1759 keeps Intel/signed installed/delivered Cocoa.
- **Candidates:** latest product/package source `df6e8be48`; fresh-configure
  correction `b449356dd`; corrected Release comparison identities and all
  archive/source/installer hashes are in the
  [final ledger](../../verification/RFC-BPR-0093.md#distribution-result--mandate-r4).
  The unchanged baseline passes Mac/Windows six rows and Linux four; mimalloc
  passes three/two/two. Additional footprint failures and higher post-close
  retention are regressions, with every limit and BPR-0967 scope unchanged.
- **Changed paths:** allocator provider and provider controls; explicit composed
  benchmark selection; standalone CLI installed-runtime/bootstrap and build
  graph; runtime/source/legal preparation and strict CLI/public PE audit paths;
  focused hostile controls, build/package/architecture docs, ledger and live
  plan/cards. No allocator API crosses core/application boundaries.
- **Verified:** current native product/failure stress, actual standalone public
  CLI and package controls on all stations; actual ASan/UBSan/libFuzzer and
  instrumented CLI; matched Release six-row and supplemental close reports;
  paired desktop prune/delete; DEB/MSI extraction audits; focused package/source/
  PE/doc/format/tidy gates. Full suite and release approval are not claimed.
- **Ownership:** main is the only integration writer. Runtime scripts/logs and
  checkpoint/observation live in `build/mimalloc-distribution/`; complete native
  roots are `C:/code/bpr-rfc0093-dist-6165b97d2cea` and
  `/home/mmarcin/bpr-rfc0093-distribution-hrxjvs5j`. Local CLI source/artifacts
  are inside that runtime root. All own commands are quiescent; no review
  server/session/cursor/URL exists. Prior candidate roots and recorded hashes
  remain unchanged. Negative copies were removed; only this task's transport
  copy and regenerable fuzz objects were removed to recover disk capacity.
- **Next:** provide the actual authorized Mac candidate/native course, verify
  affected source/runtime/signing boundaries and reconcile BPR-1762/AC07/AC09,
  then dependency-ordered BPR-1764. Preserve every earlier root and raw report.
  Main's unrelated MASK/NEXT_ID/BPR-1766 changes stay excluded. No signing
  credential/keychain lookup, publication, push or desktop capture occurred.

## Latest Mac checkpoint — mandate r4

Run `mimalloc-20261024`, segment `macos-m1-current-r4`, sole leaf
`/root/mimalloc_m1`; same coordinator-implementer recipe and authored milestone.
A fresh clean producer `75baca275aa8f052997a231a83ebc7d77d27ab6d` (product/package source `df6e8be48ff17271e2258f0040e4ca87795f44bd`)
prepared the universal current app at
`/Users/jaeger/.codex/worktrees/mimalloc-macos-r4/BetterPullReview/build/mimalloc-macos-r4/local-candidate/`.
The [ledger](../../verification/RFC-BPR-0093.md#current-mac-candidate--mandate-r4)
binds artifact hashes, public package/source/negative controls, relocated
assessment and additional native arm64 observations on every selected app
process and the retained current CLI pair. All payloads remain unchanged.

Source/tracker is retained branch `codex/mimalloc-macos-r4` in `/Users/jaeger/.codex/worktrees/mimalloc-macos-r4/BetterPullReview`;
transient evidence, reproducible external observer sources and final machine
checkpoint/observation live in `build/mimalloc-macos-r4/`. Only three docs change.
Main's unrelated MASK/NEXT_ID/BPR-1766 state and every earlier candidate are
untouched. All own commands are finished; no review session/server/URL/cursor.

The new evidence completes this permitted unattended round, not M1 or AC03.
BPR-1759/BPR-1762 and closeout remain incomplete on actual native Intel,
Developer-ID signed-installed and delivered Cocoa observations. The r4 steering
continues work without Intel while retaining its criterion. No acceptance exit,
memory limit, BPR-0967 result, product code or source pin changed. No Keychain,
Developer ID operation, publication, push or owner desktop operation occurred.
The coordinator owns integration and the next authorized acceptance round.


## Latest checkpoint — Intel acceptance amendment, mandate r5

Run `mimalloc-20261024`, segment `macos-intel-amendment-r5`, sole writer
`/root/mimalloc_intel_amendment`; source/tracker main at
`/Users/jaeger/Documents/GitHub/BetterPullReview`, entry revision
`475d506854121c4c707a3a193cc4eb0d08ce0d3f`, immutable RFC base unchanged.
The owner explicitly answered “Amend Intel criterion”. This bounded round
completes that acceptance amendment; it does not complete the Mac milestone.

- **Result:** AC03, BPR-1759 and macOS exit 2 accept the verified universal
  arm64/x86_64 static proof plus native arm64 observations without requiring
  native Intel execution. No native Intel execution is reported. BPR-1759 and
  BPR-1762 remain Blocked on actual Developer-ID signed-installed and delivered
  native Cocoa acceptance. AC03/AC07/AC09 and this closeout remain partial.
- **Candidates:** app `75baca275aa8f052997a231a83ebc7d77d27ab6d`, CLI/product
  `df6e8be48ff17271e2258f0040e4ca87795f44bd`, with the original artifact and
  image hashes in the [ledger](../../verification/RFC-BPR-0093.md#current-mac-candidate--mandate-r4).
  Both private allocator runtimes and all six selected product images retain
  their exact hashes. Static per-slice dependencies/interpose and universal
  headers pass; six recorded native arm64 ownership courses are reused unchanged.
- **Changed paths:** live RFC and index; live milestone JSON/generated view;
  BPR-1759, BPR-1762 and this card; verification ledger; system architecture and
  build guide. No product/package code, immutable pack, source receipt, other
  criterion, selector, dependency, outcome or memory limit changed.
- **Checks:** documentation gate and its 67 tests pass; plan check/render and
  focused requirement/closeout/status/preservation checks pass. `check-amendment`
  intentionally refuses the changed exit (exit 2, preservation guard), retained
  as such. The explicit owner requirement amendment supplies the authority;
  no helper success or new native acceptance is inferred. The old selection's
  recipe-text fingerprint was stale; the same revision-2 coordinator-implementer
  strategy is resolved afresh for this quiescent r5 segment and binding.
- **Ownership/commands:** all task-owned commands are finished. No product
  process, review session, server, URL or event cursor exists for r5. Current
  and earlier candidate roots remain protected and retained. Main's unrelated
  `.mask.json`, `.mask.lock.json`, `docs/issues/NEXT_ID` and BPR-1766 reservation
  retain their entry hashes and are excluded from the commit. No signing,
  release, desktop, Keychain, push or publication operation occurred.
- **Runtime/continuation:** `build/mimalloc-intel-amendment-r5/` holds mandate,
  before/proposed plans, helper refusal, selection reconciliation, static proof,
  focused checks, renewed checkpoint/observation and final handoff. The
  coordinator owns the remaining acceptance round and exact candidate binding:
  actual authorized signed-installed and delivered native Cocoa evidence, then
  affected BPR-1762 AC07/AC09 reconciliation and dependency-ordered BPR-1764.
  Intel is no longer a pending requirement. No signing approval is prepared
  by this documentary round.

## Active checkpoint — retention follow-up under r5

The coordinator resumed `/root/mimalloc_distribution` for a causal follow-up to
BPR-1763's recorded regressions; BPR-1763's terminal reporting acceptance is
not reopened or converted into a performance claim. The owner r5 Intel
amendment remains authoritative; signed-installed/native Cocoa Mac and final
package/closeout criteria remain pending.

New task-owned source worktree/branch:
`/Users/jaeger/.codex/worktrees/mimalloc-retention-r6/BetterPullReview`,
`codex/mimalloc-retention-r6`, prototype `645a8125b`. Runtime diagnostics are
`build/mimalloc-retention-r6/` on main and
`/home/mmarcin/bpr-rfc0093-retention-r6` on the approved Ubuntu station.
All prior candidates and raw records are retained. Main's unrelated allocation
and MASK edits are unchanged.

Same-byte option experiments identify delayed purging and Linux eager arena
commit/THP residency. Six unchanged Release rows pass on Mac with purge delay
zero and Linux with zero purge delay plus lazy arena commit. Default controls
retain failures. A diagnostic Linux six-second idle does not reduce RSS;
nonforced collection then drops it substantially. No budget, trim behavior,
core policy or source pin changed in these experiments.

The prototype explicitly calls a private executable-startup option shim,
retaining operator overrides and selecting prompt purging without collection
or environment changes. Linux selects the upstream lazy arena-commit default;
it does not disable process/child THP policy. This is not yet integrated or
accepted. Fresh Mac provider/default/explicit-option/OOM controls pass. New
source-owned Mac and Linux builds and direct native courses are active; actual
package/security checks remain required before any corrective integration.
The ledger and machine checkpoint will receive the final measured decision.
No signing, release/version change, push, publication or desktop capture.

### Owner correction — diagnostic collection only

The owner explicitly rejected setting product purge delay to zero merely to
pass a course, and asked whether tests can trigger a manual purge. No product
prototype was integrated into main. The isolated prototype `645a8125b` was
reverted in `676ac3b42`; its unfinished native runner and tidy were interrupted
by their exact task-owned PIDs. Its partial results are unaccepted evidence.
The current correction is limited to an opt-in benchmark diagnostic: retain
normal close/destruction samples, observe one/five seconds of Qt idle, then
call `mi_collect(true)` and record the result and cost separately. The unchanged
real-workflow course and product settings remain authoritative. No production
hook, trim policy or accepted memory-budget change is authorized by this step.

## Latest checkpoint — diagnostic retention follow-up

Same implementer `/root/mimalloc_distribution`, run `mimalloc-20261024`, segment
`distribution-retention-followup`, mandate `r5-retention-followup`; owner-r5
Intel amendment and subsequent diagnostic-only correction preserved. The
[follow-up ledger](../../verification/RFC-BPR-0093.md#retention-follow-up--benchmark-only-collection)
binds test producer `1322c71b5` and main integration `fdb7eba8b`/`d217078a2`.
Only three test paths changed: the benchmark CMake/private capability flag,
benchmark diagnostic, and actual-call allocation probe. Product/provider/
package paths are unchanged from `0f799140c`. The rejected product prototype
was reverted in its isolated branch and never integrated.

Native Mac/Linux manual-collection/default/100/10 ms timelines pass as explicit
diagnosis; baseline refusal is verified. Default five-second retained growth
was about 144/283 MiB, then about 44/98 after forced collection (4/5 ms in these
runs). The unchanged ordinary course remains Mac4/6 and Linux2/6; no manual
measurement passes that course or changes a limit. The Release malloc probe
compiler artifact is fixed by observing the actual call, with native7/7 and6/6.

Source/build roots: `/Users/jaeger/.codex/worktrees/mimalloc-retention-r6/BetterPullReview`
and `/home/mmarcin/bpr-rfc0093-retention-r6/source`; raw observations, exact
hashes, validation and machine checkpoint are `build/mimalloc-retention-r6/`.
All prior candidates remain protected. Own native/local commands are quiescent
at handoff; no review server/session/cursor/URL exists. Main's unrelated MASK,
NEXT_ID and BPR-1766 receipt remain excluded. BPR-1763 remains terminal reporting
acceptance; BPR-1762 and final signed-installed/Cocoa/AC07/AC09 closeout remain
pending. No production policy/hook, source pin, version or release change,
signing/credential access, publication, push or desktop capture occurred.

Next: use the explicit diagnostic for attribution while keeping ordinary
workflow results authoritative. Actual signed Mac qualification and final
package closeout still require their own candidate and evidence; no delay
change or automatic purge policy is approved by this follow-up.

## Active checkpoint — cleanup-policy reassessment

The owner asked the same distribution implementer to reassess test cleanup
semantics, permit test-only collection at completed cleanup boundaries when
justified, and compare the default allocator empirically. No production hook,
zero-delay default, threshold change or final acceptance amendment is inferred.
Source worktree is `/Users/jaeger/.codex/worktrees/mimalloc-cleanup-r7/BetterPullReview`,
branch `codex/mimalloc-cleanup-r7`, candidate `806df31d2`; runtime is
`build/mimalloc-cleanup-r7/`. Earlier candidates/worktrees remain unchanged.

The exact original predicate inventory found that failed observations were
printed before checking but omitted from partial JSON. Stderr reconstruction
shows active cache/loaded-content footprint and lifetime-peak failures, and one
Mac resources failure first observed after prune. The candidate retains failed
samples, adds an opt-in `--allocator-cleanup-collect` comparison for the four
resource courses, and records pre/post samples, allocator release API/cost and
per-phase numeric predicates. Numeric failures are retained until the end so
cleanup can be observed; any active, lifetime-peak or other original numeric
failure still makes the comparison fail. The original flags retain their gates.
The native allocator baseline gets its own supported release request; no claim
that it has an effective zero delay is assumed. Native heap counters which do
not cover mimalloc are explicitly marked in the predicate records.

Fresh native Mac/Linux builds are active. Next: run matched ordinary/collected
policies with mimalloc and the toolchain baseline, inspect every predicate,
check whether any actual row changes verdict, and record ambiguity before any
acceptance update. BPR-1763 stays terminal reporting acceptance; Mac signed/
Cocoa/package closeout remains pending under r5. Main's unrelated MASK/NEXT_ID/
BPR-1766 changes remain untouched; no release/signing/version/push operation.

## Latest checkpoint — completed-boundary comparison and useful-test mandate

The same implementer completed the owner-directed R7 cleanup comparison and
integrated only test paths as `c6be37f34` / `42f0ea86b`; exact test producers
are `806df31d2` / `84a177214`. The
[ledger](../../verification/RFC-BPR-0093.md#cleanup-policy-reassessment--completed-boundaries)
binds all six ordinary rows and four collected variants for native Mac/Linux
and both allocators, all raw pre/post samples, original limits, native API
results/costs, exact images and real failures. A pre-collection peak guard
prevents a lower post-collection Linux VmHWM reading from hiding a miss.
Mac MSVC-independent native builds, Linux builds and Windows native-memory
syntax checks pass. Exact format/tidy 22 pass; the collected failures retain
successful ownership/cleanup assertions and correct exit codes. Ordinary flags
call no collection. No product default or hook changed.

The full matrix's observed Mac resources fail→pass cannot be attributed solely
to collection: its pre-collection peak was already lower. Final exact-source
Mac proof instead reduces current prune growth 162.33→119.69 MiB while its
162.34 MiB lifetime peak correctly fails the 160 MiB limit. Ubuntu retains
active/peak failures and some post-collection current-memory failures. Native
heap counters do not cover mimalloc; zero is not heap acceptance. R7 raw data,
validation and native identities are in `build/mimalloc-cleanup-r7/` and the
approved station root `/home/mmarcin/bpr-rfc0093-cleanup-r7`.

The owner then expanded the continuing mandate: BPR uses mimalloc and must
pass useful tests. Audit each affected row's original purpose/history and
user-visible risk; distinguish ownership/cache bounds, active/peak footprint,
cleanup footprint and timing. Recommend retain/redefine/retire with evidence,
then implement a defensible allocator-neutral or separately calibrated gate
if warranted, preserving historical limits and failures without a silent
waiver. Evaluate realistic upstream-supported options without zero purge delay
or child-process global side effects, and verify responsiveness plus a real
public workflow before adopting any product default. No release/signing/push.
That is the next active frontier; no gate redesign is already accepted here.
Signed-installed/delivered native Cocoa Mac and final package/closeout criteria
remain pending under r5. Prior candidates and unrelated MASK edits remain
protected. No review session/server/URL/cursor is active.

### Active useful-test frontier — bounded reopen attempt

New isolated source/branch:
`/Users/jaeger/.codex/worktrees/mimalloc-memory-r8/BetterPullReview`,
`codex/mimalloc-memory-r8`; native counterpart
`/home/mmarcin/bpr-rfc0093-memory-r8/source`. Runtime and current mandate are
`build/mimalloc-memory-r8/`. R7 source/builds, raw images and candidates are
preserved. No R8 product or test-policy change is integrated on main.

The original-purpose audit retains all six courses' useful semantic/latency
claims. Their process envelopes were baseline-allocator observations plus10%,
rounded8MiB. A pinned-MI64MiB worker-allocation control proves MI statistics
underreport while another live thread has not collected, so they are not an
all-thread live-heap oracle. Same-byte lazy/nonzero-delay options improve
Linux peak293.52→187.43/181.82/176.56MiB but still miss168; all corresponding
response courses pass. No setting is adopted.

Prototype `4dbbb8981` attempts five full-composition lifecycles in one process.
It falsifies its initial-variation trend rule: Linux baseline grows less yet
fails, while MI's larger initial variation grants a larger tolerance. That
rule is not adopted or tuned to green. Its raw normal/cache-fault controls are
retained. Keeping QApplication alive across composition recreation also differs
from production's process exit, so the next bounded prototype `8f9a40aac`
uses the real Home → Close review → open → select-large-source path within one
live application. It records absolute pre/post collection footprint and every
native peak, and has a deliberately retained-comparison-owner control. One
matched Mac/Linux baseline/MI attempt is active; source/CTest policy remains
unchanged while it runs.

The coordinator bounds the decision: if no allocator-neutral process-memory
gate validates across baseline/MI without false positives, retire only the
baseline-specific RSS/peak predicates to explicit telemetry, retaining all
semantic/cache/latency gates and the historical failures. Do not invent a green
trend metric. Remaining unqualified memory risk gets a separate follow-up,
not a resource-qualification claim. No product purge0/default/hook, release,
signing, push, credentials or desktop action. Exact command IDs, identities and
raw paths remain in the runtime mandate/checkpoint; finish commands before
handoff. Signed-installed/Cocoa/package acceptance remains pending under r5.

### Intermediate useful-test checkpoint — native correctness continuation

This supersedes the earlier active prototype note. Main `64752aa7e` integrates
the owner-approved useful-test policy; `c67f07196` removes two transient shared
handle copies found by tidy. Exact C++ producer `355669cc1` has Mac/Linux 7/7
for both allocators, four additional MI public workflows passing, and genuine
failing middle-owner controls. Process memory remains unqualified; original
physical limits/misses remain explicit telemetry and plain legacy commands
keep qualification. No new trend gate, default setting or product purge hook.
[[BPR-1768]] is separately allocated by the main helper with committed
counter/receipt `2b0930053`, preserving unrelated reservations/MASK work.

Windows exact `355669cc1` is 5/7: launch child theme_light100.66ms/100ms and
cache-byte stale-comparison correctness fail. MI additional workflows are3/4
(active-work input/drain timing miss). Cache-byte cleanup was not reached;
this is not a memory pass or a BPR-0967 attribution. A direct owner-control
attempt omitted registered Qt/build binary search paths and exits before a
report; its harness failure is preserved, not accepted. Matched Windows
baseline is active, followed by a corrected control and one bounded exact
semantic reproduction. The coordinator requires a correction if reproduced,
keeping that correctness assertion fully enforced. No broad design change.

The [ledger](../../verification/RFC-BPR-0093.md#useful-resource-tests--owner-verification-amendment)
binds source history, options, failed trend hypotheses, actual comparison
samples, all old limits and direct controls. Runtime/source roots remain
`build/mimalloc-memory-r8/`, the task-owned Mac worktree, and
`/home/mmarcin/bpr-rfc0093-memory-r8/source` plus
`C:/code/bpr-rfc0093-memory-r8/source`. Earlier candidates remain protected.
No human session/server/URL/cursor is active. Signed-installed/delivered Cocoa
Mac, BPR-1762 and final package/source/trust closeout remain pending under r5;
Intel execution is not required or claimed. No release/signing/version/push,
credential or desktop action. Finish native commands and record final candidate
identity, exact results and quiescent ownership before handoff.

### Final useful-test checkpoint — quiescent boundary

The bounded owner amendment is complete. Main source slices `64752aa7e`,
`c67f07196` and `e1d222aab` match native final producer `e75a78a72` on all five
test/script paths. The [final ledger](../../verification/RFC-BPR-0093.md#corrected-held-b-barrier-and-final-native-evidence)
and `build/mimalloc-memory-r8/final-verification-summary.json` bind twelve
passing final MI resource paths across Mac/Windows/Ubuntu, genuine failing
middle-owner controls on all three, unchanged plain legacy numerical failures,
covered Mac/Linux baseline tail passes, exact identities and all raw evidence.
Heap observations outside native counter coverage remain unmeasured.

Windows's earlier generic stale-comparison failure is traced to the final
quiescence recheck, with no stale publication and the exact selected B key.
The existing separately reported preparation tail drains deferred work while
source observers stay connected; every original correctness predicate stays
enforced. Mac/Windows/Linux corrected cache-byte courses pass. No product race
repair is claimed. A single predeclared Windows same-source active-work pair
passes unchanged limits (baseline 44.2379 ms / MI 47.9343 ms close drain); original
259.9814 ms close and 100.66/100 ms launch misses remain historical failures with
unresolved cause. No retry loop or timing waiver.

Format/tidy 22, summary actual-report checks, documentation and scoped board/
plan checks are the final focused gates. Mac/Linux static manifests pass;
Windows's registered manifest remains red because contained CMake/CTest exits
0xC0000142. Direct inventory separately retains three existing unmeasured
Windows pins; no new exemption or global security approval. All production/
provider/package paths remain unchanged from `0f799140c`. Earlier native product,
sanitizer/fuzzer and package observations keep their exact original identities.

[[BPR-1768]] remains Backlog for unqualified process/allocated-heap memory risk,
independent of final RFC closeout under the explicit bounded mandate. No new
memory trend gate, limit raise, default option or production purge hook. All
prior roots/candidates, reservation history and unrelated MASK edits remain.
Current source/build roots and images stay available; MI graphs are restored,
all task commands are quiescent, and no review server/session/URL/cursor is
active. Final runtime `checkpoint-final.json` / `observation-final.json` bind
this segment to the committed state and retain distribution exit 1 pending,
exit 2 reporting pass. Next: coordinator handles actual signed-installed/
delivered Cocoa Mac and source/package/trust reconciliation under r5, then
BPR-1759/1762/1764. Native Intel execution remains waived, not claimed. No
release, signing, credentials, version change, push or desktop capture.
