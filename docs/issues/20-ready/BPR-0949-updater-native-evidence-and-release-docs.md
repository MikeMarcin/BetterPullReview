---
schema: "implement-rfc.card.v1"
id: "BPR-0949"
title: "Qualify updater packages and document release security"
status: "Ready"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0948"]
milestone: "RFC-0035 milestone 4"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Qualify updater packages and document release security

Fourth authored milestone of [[BPR-0911]]. Obtain target-native upgrade evidence
and complete release/security/installation/recovery documentation. Windows/Linux
self-installation remains future scope; manual fallback is required now.

## Acceptance

- AC-01: An older signed package upgrades to the offered signed package on Apple
  Silicon, relaunches with retained user state, and reports truthful
  progress/result. Bind artifact digests, candidate revisions and actual platforms.
- AC-02: Document authority, destinations/redirects, bounds, helper inventory,
  signing/notarization, state ownership, cleanup and recovery. Explicitly amend
  RFC-0014 and packaging's no-updater statements only for qualified macOS packages.
- AC-03: Reconcile every RFC criterion with direct evidence; unavailable native,
  human or external authority stays open. No live private access or publication is
  implied. Finish [[BPR-0911]] only once its complete acceptance is met.

[[BPR-0912]] owns dependency-ordered closeout. RFC status cannot bypass RFC-0014.

Owner acceptance amendment r3, 2026-09-20: Intel hardware qualification moves to
[[BPR-0950]] outside this card, M4 and RFC-BPR-0035 closeout. It is deferred, not
observed passing. AC-01 is narrowed only to Apple Silicon; AC-02/03 and all other
RFC criteria/dependencies remain unchanged. AC-03 requires honest reconciliation,
not closing the parent while its live private-release/vault criterion is pending.

## Verification and execution record

### Resumed acceptance reconciliation (mandate r3)

Run `rfc0035-20260920`, segment `m4-post-block-intel-deferral`; sole owner
`/root/rfc0035_m4_resume` on `main` from
`fe728286093326839b63f49f83487d61b5d88ad8`. Product source is unchanged from M4
`518c384a285ef3cb873d12ea167488d105aa91b7`; the signed Apple Silicon install
candidate remains M3 `b7e2df46cc178bbcff88b49dd2170ecf27a8c764`, package source
`5db1d104a2436a2b0df811ac2e40cff0c58c2319`. New native Intel or live credential
observations are not claimed.

| Criterion | Evidence and disposition under r3 |
|---|---|
| AC-01 | Pass for amended Apple Silicon scope: reuse BPR-0948's signed 0.0.350→0.0.352 upgrade, exact receipt, retained state, timeout and recovery with ledger digests. The native Cocoa menu correction remains separately tested at `518c384a`; its source does not alter the qualified installer/helper path and is not claimed to be in the signed 0.0.352 archive. Intel execution is deferred to BPR-0950, not passed. |
| AC-02 | Pass: operator, recovery, security, package and scoped RFC-0014 updater documentation remain verified at `518c384a`; current cross-references separate the Intel follow-up from outstanding live/full-RFC acceptance. |
| AC-03 | Pass: the closeout maps all eight amended RFC criteria to actual evidence. RFC AC-03's live private-release/native-vault/revocation course is still pending on BPR-0911/0912; RFC-0014 and canonical gates remain explicit. This card's completion does not close those parents. |

The shortest composed public-path check passed again before this amendment.
Final focused checks and acceptance commit are recorded in the ledger and
[[BPR-0912]]. The prior blocked record below remains historical evidence under
r2, superseded only by this explicit owner amendment and current dispositions.

### Prior blocked record (mandate r2)

Run `rfc0035-20260920`, segment `m4-native-closeout`, mandate `r2`; sole owner
`/root/rfc0035_m4`. Initial HEAD `c8b16090473c694d70470a1e86f51e952b1d22a8`;
claim `79217b124758880524ac6a7e81471b1b680bee37`.
Source candidate `518c384a285ef3cb873d12ea167488d105aa91b7`.

| Criterion | Evidence and disposition |
|---|---|
| AC-01 | **Pending.** Reuse the unchanged signed Apple Silicon 0.0.350→0.0.352 upgrade/timeout/receipt/recovery protocol from [[BPR-0948]], source `b7e2df46`, with exact digests in the [ledger](../../verification/RFC-BPR-0035.md). No Intel Mac is available; universal slices do not prove Intel execution. M4's menu correction has native Cocoa developer-test evidence, not a newly signed package. |
| AC-02 | **Pass at the source candidate.** Added [operator/recovery guidance](../../development/application-updates.md) and [security inventory](../../security/release-updater.md), linked package/component/user documentation, and narrowly amended RFC-0014 and packaging's no-updater rule. Explicit signed-helper recovery is documented; no recovery wizard or attended usability approval is claimed. |
| AC-03 | **Pass for reconciliation, not full RFC acceptance.** [[BPR-0912]] maps all eight criteria to exact source/native evidence and gaps. Actual private-release routing, native vault acquisition and live revocation remain unobserved without explicit account/target authority. Parent BPR-0911 and closeout BPR-0912 remain Backlog. |

The duplicate two-window **Check for Updates…** defect is fixed with one
application-lifetime AppKit item. The native public-menu test counts exactly one,
invokes it and observes real Updates status across both windows, Settings,
last-window closure and reopening. The offscreen course also proves idempotent
attachment. Final updater integration passes 9/9; boundary scans/contracts,
tests-disabled application build, focused format/tidy and documentation checks
are recorded with initial failures and limits in the ledger.

This card is **Blocked**, not Done, on `intel-and-private-service-qualification`.
The current host is Apple Silicon macOS 26.6.2; no live private-service or native
vault credential was accessed. RFC-0014 is still Proposed and canonical closure
starts at RFC-0008, independently gating M4's exit. Continue with an authorized
Intel hardware course and explicitly authorized live account/target evidence,
then reconcile dependency-ordered closeout. No tags, pushes, publication or
installed/developer-app replacement occurred or is implied. No attended session,
review server or protected candidate exists; commands are quiescent at handoff.
