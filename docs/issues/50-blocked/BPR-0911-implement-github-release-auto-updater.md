---
schema: "implement-rfc.card.v1"
id: "BPR-0911"
title: "Implement GitHub release auto-updater"
status: "Blocked"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0946", "BPR-0947", "BPR-0948", "BPR-0949"]
milestone: "Post-Pack v2"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
blocked_by: "private-service-qualification"
---

# Implement GitHub release auto-updater

## Scope and entry

The 2026-09-20 mandate requests implementation of
[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md).
The earlier 2026-09-18 authoring-only mandate remains historical provenance.
Current implementation permits local source/board edits and disposable checks;
live private-service access, release publication and replacement of the developer's
running application are not authorized.

The authored milestones are [[BPR-0946]], [[BPR-0947]], [[BPR-0948]] and
[[BPR-0949]] in order. Preserve
its native authentication boundary, fixed private release source, typed core/host/
Qt allocation, application-wide ownership and signed macOS package constraints.
Canonical dependency and closeout order remains unchanged.

Owner acceptance amendment r3, 2026-09-20, defers native Intel updater validation
to [[BPR-0950]] outside M4 and RFC-BPR-0035 closeout. It must not gate this parent.
The amendment changes AC-07's platform acceptance to Apple Silicon; live
private-release/vault AC-03 and unrelated dependency requirements remain required.

## Acceptance

- Implement AC-01 through AC-08 using the RFC's milestone order; track any split
  card and its exact criterion ownership on the closeout.
- Demonstrate the real menu/Settings-to-host path and native disposable signed
  upgrade, failures, recovery, shutdown and retained review state.
- Record actual results and unresolved target-native evidence on [[BPR-0912]].
  Fixtures alone cannot establish private-service or native package success.

## Verification and execution record

Milestones 1–3 are accepted: [[BPR-0946]] at `42a85cf7`, [[BPR-0947]] at
`54535c1e`, and [[BPR-0948]] at `b7e2df46`. Native signed/notarized Apple Silicon
upgrade, helper timeout, exact healthy receipt, retained synthetic review state
and recovery evidence is bound in the [ledger](../../verification/RFC-BPR-0035.md).
The r2 amendment permits disposable local signing/notarization, not publication.

Milestone 4 source `518c384a285ef3cb873d12ea167488d105aa91b7` fixes duplicate
native update-menu entries and completes operator/recovery/security documentation.
Under r3, [[BPR-0949]] is Done at `d6b5ed528b6646830debf5173de2f77c35c83144`,
with its Apple Silicon native evidence and truthful criterion reconciliation.
This parent is Blocked on `private-service-qualification`: it still lacks
explicitly authorized live private-release/vault evidence for RFC AC-03. Fixtures
exercise selection, revocation, redirects and refusal, but no actual vault-backed release access or
live route/revocation is observed. Intel qualification is separately deferred to
[[BPR-0950]] and is not a blocking dependency. [[BPR-0912]] also retains the
RFC-BPR-0014/canonical-order closure gates; the RFC remains Proposed.
AllTheThings remains read-only provenance; Windows/Linux self-installation remains
outside this card. Only the explicit Intel acceptance amendment changes scope.

Next action: supply explicitly authorized live private-release/vault account and
target evidence, bound to the actual candidate, observed asset route and revocation.
Do not infer that authority from local signing/notarization permission. No live
credential/service operation was attempted in the r3 round. All task commands are
quiescent; the current handoff is [[BPR-0912]]. BPR-0950 remains a separate
nonblocking Backlog follow-up, not a prerequisite of this card.
