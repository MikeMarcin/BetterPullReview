---
schema: "implement-rfc.card.v1"
id: "BPR-0912"
title: "Verify and close RFC-BPR-0035"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0911"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0035"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0035

## Scope and continuation

[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md) implementation
was requested on 2026-09-20. Milestones 1 and 2 are accepted under [[BPR-0946]] and
[[BPR-0947]]. Installation and replacement remain gated on [[BPR-0948]].

RFC base: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.

Planned cards: BPR-0911, BPR-0912, BPR-0946, BPR-0947, BPR-0948, BPR-0949.

## Current engineering checkpoint

Run `rfc0035-20260920`, segment `m3-implementation`, mandate revision `r1`
(implement RFC-BPR-0035; no amendments). Child `/root/rfc0035_m3` owns only
`m3-install-recovery` / [[BPR-0948]] as the sole engineering/bookkeeping writer
on `main`. Initial HEAD: `f39850514b5449471fbff11b20a4998cad2560e5`;
claim: `5ca786979432273b56303c1a7d11d66b3b363dc4`. The unchanged authored plan
is bound in `build/rfc0035-run/m3-checkpoint.json`; prior accepted source and
evidence are retained below. The unrelated `.mask.lock.json` and nested Claude
worktree are preserved. Local source/build/test and disposable signed artifacts
are authorized; no live-service mutation, credential inspection, release
publication, forced process termination or developer-app replacement is allowed.

Source candidate: `d1e0f46c77b02c07e6ca9ebc4d87f964f94035d3`.
This commit implements the dormant installer/helper, exact refreshed-candidate
handoff, inode-bound installation/process leases, same-volume journaled
replacement/rollback and matching startup receipt. Draft snapshots use a bounded
append-only private store and read-only recovery presentation. Desktop shutdown
freezes admission, saves every profile's text and waits for existing lifecycle
drains. Native startup joins before profile stores and waits for readiness before
its receipt. Kernel lifetime locks survive desktop destruction until process exit.
Backup cleanup uses a durable per-inode inventory and survives partial deletion
without deleting foreign additions.

Changed paths are the core/application release-update pairs and ports; native
update transaction/helper; private SQLite/Qt recovery adapters; desktop/lifetime
composition and durability queries; associated CMake files, tests, exact spawn
owner gate and component/system/package notes. Exact inventory:
`git diff --name-only 5ca786979432273b56303c1a7d11d66b3b363dc4 d1e0f46c77b02c07e6ca9ebc4d87f964f94035d3`.
The card, this checkpoint and verification ledger contain the pending-state
bookkeeping; the runtime observation binds final HEAD without changing the plan.

Verification: shortest public path 1/1; final eight-row dev integration 8/8;
core-only focused 2/2; tests-disabled application/helper build; actual cooperating
peer process and full journal-boundary fixtures; private draft-store and two-window
preservation/refusal course; 139 architecture and four core-boundary contract cases;
clean exact-candidate architecture/core scans; 29-file and nine changed-range format
checks; 11 full-TU and ten changed-surface tidy checks. The final genuine signed
0.1.3 rollback probe passes under network denial and a disposable-root guard with
all three selected live files unchanged. It launches no application and is not an
upgrade. Exact commands, source/artifact hashes, initial failures/corrections,
logs and preexisting environmental scanner/tidy limits are retained in the
[verification ledger](../../verification/RFC-BPR-0035.md).
The documentation gate, 64 documentation tests, scoped board validation and
whitespace check pass; live plan rebinding retains the original fingerprints.

[[BPR-0948]] is Blocked on `signed-helper-upgrade-qualification`, not Done;
all native acceptance and the milestone exit remain pending. The notarized 0.1.3
ZIP predates the helper/receipt protocol. The current tests-disabled build is
ad-hoc signed and cannot bootstrap production trust. No existing artifact proves
the signed older-to-newer helper/startup/draft-retention/timeout course. Production
composition intentionally leaves the installer unselected. Explicit helper
recovery exists; native missing-receipt behavior and the user-facing recovery
launch course are still unqualified. No independent or attended acceptance is
claimed; there is no session, review server or protected candidate.

All source/test commands are joined and no task-owned process, installation or
product writer remains after handoff. The two native probe roots and clean source
export (`bpr-rfc0035-m3-native.HNgoyF`,
`bpr-rfc0035-m3-native-final.Vr2dT7`, `bpr-rfc0035-m3-export.OmYiXQ`) were moved to
`/Users/jaeger/.Trash/` and remain recoverable. Shared build trees and runtime
evidence under `build/rfc0035-run/` are retained. No developer app was replaced.
Final observation: `build/rfc0035-run/m3-observation.json` against the unchanged
M3 binding, with pending obligations and `wait` continuation.

Next action: obtain an authorized participating signed/notarized baseline and
newer candidate containing this helper/startup protocol, then continue **M3** on
disposable roots through native shutdown, retained state, exact startup receipt,
timeout, rollback and cleanup. Qualification must precede installer enablement.
The coordinator must not promote dependent [[BPR-0949]] or start M4 from these
partial results. Artifact production requiring credentials, notarization or live
publication needs its own authorization. Prior accepted M1/M2 evidence follows.

### Prior accepted milestone 2 checkpoint

Run `rfc0035-20260920`, segment `m2-implementation`, mandate revision `r1`
(implement RFC-BPR-0035; no amendments). Child `/root/rfc0035_m2` owned the sole
milestone-2 engineering and bookkeeping scope on `main`, starting from
`c81f4a0cad4f338bb3f246649faba510ce0cf9ce`. BPR-0947 claim commit:
`a8578fcc2f44bb8b4933cdb91a46478826c83122`; current binding:
`build/rfc0035-run/m2-checkpoint.json`. The selected strategy and authored plan
are unchanged. The unrelated `.mask.lock.json` modification and nested Claude
worktree are preserved. No live service mutation, credential inspection,
release publication or running-application replacement is authorized.

Accepted source candidate: `54535c1e3638af99c5e3560ce3033f4cf1ff5a06`.
Engineering commits: `5ed24a8c` (composition/transport/staging), `1f229029`
(exact signing-effect owner) and `54535c1e` (writable mounted-image refusal).
BPR-0947's four acceptance criteria and the milestone exit pass; the completed
card and [verification ledger](../../verification/RFC-BPR-0035.md) retain the
evidence. Acceptance/upkeep commit: `ec3552df09267c435e55bdb3adab07b141c74623`.
BPR-0947 is Done; BPR-0948 is promoted to Ready without being claimed.
Runtime acceptance and final machine observation are
`build/rfc0035-run/m2-acceptance.json` and
`build/rfc0035-run/m2-observation.json`, against the original m2 binding.

Changed product paths: the new `release_update` service/projection in application;
GitHub native HTTPS/release-access adapter and `github_application.cpp` vault
bridge; SQLite release-update store; the native `src/adapters/update/` adapter;
Qt release-update panel; app release-update desktop and `profile_desktop.cpp`;
their CMake composition, pinned archive provider and architecture gate. Tests are
the transport contract, two end-to-end update courses, two native Python fixture
harnesses and architecture contract test. Exact path inventory:
`git diff --name-only c81f4a0cad4f338bb3f246649faba510ce0cf9ce 54535c1e3638af99c5e3560ce3033f4cf1ff5a06`.
The component/system architecture and live RFC now describe the actual bounded
composition; release/package install claims remain unchanged.

The actual desktop menu and General Settings composition, SQLite restart/daily
scheduling and revocation course pass. All 24 local TLS transport scenarios and
17 archive refusals pass. The genuine local 0.1.3 universal ZIP, SHA-256
`72b71ebd642f83f86ff7ec889bd4e201a0586af40d32d9be9a15201e8d91a4a9`, passes native
signature/trust, native eligibility, exact revalidation and changed-tree rejection
under the network-denied harness. Final six-row CTest, clean exact-candidate
architecture/core scans, 138 architecture/4 core-boundary contract cases,
formatting and changed-surface tidy pass. The documentation gate and 64 tests pass.
The ledger retains initial failures, corrections and exact commands. Full-header
tidy retains the preexisting `home_view_state.hpp:35` finding; it is not claimed
as a global pass. The unrelated nested-worktree scan failure is also retained.

Self-review is complete. Commands are joined; no product writer, updater-owned
background command, installation, attended session, review server or protected
candidate remains. The temporary source export `bpr-rfc0035-m2.uHnO3Q` and signed
diagnostic copy `bpr-rfc0035-signed.1eKeZC` were moved to `/Users/jaeger/.Trash/`
(recoverable). Shared build outputs and runtime evidence under
`build/rfc0035-run/` are retained; the offline dependency probe's cache setting
is restored to OFF. No running developer application was replaced.

Next eligible milestone: `m3-install-recovery`, [[BPR-0948]]. The coordinator
starts a fresh implementer after this child releases ownership. It must supply
the helper, cooperative cross-window/process quiescence, pre-install authority/
candidate refresh, exact staged revalidation, journal/recovery and healthy-start
receipt, then qualify disposable signed installs before enabling replacement.
Live private-service/native-vault operation, a mounted-image installation,
Intel execution, packaged upgrade and RFC-0014/canonical closeout remain unverified.
No attended or independent acceptance is required for milestone 2; no such
acceptance or complete RFC closure is inferred from these tests.

### Prior accepted milestone checkpoint

Run `rfc0035-20260920`, segment `m1-implementation`, mandate revision `r1`
(implement RFC-BPR-0035; no amendments). Child `/root/rfc0035_m1` is the sole
milestone-1 repository writer on `main`. Initial HEAD:
`d06b6fb0272ce53bf452d87322c957f11f0f6cb1`; worktree was clean.
Selected strategy is `coordinator-implementer` revision 2; resolved snapshot:
`build/rfc0035-run/execution-strategy.json`. The installed workflow preflight
failed because it invoked obsolete `mask ensure`; the host exposes the assigned
native `rfc-implementer` role. This is a tooling observation, not acceptance.

Allocation reserved BPR-0946 through BPR-0949 for the four authored milestones.
Plan commit `ae388676`; BPR-0946 claim `bb1e9bf4`. The adopted plan is
`docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json`; exact binding is
`build/rfc0035-run/m1-checkpoint.json`. The binding is refreshed as
`build/rfc0035-run/m1-checkpoint-final.json` solely to normalize BPR-0946's mutable
evidence heading to `Verification and execution record`; its five acceptance
criteria and all milestone exits are unchanged. The original binding is retained.

Accepted source candidate: `42a85cf71bfe826a76c44dfe98e982a0bdc8c76b`.
Milestone acceptance/upkeep commit: `b6de28e2c817fd084d0a6a8a44fe9ddda438ecb1`;
BPR-0946 is Done. BPR-0947 is promoted to Ready without being claimed. The final
machine observation is `build/rfc0035-run/m1-observation.json`, checked against
the refreshed binding with quiescent ownership and `fresh` continuation intent.
Engineering commits: `64bbf9c1` (core/host/contracts), `ab5209b9` (provider
constants moved behind the adapter boundary), `42a85cf7` (complete candidate
identity and preference-restart coverage). Changed product paths are the
`release_update` header/source pairs in core and application, the application
port header, GitHub's private `github_release_update_contract.hpp`, core/host
tests and their four CMake files. Documentation is the component contract,
system architecture, [verification ledger](../../verification/RFC-BPR-0035.md),
authored milestone plan and live RFC/cards.

Verification: core-only configure/build; focused policy/public-host/foundation/
core-boundary course 4/4; both boundary contract suites; eight-file format check;
four-TU clang-tidy plus the final changed test TU; standalone private-contract
header syntax; documentation gate and 64 documentation tests. Architecture and
core scans pass on a clean export of the exact candidate. The ordinary checkout
scan hits the preexisting unrelated nested Claude worktree and remains an
environmental failure. The first export also exposed the now-fixed endpoint
placement error. Initial range-copy/capture-shadow compile warnings were fixed.

Self-review is complete. No live mutations, credential access, application
replacement, background commands, review server or protected updater candidates
exist. Disposable exports are moved to Trash after verification (recoverable);
shared build trees and the unrelated worktrees are retained. Runtime plan/binding
records in `build/rfc0035-run/` remain for the coordinator's continuation.

Next eligible milestone: `m2-download-staging`, [[BPR-0947]]. The coordinator
starts a fresh implementer after this child releases writer ownership. Real
menu/Settings controls, persistence, native release authentication/transport,
stream/archive/signature verification, install barrier/helper/recovery, native
Apple Silicon/Intel upgrades and canonical RFC-0014 closeout remain pending.
Milestone 1 requires no attended or independent acceptance; it enables no updater.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Manual and daily automatic checks, Later, Skip This Version and persisted preferences work through the shipped UI; managed and disabled installations make no automatic requests. | Composed menu/Settings, manual-clock scheduling, restart and managed-policy fixtures | Real desktop/Settings and injected-capability course, SQLite restart and automatic/manual policy pass; packaged upgrade observation remains M4 | `54535c1e` (composition) |
| AC-02: Only a strictly newer stable release with one exact compatible asset is offered; malformed versions, drafts, prereleases, duplicate assets and stale completions are refused. | Core version/selection tables and out-of-order host completions | Core, host, adapter and menu-to-offer fixture composition pass | `42a85cf7`, `54535c1e` |
| AC-03: Private release access uses an explicitly selected GitHub.com authority; revocation cancels work, credentials never cross redirect origins, and inaccessible releases are not reported as up to date. | Private-release fixture, sign-out/account replacement, redirect and secret-canary tests | Native release-only factory, Qt choice/revocation and 24 TLS scenarios pass; no live private-service or actual vault-credential observation | `54535c1e` (fixture transport) |
| AC-04: Downloads and staging enforce declared bounds and reject wrong identity, version, architecture, signer, digest, unsafe archive entries and changed staged candidates before replacement. | Adapter failure fixtures plus native signed-bundle validation on disposable artifacts | Actual streaming, 17 archive refusals, genuine signed universal bundle/trust and changed-tree revalidation pass on arm64; M3 signed rollback probe revalidates before rename, while complete signed upgrade remains pending | `54535c1e`, `d1e0f46c` (bounded native probes) |
| AC-05: Installation quiesces every window and process using the target, preserves drafts and pending publication reconciliation, and refuses timeout without forced termination. | Composed multi-window shutdown, flush failure, active publication, competing-process and timeout course | Two-window draft/save refusal and actual cooperative peer/lifetime course pass; signed shutdown, native active-publication and real helper timeout remain pending | `d1e0f46c` (partial) |
| AC-06: Interrupted replacement and failed relaunch retain a recoverable old application; success requires a matching new-process receipt, and cleanup touches only transaction-owned resources. | Native disposable-install fault injection at each journal boundary, restart, rollback and cleanup course | Journal-boundary/exact-receipt/partial-cleanup fixtures and genuine signed rollback pass; native new-process receipt/recovery course remains pending | `d1e0f46c` (partial) |
| AC-07: The packaged macOS update path upgrades an older signed version to the offered signed version, relaunches with retained user state, and reports truthful progress and terminal status. | Target-native packaged upgrade on Apple Silicon and Intel, with exact artifact digests and observed UI | No signed older-to-newer upgrade; unavailable new-protocol artifacts block M3 enablement and M4 | None (native qualification) |
| AC-08: Core, host, Qt and platform boundaries hold; unsupported platforms and managed packages expose truthful manual fallback; release and security documentation describe the shipped authority and network paths. | Architecture/composition cuts, unsupported capability fixtures, package inspection and documentation gates | Exact-candidate boundaries, UI fallback and native eligibility pass; component/security-path docs current; install package inspection and release-operator docs remain M4 | `54535c1e` (bounded composition) |

## Completion rules

Reconcile every criterion with actual commands, artifact digests, producing
commits and target-native results. Require linked implementation cards done and
RFC dependencies eligible in canonical index order before status changes. Run
`python3 scripts/verify_documentation.py --root .`,
`python3 tests/scripts/verify_documentation_tests.py` and focused public-path
checks. No full-product build is evidence for unexercised install/recovery paths.
Retain unsupported platform and unavailable native evidence explicitly.
