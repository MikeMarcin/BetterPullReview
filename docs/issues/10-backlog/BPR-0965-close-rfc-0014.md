---
schema: "implement-rfc.card.v1"
id: "BPR-0965"
title: "Verify and close RFC-BPR-0014"
status: "Backlog"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0617", "BPR-0958", "BPR-0959", "BPR-0960", "BPR-0961", "BPR-0962", "BPR-0963", "BPR-0964"]
closeout_for: ["RFC-BPR-0014"]
milestone: "M12 Verification and RFC closeout"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0014

RFC base: `d0a8a6dda005af12b3818234f4878a9f48a44146`.

This is the first dedicated RFC-0014 run's verified clean entry revision,
established under mandate r1. No earlier dedicated RFC-0014 base/closeout
record was found. Original normative import is `61ac02ea3125ada107c0709626cd6b5d884ff452`;
its predecessor is `470741a605ac401464d9e50480667ca45033ca66`.
Historical BPR-0253/0324–0328/0908 remain inherited, completed prerequisites;
this run does not reset or extend their acceptance.

## Acceptance

- AC1: Complete every canonical RFC criterion below against exact delivered
  artifacts and the real tests-disabled composition, retaining required native,
  human and independently verified trust evidence.
- AC2: Reconcile the final scoped diff against the immutable base, all live
  cards, architecture conformance, documentation, failures and owner amendments.
- AC3: Close only after RFC-BPR-0013 and all canonical predecessors are
  Auto-Verified. Update the RFC and index together, verify documentation and
  record the integrated revision and command cleanup.

## Closeout checklist

| Criterion | Direct check and accountable work | Current result and revision |
|---|---|---|
| Clean Windows, macOS Intel/Apple Silicon, and Linux test environments install, launch, authenticate, open a fixture PR, and uninstall successfully. | BPR-0958–0961; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Package signatures/notarization verify independently. | BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Runtime dependency scan shows no WebEngine, browser engine, Node, local network server, arbitrary plugin loader, or undeclared network endpoint. The existing bounded owner-authenticated profile-activation AF_UNIX IPC is the sole declared listener exception; verify its owner/peer/frame/lifetime boundary and absence of network-service capability. | BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | M9 mirrors the live RFC's mandate-r3 IPC clarification only; complete native acceptance remains pending |
| SBOM, license notices, source-offer obligations, and checksums match the delivered artifacts. | BPR-0956 and BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Hardened build settings, static analysis, sanitizers, fuzzers, secret scans, and vulnerability scans pass release policy. | BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Credential vaults, file permissions, cache locations, retention, purge, and uninstall behavior match documentation. | BPR-0953–0955 and BPR-0958–0961; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Diagnostic and crash artifacts pass canary redaction and contain no source/comment bodies by default. | BPR-0953–0955 and BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Corporate security overview clearly states network destinations, data stored, subprocesses, credentials, AI boundary, logging, and removal. | BPR-0963; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Delivered target/dependency graphs and runtime inspection prove the functional core remains Qt/provider-free and all effects are reachable only through the packaged host/composition path. | BPR-0952, BPR-0956 and BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |

## Verification and execution record

The current owner is `/root/rfc0014_m5`, assignment milestone 5, mandate r3
(preserving r1/r2), run `rfc0014-20260920`, segment
`m5-qt-presentation-adapter`, on `main`.
The available-target implementation exception permits work while earlier RFCs
remain Proposed. It does not waive a package/native requirement or permit
terminal closeout. BPR-0617 and RFC-BPR-0013 remain open; the earliest canonical
closure frontier remains RFC-BPR-0008/BPR-0242.

M5's final product candidate is `13a7d0522137eeb4ab147ff9d563de62aa978035`.
The [M5 ledger](../../verification/RFC-BPR-0014.md#milestone-5-qt-presentation-adapter)
and [durable handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m5.json)
record accepted diagnostic/maintenance presentation, real Settings/native-host
export evidence, three-scale keyboard/accessibility checks, unchanged retention
owners and exact source verification. Production destructive lifecycle remains
explicitly M6; live clear/purge commands are unavailable until composed safely.
JaegAW Windows/WSL2 station discovery is read-only, not native package acceptance.
This card and the RFC remain open; no later requirement or publication boundary
is waived by M5.

Guarded finish `42c7ac49b41fdc6b11bf072fe8a055c86ac2612e` accepts BPR-0955;
BPR-0956 is Ready for a fresh M6 owner. M5 leaves no task-owned command/server
or attended candidate running. Its temporary source archives and qualification
root were removed after retaining reproducible evidence; unrelated worktrees
and both remote station modifications were preserved. The handoff retains all
later package/native/security/predecessor obligations and the MASK discrepancy.

The authored source's twelve stages are retained in the
[adopted plan](../../rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json).
The [verification ledger](../../verification/RFC-BPR-0014.md),
[import mapping](../../rfcs/assets/rfc-bpr-0014-planning-v1.LIVE_IMPORT.md) and
[bound checkpoint](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m1.json)
retain requirements and continuation identity. No package, live GitHub/GHES
mutation, signing authority, publication or installed-application replacement
is granted by this card.

## Milestone 1 checkpoint

Planning candidate `75d9901382f6b1ec04f570d854c93b81889d44b4` completes the
bounded source reconciliation, adopted twelve-stage plan, fifteen live cards,
verification/allocation/documentation ledgers and preserved routing snapshot.
Allocation `d3d707ac` and claim `cec196fb` are ancestors. BPR-0951 alone is the
accepted milestone-1 work; BPR-0952 is the next eligible architecture slice
after its guarded finish. No other RFC or earlier acceptance is changed.

At the candidate, documentation, scoped board validation, target-only discovery,
source-copy fidelity, staged whitespace and no-product-diff checks pass.
The existing 64 documentation tests pass, and the unaccepted M2 bind refuses.
See the [ledger](../../verification/RFC-BPR-0014.md) for exact commands,
revisions, output limits and the corrected unpublished whitespace failure.

All task commands finish before handoff; no task-created worktree, review server,
native application or external writer is retained. Existing unrelated worktrees
are untouched. No attended candidate or protection was created. The coordinator
preflight discrepancy remains recorded in the import record: configuration
does not prove host loading. The successor must adopt the existing plan/base,
claim BPR-0952 and perform M2's source inventory; native package/signing,
human and predecessor requirements remain pending on their live cards.

Guarded finish accepted BPR-0951 in
`314fd9a7782c03dbef96c924a22ecaa28d3ccb5c`. BPR-0952 is Ready with its sole
local prerequisite Done. The [durable M1 handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m1.json)
contains assignment/mandate, candidate and commit identities, explicit changed
paths, commands/results, command ownership, preserved worktrees and next action.
The final mechanical observation and check receipts remain in
`build/rfc0014-run/m1-observation.json` and `m1-final-check.json`; they refer to
the final bookkeeping HEAD and do not claim additional product validation.

Before return, owner amendment r2 explicitly required inventory/reuse of the
project [publish-release skill](../../../.codex/skills/publish-release/SKILL.md).
The complete skill and underlying scripts were inspected; the
[source inventory](../../verification/RFC-BPR-0014.md#owner-amendment-r2-existing-macos-release-workflow)
maps the existing universal build, restricted Qt deployment, signing,
notarization, Gatekeeper, smoke, DMG/ZIP/checksum and guarded private-publication
mechanisms. M2/M6/M9 now explicitly reuse them and fill demonstrated gaps.
Skill/code existence is not observed package acceptance; publication authority
and all Windows/Linux, clean-target, security, SBOM/license, retention/diagnostic
and canonical closeout requirements are unchanged. The
[r2 checkpoint](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m1-r2.json)
renews mandate/segment identity with unchanged M1 obligations and exits.

The final worktree inventory observed unrelated RFC-0036 work advancing in its
separate checkout and removal of its disposable baseline checkout by another
owner (five worktrees at entry, four observed at r2 orientation). This child
created/removed no worktree and main stayed at the expected task HEAD. The
unrelated writer is outside this milestone's exclusive main checkout scope.

The r2 inventory/plan candidate is
`f66c512c7f48c01ab63d34c051675ce903872c1f`. Exact-candidate documentation,
shell syntax and empty product/release-skill diff checks pass. The current
handoff preserves r1 acceptance and r2 source inventory separately and requests
a fresh M2 owner under mandate r2. No M1 acceptance remains pending.

The final active mandate is r3: the owner authorized source-backed updates to
obsolete implementation prescriptions. The live RFC now includes required
declared third-party runtime components in the audited closure and requires
Qt/provider-free core build/test isolation without inventing a separate SDK or
general CMake embedding work. All nine canonical acceptance bullets, supported
platforms, security outcomes and milestone exits are unchanged. The
[r3 ledger](../../verification/RFC-BPR-0014.md#owner-amendment-r3-current-implementation-prescriptions)
and [renewed checkpoint](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m1-r3.json)
retain the exact rationale and unchanged M1 fingerprints. Original records are
preserved; the successor reads the r3 handoff.

The unrelated RFC-0036 writer subsequently integrated on main as
`cf490c5a9873f058ba5730759304424082f6a5d8`. This child paused during its staged
write, resumed only after a clean index, and retained the shared architecture
changes. Final M1 product no-impact is scoped from that reconciled integration
head; the immutable RFC base is unchanged and the unrelated product work is
not certified by M1's planning checks.

Final M1 prescription candidate:
`3deec8893c3f722096f797c45ecfde18e1e71534`. Exact-candidate documentation and
scoped product-no-impact checks pass; the acceptance section comparison proves
all nine canonical bullets are unchanged. The durable handoff now uses r3,
retains r1/r2 commits and the external integration separately, and requests a
fresh M2 implementer. No attended decision or task-owned command remains pending.

## Milestone 2 checkpoint

The [allocation](../../architecture/release-readiness.md) and
[M2 ledger](../../verification/RFC-BPR-0014.md#milestone-2-architecture-allocation)
bind exact source ownership, existing release/maintenance reuse, G1–G9 gaps and
all nine public/native/security checks to the remaining cards. The
[M2 binding](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m2.json)
retains accepted M1, mandate r3 and the unchanged authored milestone exits.
The default Linux baseline is now a declared engineering target, not native
qualification. The existing authenticated profile-activation IPC and old
no-local-server criterion remain explicitly unresolved for M9.

Real maintenance/composed-host checks pass; the architecture verifier passes
an exact tracked-source archive but the checkout scan still sees an unrelated
ignored nested worktree. Product, release script and publish-release skill
bytes are unchanged. No remote command, package, native trust result or human
verdict is supplied by allocation. BPR-0953 is the next core-policy milestone
after BPR-0952's guarded finish; this card and RFC stay open.

M2 allocation candidate is `fa93f89a22a189042486ccfc757e6759b982c072`;
guarded finish `57b3bce38495c544dba36b8aa279c5a7c33b068c` accepts BPR-0952.
Documentation, scoped board, target-only discovery, no-product-change and
isolated architecture receipts pass at the candidate; 64 documentation tests
pass with unchanged test inputs. All nine RFC bullets remain unchanged.
The [durable M2 handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m2.json)
names Ready BPR-0953 and the fresh M3 assignment. No M2 human decision or
task-owned command remains; all M3–M12/native/security/predecessor obligations
are retained.

## Milestone 3 checkpoint

M3 is bound to BPR-0953 under mandate r3, claimed at
`d6a39867839283b12e0cdb597e605018041814fd`. The
[core contract](../../architecture/release-maintenance.md) and
[M3 binding](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m3.json)
retain the exact new diagnostic/purge policy, reused semantic owners and all
remaining host/Qt/package/native/security obligations. Source candidate
`7445b09efd0f831ae0838f1eee3c430cdc248ebc` passes the exact-candidate focused
core integration, source boundary, format/tidy, standalone ASan/UBSan,
documentation/board/discovery and preserved-acceptance checks. The
[M3 ledger](../../verification/RFC-BPR-0014.md#milestone-3-deterministic-functional-core)
records exact commands, prior failures and reused architecture-contract evidence.
All BPR-0953 criteria and both M3 exits are accepted. Guarded finish
`b5a325423e1f11813fcc7cf9a6afd2b899ee350a` moves BPR-0953 to Done; BPR-0954
is Ready. The [M3 handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m3.json)
records cleanup and command quiescence. Next action is a fresh M4 owner for
the actual bounded host effects. No review server or remote operation is active.
Existing unrelated worktrees and every later native/security/predecessor
obligation are preserved. This child stops at the M3 boundary.

## Milestone 4 checkpoint

M4's source candidate `19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8` supplies
bounded host/native effects, exact inventory/credential cleanup and recovery,
metadata-only local export and the existing logger's private Windows mechanism.
The [M4 contract](../../architecture/release-maintenance.md#m4-host-and-native-effects)
records lifecycle ownership, deliberate residuals and r3's full-profile purge
adjustment while preserving ordinary retention/migration owners. The
[M4 ledger](../../verification/RFC-BPR-0014.md#milestone-4-imperative-application-host)
and [binding](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m4.json)
retain the exact source, direct tests and both authored exits.

All 13 host/storage/vault and six real logging/profile/retention courses pass
at the candidate. New host/native TUs pass focused ASan/UBSan; Windows mechanisms
and portable/native test TUs pass warnings-as-errors source compilation.
Architecture, format/tidy, documentation/64 verifier tests, board/plan and
unchanged canonical criteria pass. These accept BPR-0954/M4 only, not native
Windows/MSVC execution, real OS-vault continuity, UI/human acceptance, full
uninstall, package trust or crash artifacts. M5 must use the new typed host;
M6 must wire its real lifecycle. Every platform/security/predecessor requirement
and the MASK drift/host-load discrepancy remains open. This child stops at M4;
no publication authority or running review session is added.

Guarded finish `2475b4ba5af545236ac0f68f1738bc76f09685a3` accepts BPR-0954;
BPR-0955 is Ready for the fresh M5 owner. The
[durable M4 handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m4.json)
retains the source candidate, commands, explicit remaining obligations and
quiescent ownership. Temporary source/sanitizer outputs were removed; ignored
receipts and normal builds remain. No task-created worktree, review server or
native app is handed off, and unrelated worktrees were preserved. This card
and the RFC remain open.
