---
schema: "implement-rfc.card.v1"
id: "BPR-0973"
title: "Verify and close RFC-BPR-0038"
status: "Backlog"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-0965", "BPR-0958", "BPR-0959", "BPR-0960", "BPR-0961", "BPR-0962"]
closeout_for: ["RFC-BPR-0038"]
milestone: "M13 Native package and release qualification closeout"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0038

RFC base: `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`.

Allocated under RFC-0014 owner amendment r4. This successor retains all original
release qualification criteria, not a promise to accept them at a later date.
The [successor RFC](../../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md)
and [ledger](../../verification/RFC-BPR-0038.md) preserve exact partial evidence.
All BPR-0958–0962 criteria remain pending until actually supplied.

## Owner amendment 2026-10-16 — tested environments

The owner's matrix amendment, confirmed under
[BPR-1743](../60-done/BPR-1743-confirm-or-withdraw-the-october-16-qualification-matrix-amendment.md)
on 2026-10-23 UTC ("Windows 11 and ubuntu wsl2 are tested. If it works on other
things is unproven, but not disallowed"), sets the environments behind the
first checklist row. "Clean Windows" means Windows 11 x64 on JaegAW, as a
non-elevated ordinary-user desktop. "Linux" means Ubuntu 26.04 x86_64 under
WSL2/WSLg on JaegAW, with the DEB and per-user archive. macOS is unchanged.
Windows 10, other distributions and native Linux desktops are untested. They
are not refused, and closeout claims none of them.

The nine checklist criteria keep their words. The amendment accepts none of them,
and the native, vault, crash-privacy and vulnerability gaps stay open. Closeout
also checks that README, the package guide and the corporate security overview
name only the tested environments and say plainly that others are untested.

## Acceptance

- AC1: Complete every canonical criterion below against exact final delivered
  artifacts and the actual tests-disabled public composition, including required
  native, human, independent trust and security-owner decisions.
- AC2: Reconcile the final scoped diff against the immutable baseline and
  inherited RFC-0014 evidence, with every failed scan, changed candidate,
  platform, license/source-access/retention and privacy obligation accounted for.
- AC3: Finish all explicit dependencies and acceptance criteria; update RFC/index,
  documentation and exact-candidate evidence together, recording final integrated
  revision, protected candidates and command cleanup. No distribution is implied
  by closeout without separate explicit publication authority.

## Closeout checklist

| Criterion | Direct check and accountable work | Current result and revision |
|---|---|---|
| Clean Windows, macOS Intel/Apple Silicon, and Linux test environments install, launch, authenticate, open a fixture PR, and uninstall successfully. | BPR-0958–0961: partial exact candidates; full tested matrix pending (Windows 11 and Ubuntu 26.04 WSL2/WSLg on JaegAW, macOS Intel/Apple Silicon). | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Package signatures/notarization verify independently. | BPR-0962: macOS ad-hoc only, Windows MSI unsigned, Linux final packages/signing pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Runtime dependency scan shows no WebEngine, browser engine, Node, local network server, arbitrary plugin loader, or undeclared network endpoint. The existing bounded owner-authenticated profile-activation AF_UNIX IPC is the sole declared listener exception; verify its owner/peer/frame/lifetime boundary and absence of network-service capability. | BPR-0962: bounded macOS hostile/IPC and native inspector evidence; full native active-operation, foreign-user and network/privacy matrix pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| SBOM, license notices, source-offer obligations, and checksums match the delivered artifacts. | BPR-0962: latest Qt 6.11.2 Mac/Windows mechanical inventories pass; Mac licensing-v003 accepted only for its bytes; Windows v002 unanswered, Linux and actual recipient access/retention pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Hardened build settings, static analysis, sanitizers, fuzzers, secret scans, and vulnerability scans pass release policy. | BPR-0962: latest raw Grype exit 2 with 32/30 matches, broad development 374/415 and serial Qt 75/84; full policy and independent dispositions pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Credential vaults, file permissions, cache locations, retention, purge, and uninstall behavior match documentation. | BPR-0958–0961 and BPR-0962: bounded implemented maintenance and fixture evidence; full native vault/permissions/purge/removal matrix pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Diagnostic and crash artifacts pass canary redaction and contain no source/comment bodies by default. | BPR-0962: metadata/environment-canary evidence only; resident source/comment/token crash and native privacy course pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Corporate security overview clearly states network destinations, data stored, subprocesses, credentials, AI boundary, logging, and removal. | BPR-0962/BPR-0973: existing corporate packet describes implementation and gaps; reconcile final qualified artifacts before release. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |
| Delivered target/dependency graphs and runtime inspection prove the functional core remains Qt/provider-free and all effects are reachable only through the packaged host/composition path. | BPR-0962: tracked-source and exact available Mac/Windows package graph checks pass; complete delivered/native matrix pending. | Inherited exact evidence at split entry `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`; not complete qualification |

## Continuation

This card is unclaimed. Obtain the missing native/operator/security inputs
through a fresh bounded assignment. The unchanged M8/M9 exits remain pending
under the shared plan; complete them before M13. Resume the existing Windows
licensing review only when attended work is explicitly requested, retaining
session `cbd48b91000b00f1e72e1ad62c7c384c`, event cursor 478 and its exact
protected v002 fingerprint. No live review server or remote writer is handed off.
