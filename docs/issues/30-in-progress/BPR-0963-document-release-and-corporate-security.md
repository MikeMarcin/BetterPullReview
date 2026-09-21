---
schema: "implement-rfc.card.v1"
id: "BPR-0963"
title: "Document release and corporate security"
status: "In Progress"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M10 Agent and product documentation impact"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Document release and corporate security

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Complete installation, supported baseline/toolchains, package/signature verification, dependency licensing/source offers, hardening, credential/storage permissions, retention/purge/uninstall, migration and operator recovery documentation.
- AC2: Publish the corporate security overview and threat/data-flow packet covering network destinations, stored data, subprocesses, credentials, AI boundary, diagnostics/logging, removal and the qualified macOS updater exception.
- AC3: Update affected architecture/build/operator/agent documents against exact source evidence; preserve pending native/security facts and no-impact dispositions in the RFC ledger.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.

## M2 documentation allocation

The [allocation and semantic document map](../../architecture/release-readiness.md)
names the actual dependency/helper closure, G1–G9, data/credential authorities,
diagnostic privacy/retention and removal boundaries. Reconcile those against
final package observations in README, packaging/build/operator guides, product/
system architecture, conformance/release gates, threat model and corporate
packet. Historical token-cleanup/platform claims must stay candidate-bound:
remembered device grants now persist and are removed through their exact owner.
Document the native baseline/toolchain/CPU requirements, source offers and the
separate Mac updater exception. A tracked corporate packet is not authorization
to publish a package or transmit diagnostic data.
