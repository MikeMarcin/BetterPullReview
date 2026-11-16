---
schema: "implement-rfc.card.v1"
id: "BPR-0911"
title: "Implement GitHub release auto-updater"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0946", "BPR-0947", "BPR-0948", "BPR-0949"]
milestone: "Post-Pack v2"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"},"owner_amendment":{"date":"2026-11-16","source":"owner decisions on the 2026-11-15 hygiene audit, relayed by the coordinator","quote":"you decide","decision":"amend RFC-BPR-0035 AC-03 to what is achievable without a named private-release target; close if satisfied","rfc_section":"docs/rfcs/RFC-BPR-0035-github-release-auto-updater.md#owner-amendment-2026-11-16--ac-03-without-a-private-release-target"}}
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

## Hygiene audit 2026-11-15

Verdict: **UNSURE**. Blocked since 2026-09-21 on live private-release and vault evidence (RFC-BPR-0035 AC-03); there is no wizard or session for it. The text still says BPR-0912 keeps the RFC-BPR-0014 and canonical-order gates, which were removed 2026-09-27. Question for the owner: authorize a named private-release target for AC-03, or amend AC-03?
This is a read-only tracker audit: it changes no status, folder or metadata.

## Owner decision 2026-11-16: AC-03 amended; Done

On the hygiene audit's question (authorize a named private-release target for
AC-03, or amend it), the owner, 2026-11-16: "you decide".

**Decision: amend AC-03, not name a target.** The block was evidence of a
vault-backed private release, but the distribution repository's releases are
public now, and since [[BPR-1669]] the updater reads them through a
credential-free public lease unless an accepted GitHub.com account is chosen.
A private target named only to qualify this criterion would test a path the
product no longer ships, and would put a credential in play for nothing. The
amendment keeps every safety obligation of AC-03 and drops only the live
vault-backed private route; it is recorded in
[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md#owner-amendment-2026-11-16--ac-03-without-a-private-release-target)
and in [[BPR-0912]]'s checklist.

**Amended AC-03, judged.**

- Selection, revocation, redirect and refusal: the native release-only
  factory, the Qt choice and revocation courses and the 24 TLS scenarios
  (`54535c1e`, `518c384a`, fixture transport), as recorded above.
- The public lease: `release_update_transport_contract` (identity, kind,
  revocation, zero identity refused) at `bc856835c` ([[BPR-1669]]); the
  transport sends no Authorization header with an empty credential.
- Live, 2026-11-16, anonymous, no credential sent: `GET
  https://api.github.com/repos/MikeMarcin/BetterPullReview/releases/latest`
  answered 200 with release `443822880`, tag `0.4.2`, `draft: false`,
  `prerelease: false`, and one exact universal ZIP
  (`BetterPullReview-0.4.2-macos-universal.zip`, asset `742299551`, 77,872,639
  bytes, digest `sha256:a1ba8d00…`). The asset route with binary Accept
  answered 302 to `https://release-assets.githubusercontent.com/…`, the
  security boundary's allowlisted origin (signed query not recorded).

AC-03 as amended passes. AC-01, AC-02 and AC-04 to AC-08 were already
recorded on [[BPR-0912]]; Intel stays the nonblocking [[BPR-0950]]. The text
above that says [[BPR-0912]] keeps the RFC-BPR-0014 and canonical-order
gates is out of date: RFC-BPR-0014 is Auto-Verified and the canonical-order
rule was removed on 2026-09-27. This card is Done. [[BPR-0912]] moves to
Ready and re-runs the updater rows on current `main` before RFC-BPR-0035
closes.
