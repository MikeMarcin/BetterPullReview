---
schema: "implement-rfc.card.v1"
id: "BPR-0962"
title: "Verify release trust and security boundaries"
status: "In Progress"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957"]
milestone: "M9 Security and boundary verification"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Verify release trust and security boundaries

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Independently verify Windows signatures, macOS signatures/notarization and Linux package signatures against each exact delivered artifact; reconcile checksums, SBOM, transitive dependency provenance, notices and source-offer obligations.
- AC2: Inspect packaged dependencies, sockets, process launches, plugin/library resolution and network destinations; verify only declared first-party composition owns effects, core remains Qt/provider-free, and no WebEngine/browser/Node/server/arbitrary plugin loader is introduced.
- AC3: Define and pass the release hardening, static analysis, sanitizer, fuzz, secret/vulnerability-scan and diagnostic/crash-canary policy; retain exact failures, tool/database versions and artifact identities. Missing native environments or signing authority remain pending owner/operator obligations.

## Verification and execution record

M9 owner `/root/rfc0014_m9`, mandate r3, coordinator-implementer rev2, on main.
Claim `8d75ff697d8aae83a54a6396fa7a71a519ec56ba`; immutable RFC base remains
`d0a8a6dda005af12b3818234f4878a9f48a44146`. M7 is the accepted prerequisite;
M8's blocked native work and exact human decisions are preserved.

The [M9 ledger](../../verification/RFC-BPR-0014.md#milestone-9-security-and-boundary-verification),
[policy](../../security/release-policy.md), [scan dispositions](../../verification/rfc0014-m9-security-evidence.json)
and [handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m9.json)
bind source, tools, artifacts, failures and remaining obligations. Producer
`9409ff618c0e8c567fe266195ec8228852b18485` supplies new offline macOS and unsigned
Windows packages; verifier `f72013b0d245c3ef5e18b65eba7f31f250838b24` additionally
binds complete first-party PE maps, Qt pre-sign SPDX to valid publisher signatures,
exact deployed inventory/source/notices and actual emitted hardening.

- AC1 pending: mechanical inventories pass for both candidates; independent
  final distribution trust and complete supported artifacts remain pending.
  The owner approved licensing/source/build/relink obligations only for the
  revised macOS licensing-v002 inventory at hand-back 470, as recorded below;
  revised Windows/Linux artifacts and their corresponding obligations remain open.
- AC2 pending across the matrix: tests-disabled macOS relocation, loader refusals,
  real MCP boundary, five hostile IPC frames and second-launch handoff pass.
  Windows extracted MSI maps/anchors and native inspectors pass. Actual Windows
  public runtime/vault/network course and declared Linux/Intel cases remain open.
- AC3 fails release policy: Grype 0.119.0/fresh v6.1.9 DB reports 21 macOS and
  19 Windows matches, including High PCRE2 and Windows zlib. No suppression or
  security-owner waiver exists. Focused tidy, 200,000 combined ASan/UBSan fuzz
  iterations and core/host/storage/public regressions pass. Exact secret findings
  are benign prose/identifiers/synthetic tests, with raw scanner failures retained.
  OS crash reports omit synthetic environment canaries; actual resident review/
  token crash material and full native vault/root/privacy qualification stay pending.

Neither authored exit is accepted. Required next actions are dependency
remediation or independently reviewed applicability, final authorized trust
verification, remaining artifact-specific licensing and the missing native
courses. The attended
licensing question is narrower than release acceptance. No tag, push, upload,
Developer ID signing or notarization was performed.

## M9 post-review source distribution correction

Fresh sole owner `/root/rfc0014_m9_postreview` resumes only M9 at
`6ee88ac8afdc30543c0c6ea42e2a596b7556c1c4`, mandate r3 unchanged. Session
`cbd48b91000b00f1e72e1ad62c7c384c` handed back at sequence 465. Rejection
`626562d7-65a5-4eb3-842b-30b2573a09e5` applies only to licensing v001,
fingerprint `2ec3475e1eed532ab1455783ae8553b823f6f7ffb005ee8b057ce1be5119c577`.
The requested correction moves corresponding source to a separate version-bound
release asset, retains exact references/notices and replacement instructions in
the application, and requires equivalent recipient access and retention. It
does not approve legal terms, security, trust or publication.

Commit `1c429749aff4c09c6dc88da930d3344cd2d33c36` updates the existing
macOS and native CPack drivers. Installed provenance binds the sidecar version,
platform, producer, exact URL and digest; notices and source offers name it.
Audits reject absent, substituted, stale or embedded archives. The existing
publication script verifies the actual app ZIP/SPDX/source relationship before
remote operations and requires eight assets including source and its checksum.
The access policy includes original-channel fallback, corporate/offline copies
and indefinite retention until all applicable obligations end. Qt replacement
and local resealing instructions are preserved.

Offline macOS preparation, all 34 tests in four focused CTest contracts, the
exact security audit, publication source preflight, checksum/ad-hoc seal checks,
and guarded tests-disabled public package course pass. All prior notices and
seven upstream source payloads are retained; installed footprint is 92,536 KiB
versus 159,472 KiB before the correction. Windows/MSI and Linux/DEB/RPM driver
contracts pass with fixtures; revised native artifacts remain pending.

The [ledger](../../verification/RFC-BPR-0014.md#m9-post-review-source-distribution-correction)
and handoff bind the new candidate and the separate source asset. Only licensing
v002 is prepared for the remaining owner/legal obligation. The old rejected
v001 and M8 candidates stay protected and unchanged. Grype's 21 Mac/19 Windows
failures and all trust/native/privacy obligations remain pending; this card and
both M9 exits remain unaccepted. No publication, remote release mutation,
Developer ID signing or notarization occurred. Next action: hand back the exact
licensing decision; the coordinator assigns a fresh M9 engineering round.

## M9 licensing-v002 decision finalization

Fresh sole implementer `/root/rfc0014_m9_finalize` resumed at clean
`63c786684ebb1c66b314341f6ee4b66a1d569225`, with mandate r3 and
coordinator-implementer rev2 unchanged. Session
`cbd48b91000b00f1e72e1ad62c7c384c` saved human accept
`42661017-3d13-43ba-8366-7ac3bd32c0db` at event 469 and handed back at 470.
It applies only to `rfc0014-m9-licensing-v002`, fingerprint
`da5616bda5bc5727e9eb8b8cce756357300a9d5102592ca34698ec48aa8dce80`,
producer `1c429749aff4c09c6dc88da930d3344cd2d33c36`.

The existing owner/legal criterion is satisfied for that exact macOS inventory:
the separate version-bound corresponding-source asset, installed exact
reference/digest and notices, equivalent recipient access, continued retention,
and preserved build/relink/replacement instructions are approved. The raw note
is empty; no additional supporting opinion or authority is inferred. This
decision is neither a vulnerability disposition nor security, independent trust,
native-matrix or release approval. The prospective source URL is still
unpublished; fulfilling recipient access and retention remains a distribution
obligation. No approval transfers to revised Windows/Linux artifacts.

The actual app ZIP/SPDX/source preflight and all reviewed artifact digests pass
again without changing candidate bytes. The
[finalization ledger](../../verification/RFC-BPR-0014.md#m9-licensing-v002-decision-finalization)
and handoff capture the exact decision and server retirement. Historical v001,
M8 and unrelated protections remain intact. BPR-0962 stays Blocked on
`release-security-inputs`; both M9 exits remain pending, including the unchanged
21 macOS / 19 Windows Grype failures and trust/native/privacy gaps. Independent
M10/BPR-0963 is next through accepted M7; this owner stops at M9.

## M9 dependency remediation resumption

Sole implementer `/root/rfc0014_m9_finalize` resumes AC3 engineering from clean
`08d129d0b04d2cac2649013b16b315cf7ccf929e`, mandate r3 and
coordinator-implementer rev2 unchanged. M10 and M11 are integrated; their
acceptance does not resolve M9. The scope is supported pinned dependency
remediation, exact offline package rebuild/rescan, and primary-source
applicability evidence for every existing finding. No suppression, security
waiver, native/trust acceptance or publication is authorized.

Execution plan: preserve all six attended protections and recorded decisions;
upgrade the official Qt provider/source pin together where a released fix is
available; exercise the package/source/public path and focused Qt-dependent
contracts; bind unfiltered scans and per-finding evidence to the new artifact;
then return this card to Blocked if independent decisions or unavailable
upstream fixes remain. SQLite is already at the latest published 3.53.4.
Historical packages and approvals remain immutable and do not transfer to
changed inventories. M10/M12 are out of scope.

## M2 security allocation

The [source allocation](../../architecture/release-readiness.md) binds all nine
canonical public checks and G1–G9. Verify exact final package/helper bytes,
static/transitive SBOM coverage, effective emitted hardening, loader policy,
vault/root behavior and native crash-artifact canaries independently of build/
sign success. Write and apply an explicit tool/database/findings release policy
for static analysis, sanitizer/fuzzer, secrets and vulnerabilities.

G9 requires explicit reconciliation: the existing profile-activation adapter
uses an owner-authenticated bounded AF_UNIX listener, while the old RFC runtime
criterion says no local server. M2 did not waive or rewrite that criterion.
Inspect owner/peer/frame/lifetime/no-network-service properties; a TCP-only scan
cannot omit this IPC. Preserve any required acceptance/mandate decision rather
than claiming the current implementation already passes. Existing updater/live/
Intel gates remain separate; publication is not authorized.

Owner amendment r2 directs reuse of the existing release script's macOS
signature/notarization/Gatekeeper and dependency checks, followed by independent
exact-artifact qualification. The project `publish-release` skill preserves a
separate explicit user request before tag/push/release creation. Neither its
existence nor BPR-0908's skipped-notarization run satisfies this card; retain
the full supported matrix and SBOM/license/privacy/hardening obligations.
