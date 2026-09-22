---
schema: "implement-rfc.card.v1"
id: "BPR-0963"
title: "Document release and corporate security"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M10 Agent and product documentation impact"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f6af44a5083a3b05a2a963eeb24b5f199406ab65","evidence":{"kind":"artifact","detail":"docs/development/release-packages.md and storage-and-diagnostics.md cover installation/baselines/observed toolchains, native signature/checksum checks, separate source licensing/access/retention, hardening gates, private root/vault permissions, retention/cleanup, migration/recovery and uninstall residuals. Exact-source map in docs/verification/RFC-BPR-0014.md; source-asset preflight and seven composed maintenance tests pass. Pending native/trust/privacy/vulnerability qualification remains explicit."}},{"criterion":"AC2","outcome":"pass","revision":"f6af44a5083a3b05a2a963eeb24b5f199406ab65","evidence":{"kind":"artifact","detail":"Tracked docs/security/corporate-security-overview.md and revised threat/data-flow packet name actual network, local stores, subprocesses/IPC, native and external credentials, AI context/tool boundary, metadata export versus raw logs/crashes, exact removal/residuals and qualified macOS updater. 21 Mac/19 Windows scan findings, M8/M9 gates, exact licensing-v002 acceptance and prospective URL are preserved; no release is published."}},{"criterion":"AC3","outcome":"pass","revision":"f6af44a5083a3b05a2a963eeb24b5f199406ab65","evidence":{"kind":"observation","detail":"Semantic source/no-impact ledger reconciles architecture/typed identity/concurrency/lifecycle, product/UX, build/package/operator and AGENTS guidance. Documentation gate, 64 regressions, seven maintenance tests, 957-card scoped board, unchanged protected artifact hashes and explicit empty product/test/script/package/release-skill/consumer diff pass. Receipts build/m10-final-*-evidence.json; runtime/native/human acceptance remains candidate-bound and unpromoted."}}]
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

Completed under mandate r3 by `/root/rfc0014_m10`, coordinator-implementer rev2.
Documentation candidate `f6af44a5083a3b05a2a963eeb24b5f199406ab65` preserves
implementation source from entry `59226c62058cfa08387a91c872d7208b30275917`.
The immutable RFC base remains `d0a8a6dda005af12b3818234f4878a9f48a44146`.

- AC1: The [package guide](../../development/release-packages.md) names actual
  installation/toolchains, native checksum/signature checks, hardening gates,
  existing release paths and separate corresponding-source/access/retention
  obligations. The [storage guide](../../development/storage-and-diagnostics.md)
  records private roots/vault semantics, metadata export, two-stage cleanup,
  stopped-profile behavior, exact recovery, migration and uninstall residuals.
- AC2: The tracked [corporate packet](../../security/corporate-security-overview.md)
  and revised threat model cover actual network, stored data, subprocesses, AI,
  credentials, diagnostics and removal, with the narrow macOS updater exception.
  Publishing these repository documents is not publishing a release. M8/M9's
  native/trust/privacy gaps, 21 Mac/19 Windows unresolved scan matches and exact
  licensing-v002 approval scope remain explicit.
- AC3: The [semantic impact ledger](../../verification/RFC-BPR-0014.md#milestone-10-agent-and-product-documentation)
  maps architecture/typed ownership, UI, build, operator and agent documents to
  source and no-impact evidence. It corrects stale Windows dispatch/PE, durable
  workspace, remembered-grant, source-delivery, core-SDK and IPC statements.

At exact candidate `f6af44a5`, documentation passes, all 64 verifier regressions
pass, and all seven maintenance core/host/native-file/Qt-scale/public-composition
tests pass (18.77s; public path 16.60s). The source-asset preflight passes against
the unchanged protected `1c429749` macOS app ZIP/SPDX/source ZIP, whose four
principal hashes still match M9. The 957-card scoped board and no-product-change
diff pass. Receipts are `build/m10-final-*-evidence.json`; the
[handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m10.json)
names commands and limits. No required human, native or independent acceptance
is inferred from these documentation checks. Existing protections are unchanged;
no release, credential, remote or package mutation occurred.

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
