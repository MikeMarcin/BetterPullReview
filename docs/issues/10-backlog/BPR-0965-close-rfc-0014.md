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
| Runtime dependency scan shows no WebEngine, browser engine, Node, local server, arbitrary plugin loader, or undeclared network endpoint. | BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| SBOM, license notices, source-offer obligations, and checksums match the delivered artifacts. | BPR-0956 and BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Hardened build settings, static analysis, sanitizers, fuzzers, secret scans, and vulnerability scans pass release policy. | BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Credential vaults, file permissions, cache locations, retention, purge, and uninstall behavior match documentation. | BPR-0953–0955 and BPR-0958–0961; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Diagnostic and crash artifacts pass canary redaction and contain no source/comment bodies by default. | BPR-0953–0955 and BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Corporate security overview clearly states network destinations, data stored, subprocesses, credentials, AI boundary, logging, and removal. | BPR-0963; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |
| Delivered target/dependency graphs and runtime inspection prove the functional core remains Qt/provider-free and all effects are reachable only through the packaged host/composition path. | BPR-0952, BPR-0956 and BPR-0962; exact-candidate procedure in the RFC-0014 verification ledger | Not run for the complete RFC matrix at `d0a8a6dda005af12b3818234f4878a9f48a44146`; inherited evidence remains bounded |

## Verification and execution record

The current owner is `/root/rfc0014_m1`, assignment milestone 1, mandate r1,
run `rfc0014-20260920`, segment `m1-planning`, on `main`.
The available-target implementation exception permits work while earlier RFCs
remain Proposed. It does not waive a package/native requirement or permit
terminal closeout. BPR-0617 and RFC-BPR-0013 remain open; the earliest canonical
closure frontier remains RFC-BPR-0008/BPR-0242.

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
