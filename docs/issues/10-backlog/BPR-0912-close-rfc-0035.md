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
evidence. Runtime acceptance and final machine observation are
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
| AC-04: Downloads and staging enforce declared bounds and reject wrong identity, version, architecture, signer, digest, unsafe archive entries and changed staged candidates before replacement. | Adapter failure fixtures plus native signed-bundle validation on disposable artifacts | Actual streaming, 17 archive refusals, genuine signed universal bundle/trust and changed-tree revalidation pass on arm64; immediately-before-replacement integration remains M3 | `54535c1e` (native staging) |
| AC-05: Installation quiesces every window and process using the target, preserves drafts and pending publication reconciliation, and refuses timeout without forced termination. | Composed multi-window shutdown, flush failure, active publication, competing-process and timeout course | Not run; proposal only | None |
| AC-06: Interrupted replacement and failed relaunch retain a recoverable old application; success requires a matching new-process receipt, and cleanup touches only transaction-owned resources. | Native disposable-install fault injection at each journal boundary, restart, rollback and cleanup course | Not run; proposal only | None |
| AC-07: The packaged macOS update path upgrades an older signed version to the offered signed version, relaunches with retained user state, and reports truthful progress and terminal status. | Target-native packaged upgrade on Apple Silicon and Intel, with exact artifact digests and observed UI | Not run; proposal only | None |
| AC-08: Core, host, Qt and platform boundaries hold; unsupported platforms and managed packages expose truthful manual fallback; release and security documentation describe the shipped authority and network paths. | Architecture/composition cuts, unsupported capability fixtures, package inspection and documentation gates | Exact-candidate boundaries, UI fallback and native eligibility pass; component/security-path docs current; install package inspection and release-operator docs remain M4 | `54535c1e` (bounded composition) |

## Completion rules

Reconcile every criterion with actual commands, artifact digests, producing
commits and target-native results. Require linked implementation cards done and
RFC dependencies eligible in canonical index order before status changes. Run
`python3 scripts/verify_documentation.py --root .`,
`python3 tests/scripts/verify_documentation_tests.py` and focused public-path
checks. No full-product build is evidence for unexercised install/recovery paths.
Retain unsupported platform and unavailable native evidence explicitly.
