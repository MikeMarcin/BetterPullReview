---
schema: "implement-rfc.card.v1"
id: "BPR-0962"
title: "Verify release trust and security boundaries"
status: "Ready"
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

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.

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
