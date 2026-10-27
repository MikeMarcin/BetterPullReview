---
schema: "implement-rfc.card.v1"
id: "BPR-0912"
title: "Verify and close RFC-BPR-0035"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0911"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0035"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"},"triage_20261026":{"date":"2026-10-26","disposition":"updated","reason":"RFC-BPR-0014 is Auto-Verified and the canonical-order gate was removed on 2026-09-27; the private-release evidence on BPR-0911 is the real remaining gate."}}
---

# Verify and close RFC-BPR-0035

## Scope and continuation

[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md) implementation
was requested on 2026-09-20. Milestones 1, 2 and 3 are accepted under [[BPR-0946]],
[[BPR-0947]] and [[BPR-0948]]. [[BPR-0949]] has completed available-target M4
engineering and is Done under its amended Apple Silicon acceptance in r3.
Intel updater validation is deferred to [[BPR-0950]] and does not gate this closeout.
BPR-0911 is Blocked on live private-release/vault evidence; this card remains
Backlog with that prerequisite and RFC dependency/order gates open.

RFC base: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.

Planned cards: BPR-0911, BPR-0912, BPR-0946, BPR-0947, BPR-0948, BPR-0949.

Owner amendment r3 (2026-09-20) removes Intel Mac updater validation from M4 and
this closeout. BPR-0950 is the separate unstarted follow-up, not a required planned
card or prerequisite. This acceptance decision does not report native Intel
success and does not waive live private-release/vault evidence or other RFC gates.

## Acceptance

- AC-01: Manual and daily automatic checks, Later, Skip This Version and persisted
  preferences work through the shipped UI; managed and disabled installations
  make no automatic requests.
- AC-02: Only a strictly newer stable release with one exact compatible asset is
  offered; malformed versions, drafts, prereleases, duplicate assets and stale
  completions are refused.
- AC-03: Private release access uses an explicitly selected GitHub.com authority;
  revocation cancels work, credentials never cross redirect origins, and
  inaccessible releases are not reported as up to date.
- AC-04: Downloads and staging enforce declared bounds and reject wrong identity,
  version, architecture, signer, digest, unsafe archive entries and changed staged
  candidates before replacement.
- AC-05: Installation quiesces every window and process using the target,
  preserves drafts and pending publication reconciliation, and refuses timeout
  without forced termination.
- AC-06: Interrupted replacement and failed relaunch retain a recoverable old
  application; success requires a matching new-process receipt, and cleanup touches
  only transaction-owned resources.
- AC-07: The packaged macOS update path on Apple Silicon upgrades an older signed
  version to the offered signed version, relaunches with retained user state, and
  reports truthful progress and terminal status.
- AC-08: Core, host, Qt and platform boundaries hold; unsupported platforms and
  managed packages expose truthful manual fallback; release and security
  documentation describe the shipped authority and network paths.

Reconcile every criterion with actual commands, artifact digests, producing
commits and target-native results. Require linked implementation cards done and
explicit RFC dependencies satisfied before status changes. Run
`python3 scripts/verify_documentation.py --root .`,
`python3 tests/scripts/verify_documentation_tests.py` and focused public-path
checks. No full-product build is evidence for unexercised install/recovery paths.
Retain unsupported platform and unavailable native evidence explicitly.

## Verification and execution record

### Current engineering checkpoint

Run `rfc0035-20260920`, segment `m4-post-block-intel-deferral`, mandate revision
`r3`; sole engineering/bookkeeping/product owner `/root/rfc0035_m4_resume` on
`main`, initial HEAD `fe728286093326839b63f49f83487d61b5d88ad8`. The previous M4
owner and commands are quiescent. The preserved coordinator-implementer selection
is unchanged. No review session, server or protected updater candidate exists.
Unrelated `.mask.lock.json` and all separately owned worktrees remain untouched.

The owner expressly defers Intel validation to BPR-0950 outside M4 and RFC-0035
closeout. Allocation/amendment commit `1ded36b9` includes the Backlog follow-up,
counter and reservation; resumed BPR-0949 claim is `9933db57`. The authored plan
retains all four milestones, all work selectors and the M4 exit; its boundary
explicitly records the deferral and the unwaived live-service/dependency gates.
`milestone_plan.py check-amendment` accepts
only M4 as affected. Initial r3 binding: `build/rfc0035-run/m4-r3-checkpoint-initial.json`.
The r2 checkpoint/observation is preserved as history. The amended r3 binding at
`build/rfc0035-run/m4-r3-checkpoint.json` is retained; final binding
`build/rfc0035-run/m4-r3-checkpoint-final.json` refreshes only the closeout's
AC-07 line wrapping after reconciliation. Its definition and obligation
fingerprints bind the explicit acceptance amendment, not a change to M1–M3 or an
invented native observation.

Product source remains `518c384a285ef3cb873d12ea167488d105aa91b7`; qualified M3
source remains `b7e2df46cc178bbcff88b49dd2170ecf27a8c764`, signed 0.0.352 package
source `5db1d104a2436a2b0df811ac2e40cff0c58c2319`. This round changes acceptance
and documentation only. The prior signed package does not contain the native
menu correction; its unchanged installer/helper course and the separate native
menu check are credited to their exact candidates.

Acceptance commit `d6b5ed528b6646830debf5173de2f77c35c83144` finishes BPR-0949
with all three amended card criteria passing. Its acceptance file is
`build/rfc0035-run/m4-r3-acceptance.json`. BPR-0911 moves to Blocked on
`private-service-qualification`, with all its implementation prerequisites Done;
this card stays Backlog and RFC-0035/index remain Proposed. Full M4 is blocked:
RFC AC-03 still lacks actual vault-backed private-release access, observed live
asset routing and live revocation. No live credential/service authority is granted
by r3. RFC-0014 remains Proposed with its broader packaging/security criteria and
RFC-0013 dependency; the canonical closure frontier remains RFC-0008/BPR-0242.
These are separate from the explicitly nonblocking Intel follow-up.

Verification: shortest public-path 1/1 before edits; one updater/native-menu
integration pass 9/9 at `9933db57` (21.50 seconds); documentation gate and 64
documentation tests; scoped board/plan checks and whitespace check pass. Runtime
command receipts are `build/rfc0035-run/m4-r3-{integration,documentation,documentation-tests}-evidence.json`.
`git diff --exit-code 518c384a285ef3cb873d12ea167488d105aa91b7 HEAD -- src tests cmake CMakeLists.txt CMakePresets.json`
is empty: no product/test/build input changed. Reuse earlier signed/helper,
boundary and lint evidence with their original limits. The final documentation
and board gates run again for the final lane reconciliation; test-suite source
did not change. No new build, signing, live credentials, release access, tag, push,
publication or installed/developer-app replacement occurred.

Changed r3 paths are the Intel follow-up/counter/reservation, BPR-0949's amended
acceptance and lane, BPR-0911's lane/reason, this closeout, RFC-0035/index, plan JSON
and generated view, ledger and updater references in README, component/operator/
security/package documentation and RFC-0014's scoped exception. Exact inventory:
`git diff --name-status fe728286093326839b63f49f83487d61b5d88ad8 HEAD`.
The immutable RFC baseline above and prior M1–M4 source inventories remain the
accepted comparison basis; this round changes no implemented behavior.

All task-owned commands and native fixture processes are joined. No helper/app
installation, temporary source worktree, review server, attended session, protected
candidate or pending human decision was created. Shared logs and packages remain
in `build/rfc0035-run/`. `.mask.lock.json` retains its unrelated preexisting bytes
and the external native-markdown, nested Claude and superseded sibling worktrees
are preserved. The final runtime observation `build/rfc0035-run/m4-r3-observation.json`
binds this bookkeeping commit's exact HEAD, with BPR-0949 pass, parent/closeout/exit
pending, `blocked` disposition, `wait` continuation and quiescent ownership; the
checkpoint check records consistency, not acceptance authority.

Next action: resume this same M4 with explicitly authorized live private-release/
native-vault account and target evidence, then reconcile RFC-0014 and canonical
closeout eligibility. The r3 amendment requires no Intel course or attended review
to take effect; live-service authority remains separate. BPR-0950 stays outside
this milestone. This owner stops at the assigned final-milestone boundary; no
other RFC work starts.

### Prior blocked M4 checkpoint (mandate r2)

Run `rfc0035-20260920`, segment `m4-native-closeout`, mandate revision `r2`.
Sole engineering/bookkeeping/product owner `/root/rfc0035_m4` worked only on M4 /
[[BPR-0949]] and this bounded parent/closeout reconciliation on `main`, from
`c8b16090473c694d70470a1e86f51e952b1d22a8`.
Claim: `79217b124758880524ac6a7e81471b1b680bee37`.
Engineering candidate: `518c384a285ef3cb873d12ea167488d105aa91b7`.

That candidate fixes the observed duplicate native Check for Updates entries by
owning one AppKit application-menu item for the updater lifetime. Native Cocoa
checks count and invoke it through two windows, Settings, empty desktop and
reopening; repeated attachment is idempotent. The 50 ms HTTPS cancellation probe
race found during integration was corrected to cancel deterministically after the
first response byte, retaining exact request/secret-containment assertions.
There is no installer/helper/authority policy change in M4.

New [operator/recovery](../../development/application-updates.md) and
[security](../../security/release-updater.md) guides cover source authority,
redirects, process/resource inventory, bootstrap/manual fallback, trust, storage,
retention and absolute-path signed-helper recovery. README, package rules,
RFC-0014's narrow updater exception and component/security notes are synchronized.
The code-simplifier pass removed an abandoned menu lookup helper, made attachment
idempotent and kept native presentation ownership out of the host/core.

Verification at the candidate: updater integration 9/9 (including actual native
AppKit menu and 24 loopback HTTPS scenarios), tests-disabled application build,
139 architecture and four core-boundary contract cases, both boundary scans from
an exact clean source export, seven full-file plus Objective-C++ format checks,
one complete and five changed-header-surface tidy translation units, documentation
gate/64 tests and whitespace checks. Initial failed menu designs, cancellation
fixture race, localizability/naming corrections and unrelated full-header tidy
diagnostic are retained in the [ledger](../../verification/RFC-BPR-0035.md).
M1–M3 checks and exact signed package evidence below remain credited only to their
unchanged surfaces. The final 0.0.352 package does not contain M4's menu correction;
that correction has native developer-test evidence and an ad-hoc build, not a
new production-signed/notarized package or Intel execution claim.

Changed engineering/docs paths: the Qt release-update menu/header/source/CMake,
app updater composition, native/public-menu tests, transport test/harness,
README, component contract, operator/security guides, foundation security,
RFC-0014 and packaging guide/instructions. Exact inventory:
`git diff --name-only 79217b124758880524ac6a7e81471b1b680bee37 518c384a285ef3cb873d12ea167488d105aa91b7`.
Upkeep changes only BPR-0949's lane/record, BPR-0911/0912, RFC-0035/index and ledger.

The original strategy/plan remain unchanged. Initial binding is
`build/rfc0035-run/m4-checkpoint.json`; final binding is
`build/rfc0035-run/m4-checkpoint-final.json`, refreshed solely because BPR-0911's
mutable evidence heading and this closeout's unchanged eight criteria/completion
rules were separated from mutable checkpoints. No obligation or exit criterion
was removed or waived. The final machine observation is
`build/rfc0035-run/m4-observation.json`, disposition `blocked`, all whole-card/exit
obligations pending, continuation `wait` for this same M4. The original binding is
retained for comparison. The final upkeep revision is the commit adding this
checkpoint; the observation binds that exact HEAD without a self-referential hash.

[[BPR-0949]] is Blocked on `intel-and-private-service-qualification`: no Intel Mac
execution is available, and no live private-service/vault account/target authority
has been granted. Hardware registry/host inspection gives this arm64 Mac and a
Windows/Linux station, not an Intel Mac. Universal headers or Rosetta are not
native Intel evidence. Authorized signing/notarization from r2 does not authorize
GitHub release access, publication, tags, pushes or installed/developer-app
replacement; none occurred in M4. [[BPR-0911]] and this card remain Backlog.
RFC-0035 and its index remain Proposed: RFC-0014 is Proposed, depends on Proposed
RFC-0013, and the canonical closure frontier is still RFC-0008/BPR-0242.

All task commands and native test processes are joined at handoff. No product
writer, review server, attended session, protected candidate or pending human
decision exists. Temporary source export `bpr-rfc0035-m4-export.AJhcpB` was moved
to `/Users/jaeger/.Trash/` (recoverable). Shared build/runtime logs and all signed
M3 packages remain under `build/rfc0035-run/`. The unrelated `.mask.lock.json`
amendment and all separately owned worktrees are preserved, including
`/Users/jaeger/.codex/worktrees/14a8/BetterPullReview`, the nested Claude checkout
and superseded sibling worktree. No M4 worktree or new signing artifact was made.

Next action: resume M4 only when an Intel Mac course and explicitly authorized
live private-release/vault target/account evidence can be supplied. Bind that
course to its exact final signed candidate; do not assume the M3 package contains
M4's menu change. Reconcile all eight criteria and the unchanged canonical
dependencies before closing BPR-0949, BPR-0911, BPR-0912 or RFC-0035. There is no
unattended local engineering prerequisite left to complete under this mandate.

### Prior accepted milestone 3 checkpoint (mandate r2)

Run `rfc0035-20260920`, segment `m3-post-block-signing`, mandate revision `r2`.
Sole owner `/root/rfc0035_m3_resume` resumes only `m3-install-recovery` /
[[BPR-0948]] on `main` from `14efff7ee60bc1a10c5122402e48aca2f44c690a`.
The prior owner and commands are quiescent; no review server or protected
candidate exists. Claim: `182733b3105db797fbf345bc4ac099053f18e005`.
The unchanged plan/obligations are bound in `build/rfc0035-run/m3-r2-checkpoint.json`.

Owner amendment r2 authorizes local production signing/notarization credentials
for disposable qualification artifacts. Publication, tags, pushes, GitHub releases,
live review mutations and installed/developer-application replacement remain
prohibited. The signing identity and existing `BetterPullReview-notary` profile
are available; no credential material is retained. Disposable versions `0.0.350`,
`0.0.351` and final installer-enabled `0.0.352` are stable SemVer, ordered below
the repository's latest `v0.1.3` release, and
cannot be offered as upgrades to that production version. Main's version and
release notes remain unchanged. The unrelated `.mask.lock.json` and nested
Claude worktree remain preserved.

Engineering commits: `5cf310c42ac392c704d4722f344afe81886ea89e` binds the canonical
private state root/inode before startup opens stores;
`b7e2df46cc178bbcff88b49dd2170ecf27a8c764` enables the qualified macOS installer
and adds the reusable native qualification course. This final source is accepted.
The final signed package source `5db1d104a2436a2b0df811ac2e40cff0c58c2319`
differs only in CMake's disposable version. The local branch
`codex/rfc0035-m3-r2-qualification` retains all three version/source commits;
main's release version and notes are unchanged.

Actual native results: two signed upgrades (0.0.350→0.0.351, then →0.0.352),
cooperative two-window shutdown, exact startup receipt, retained window IDs/drafts/
possibly-transmitted publication state, real helper timeout without termination,
missing-receipt rollback and signed backup-helper recovery all pass. Native UI
shows the exact installed version and healthy receipt; the recovered old app
truthfully reports no completed installation and no publication retry. Final ZIP
SHA-256: `29ab8fbfc786864ca7f27bdbe95315bcce254fabdb4e5c0695973c2bf1bc3fa7`.
Final executable SHA-256:
`1882523982a47a6c0abaa528f09fb19bb6bf8913629252d22f507b22bc5028c5`.
The [ledger](../../verification/RFC-BPR-0035.md) records all artifacts, commands,
failed probe corrections and scope limits. Native result records are retained at
`build/rfc0035-run/native-r2-evidence/`; package and gate logs share that run root.

Verification: final updater integration 8/8; 139 architecture and four core-boundary
contract cases; both boundary scans on an exact clean source export; six full-file
and two changed-range format checks; three complete and three changed-surface
translation-unit tidy checks; documentation gate and 64 documentation tests.
Unchanged M1/M2/core-only/fault evidence is retained rather than re-inferred.
Existing unrelated full-header tidy diagnostics remain documented.

Changed paths since resumed claim: six state-root contract/composition/test files,
installer composition, public-path and transaction drivers, native Python harness,
component/system/package/RFC notes and this milestone's ledger/cards. Exact list:
`git diff --name-only 182733b3105db797fbf345bc4ac099053f18e005 b7e2df46cc178bbcff88b49dd2170ecf27a8c764`.
Acceptance and final binding observation are `build/rfc0035-run/m3-r2-acceptance.json`
and `build/rfc0035-run/m3-r2-observation.json`; the original M3 fingerprints remain
unchanged. No attended session, review server, protected candidate or pending
M3 human decision exists.
Acceptance/upkeep commit: `f0e783afaa8681785cd20582eb1f27a1a141aed4`.
[[BPR-0948]] is Done; [[BPR-0949]] is promoted to Ready without a claim.

All task commands/native applications are joined or orderly quit; the detached
source worktree was removed only after its commits were retained on the branch
above. Disposable installations, failed probes/export and qualification build
were moved to `/Users/jaeger/.Trash/bpr35.6aKkXi`,
`/Users/jaeger/.Trash/bpr-rfc0035-m3-r2.g1eYKY` and
`/Users/jaeger/.Trash/bpr-rfc0035-m3-r2-build-g1eYKY` (recoverable). Packages/logs
remain under `build/rfc0035-run/`; no product writer remains.
The final repository inventory also contains the separately owned
`/Users/jaeger/.codex/worktrees/14a8/BetterPullReview` worktree on
`codex/rfc-bpr-0036-native-markdown`; it was not modified or cleaned by this owner.

Next action: the coordinator may start a fresh M4 owner for [[BPR-0949]], now
Ready. M4 was not started here. Carry forward Intel execution,
live private-release/vault verification, self-service recovery/operator/security
closeout and the observed duplicate Check for Updates menu entries in a two-window
native session. RFC and parent/closeout cards remain open. No release publication,
tag, push, account authorization or developer-app replacement is implied.

### Prior M3 blocked checkpoint (mandate r1)

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
| AC-01: Manual and daily automatic checks, Later, Skip This Version and persisted preferences work through the shipped UI; managed and disabled installations make no automatic requests. | Composed menu/Settings, manual-clock scheduling, restart and managed-policy fixtures | Pass for composed UI/persistence/policy and signed Apple Silicon workflow. M4 native menu count/invocation covers two windows, Settings and empty desktop; no Intel or live automatic private-service observation | `54535c1e`, `b7e2df46`, `518c384a` |
| AC-02: Only a strictly newer stable release with one exact compatible asset is offered; malformed versions, drafts, prereleases, duplicate assets and stale completions are refused. | Core version/selection tables and out-of-order host completions | Core, host, adapter and menu-to-offer fixture composition pass | `42a85cf7`, `54535c1e` |
| AC-03: Private release access uses an explicitly selected GitHub.com authority; revocation cancels work, credentials never cross redirect origins, and inaccessible releases are not reported as up to date. | Private-release fixture, sign-out/account replacement, redirect and secret-canary tests | **Pending live evidence.** Native release-only factory, Qt choice/revocation and 24 TLS scenarios pass; no actual vault credential, private-release route or live revocation observed. Authority is not granted | `54535c1e`, `518c384a` (fixture transport only) |
| AC-04: Downloads and staging enforce declared bounds and reject wrong identity, version, architecture, signer, digest, unsafe archive entries and changed staged candidates before replacement. | Adapter failure fixtures plus native signed-bundle validation on disposable artifacts | Pass for actual streaming, 17 refusals, signed universal trust/changed-tree revalidation and pre-rename revalidation on Apple Silicon; exact signed 0.0.350→0.0.352 package upgrade passes. No Intel execution inference | `54535c1e`, `b7e2df46`; package `5db1d104` |
| AC-05: Installation quiesces every window and process using the target, preserves drafts and pending publication reconciliation, and refuses timeout without forced termination. | Composed multi-window shutdown, flush failure, active publication, competing-process and timeout course | Pass for public two-window/save-refusal and cooperating peer fixtures plus actual signed shutdown and 30-second helper timeout. Synthetic possibly-transmitted publication and both drafts retained; no live remote publication was attempted or replayed | `d1e0f46c`, `5cf310c4`, `b7e2df46` |
| AC-06: Interrupted replacement and failed relaunch retain a recoverable old application; success requires a matching new-process receipt, and cleanup touches only transaction-owned resources. | Native disposable-install fault injection at each journal boundary, restart, rollback and cleanup course | Pass for boundary/foreign-cleanup fixtures, matching signed new-process receipt, actual 60-second missing-receipt rollback, and signed backup-helper interrupted-rename recovery on Apple Silicon. Operator procedure documented; no recovery wizard or attended approval | `d1e0f46c`, `5cf310c4`, `b7e2df46`, `518c384a` |
| AC-07: The packaged macOS update path on Apple Silicon upgrades an older signed version to the offered signed version, relaunches with retained user state, and reports truthful progress and terminal status. | Target-native packaged upgrade on Apple Silicon, with exact artifact digests and observed UI | **Pass under owner amendment r3.** Apple Silicon 0.0.350→0.0.351→0.0.352 succeeds with exact receipt/state/UI/digests in the ledger. Intel execution is deferred to BPR-0950 outside M4/RFC closeout and is not claimed passed. M4 native-menu correction has separate Cocoa test evidence and is not in that signed package | `b7e2df46`; package `5db1d104`; menu developer candidate `518c384a`; owner amendment r3 |
| AC-08: Core, host, Qt and platform boundaries hold; unsupported platforms and managed packages expose truthful manual fallback; release and security documentation describe the shipped authority and network paths. | Architecture/composition cuts, unsupported capability fixtures, package inspection and documentation gates | Pass for exact-source boundaries, fallback/eligibility fixtures, M3 package/helper inspection and M4 operator/recovery/security documentation. Wider cross-platform package/uninstall matrix remains RFC-0014's unclosed scope | `54535c1e`, `b7e2df46`, `518c384a` |

No table row substitutes fixtures for live authority or reports Intel hardware
success. Intel qualification is expressly deferred by r3 to nonblocking BPR-0950.
BPR-0949:AC-01 passes within its amended scope. Full BPR-0911 acceptance and this
closeout remain pending for live AC-03 evidence; M4's exit additionally retains
RFC-0014 and canonical-order gates.

## Triage, 2026-10-26

The "RFC-0014 and canonical-order gates" mentioned
above no longer hold. RFC-BPR-0014 is Auto-Verified under owner amendment
r4, and on 2026-09-27 the owner removed the canonical-order rule globally.
The gate that remains is [[BPR-0911]]'s explicitly authorized live
private-release, vault and revocation evidence. Native Intel qualification
stays the nonblocking [[BPR-0950]], and the owner skipped it again for
v0.3.4. The card stays in Backlog.
