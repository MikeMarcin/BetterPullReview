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
  final distribution trust, complete supported artifacts, owner/legal licensing
  and corresponding build/relink obligations are not approved.
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
verification, legal disposition and the missing native courses. The attended
licensing question is narrower than release acceptance. No tag, push, upload,
Developer ID signing or notarization was performed.

## M2 security allocation

### M9 post-review correction in progress

Fresh sole owner `/root/rfc0014_m9_postreview` resumes only M9 at
`6ee88ac8afdc30543c0c6ea42e2a596b7556c1c4`, mandate r3 unchanged. Session
`cbd48b91000b00f1e72e1ad62c7c384c` handed back at sequence 465. Rejection
`626562d7-65a5-4eb3-842b-30b2573a09e5` applies only to licensing v001,
fingerprint `2ec3475e1eed532ab1455783ae8553b823f6f7ffb005ee8b057ce1be5119c577`.
The requested correction moves corresponding source to a separate version-bound
release asset, retains exact references/notices and replacement instructions in
the application, and requires equivalent recipient access and retention. It
does not approve legal terms, security, trust or publication.

The implementation plan is to update the existing package/release drivers,
bind the sidecar digest and release location in installed source provenance,
reject absent/substituted/embedded archives in audits, test the public package
and publish preflight locally, and prepare only the affected licensing question.
The old protected candidates remain untouched. The review server PID 49181 is
the only inherited task command; no other writer is assigned. Grype's 21 Mac/19
Windows failures and every M8/M9 native/independent obligation remain pending.
Next action: implement and verify this bounded packaging correction offline.

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
