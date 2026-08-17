---
id: BPR-0038
title: Decide query-preserving REST redirect policy
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0026, BPR-0027]
completed: 2026-07-30
---

# Decide query-preserving REST redirect policy

The BPR-0026 native HTTPS transport intentionally rejects every HTTP redirect
whose target contains a query. Provider pagination is separate: the bounded
provider may compile and follow validated REST `Link` continuation URLs with
queries, but an HTTP redirect cannot rewrite the exact request URI after a
credential-bearing request has begun.

## Decision

Keep query-bearing HTTP redirect targets rejected. RFC-BPR-0002 has no
normative requirement or retained direct evidence that needs one, while
acceptance would add query parsing, equivalence, ambiguity, byte-bound, and
credential-forwarding policy to the transport trust boundary. The current
rule is smaller and fail-closed: only a canonical query-free HTTPS target on
the exact validated REST authority and inside the selected API namespace can
be approved, and every accepted hop reconstructs a fresh request.

A future RFC may reconsider the rule only with direct public-contract
evidence for a required redirect and a design that preserves the exact
compiled request semantics. That future work must revalidate the complete
target, forbid cross-authority and GraphQL redirects, bound redirect and query
bytes, reject duplicate or ambiguous query members, and prove credentials and
bodies never reach a rejected target. It is not RFC-BPR-0002 closeout work.

## Evidence

- The host-profile redirect contract directly submits
  `https://api.github.com/user?next=x`, requires typed rejection, and proves a
  rejected target cannot mutate the validated profile authority.
- The native transport disables automatic redirects and sends a fresh
  credential-bearing request only after the target passes that contract, so a
  query-bearing target cannot become a network hop.
- The BPR-0026 15-scenario loopback matrix directly proves that a canonical
  query-free same-authority REST redirect receives a fresh exact request,
  while cross-authority and GraphQL redirects fail closed. The complete
  macOS and Linux target-native BPR-0026 evidence remains unchanged.
- The final macOS BPR-0027 full `dev` matrix passed 35/35, including the
  hostile host-profile contract, and the documentation and immutable
  source-pack gates passed. GitHub Actions supplies no evidence and has no
  project role.
