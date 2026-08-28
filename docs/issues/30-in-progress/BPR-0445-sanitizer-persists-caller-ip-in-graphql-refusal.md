---
id: BPR-0445
title: The sanitizer persists the caller's IP in the anonymous GraphQL refusal
status: In Progress
rfc: RFC-BPR-0017
depends_on: []
---

# The sanitizer persists the caller's IP in the anonymous GraphQL refusal

The first authorized RFC-0017 anonymous capture ([[BPR-0298]], 2026-08-23,
evidence at `4d250ab6`) recorded GitHub's real unauthenticated GraphQL
contract: a **403 zero-quota refusal** (`x-ratelimit-resource` `graphql`,
limit 0, remaining 0, used 0), not the 401 the course expected. The refusal
body is GitHub's rate-limit message template, and that template embeds the
caller's public IP address.

`scripts/record_rfc0017_anonymous_contracts.py` has no rule for that:
minimization drops headers outside an allowlist, but a body is persisted as
canonical JSON of the bytes received, and the forbidden-byte patterns cover
credentials, not addresses. The capture therefore could not be committed —
BPR-0298's required final state says bodies are proven free of credentials,
cookies, private content, and unstable unrelated fields, and a caller IP is
both private and unstable. The file was deleted uncommitted and the course
manifest trimmed to the four validated courses. The refusal contract itself
is still on the committed record: the `rate-limit` capture's body carries
`graphql {limit: 0, remaining: 0}`.

## Required final state

- An explicit, tested minimization rule admits the unauthenticated GraphQL
  refusal body and normalizes the caller address to a fixed placeholder,
  recording in the capture record that the normalization occurred. The rule
  is scoped to the `graphql_unauthenticated_refusal` operation; every other
  body remains byte-faithful.
- Explicitly synthetic hostile fixtures prove the rule: IPv4 and IPv6 address
  forms it must normalize, and shapes it must still refuse. The raw live body
  was deliberately not retained and is not reconstructed.
- The `graphql-refusal` course's expected status is corrected to the observed
  contract (403) wherever the course is named.
- The repaired sanitizer is proven hermetically against the synthetic refusal
  shape **before** any fresh authorization is spent — the [[BPR-0276]] lesson.
- One recapture of the single `graphql-refusal` target runs under a fresh
  exact operator authorization (the v001 approval was single-issuance and is
  consumed), the manifest returns to five courses, and `--validate-existing`
  reports `RFC0017_ANONYMOUS_CONTRACTS_VALID:5`.

## Non-goals

- No change to header minimization, the authority schema, or the allowed
  request and capture-output bounds. The one deliberate filesystem-boundary
  addition is the fixed repository-wide attempt-receipt registry needed to
  enforce single issuance across copied packet and output directories; it
  stores only bound digests, course identity, and schema metadata.
- Not the conditional-304 or synthetic-exhaustion authority; [[BPR-0298]]
  keeps those.

## Unattended sanitizer repair — 2026-08-24

The hermetic phase is complete. The recorder now admits only the observed
GraphQL refusal contract: status 403 with the exact zero-quota `graphql`
headers, a rate-limit message, and exactly one syntactically valid caller
address. It replaces that address with `[normalized-caller-address]`, adds the
GraphQL-only `caller_address_normalized: true` marker, and hashes the persisted
normalized body rather than the raw response. Other operations retain their
original byte-faithful body-digest rule.

Synthetic IPv4 and IPv6 courses prove the positive rule. Negative courses
refuse missing, malformed, multiple, or residual addresses; CIDR and zone
suffixes; unsupported status or quota headers; non-message bodies; marker,
placeholder, digest-consistency, and retained-header defects in the normalized
GraphQL record; duplicate GraphQL JSON members, repeated manifest courses, and
any marker on another operation. The 82 recorder tests and `py_compile` pass,
and the four committed v001 captures still report
`RFC0017_ANONYMOUS_CONTRACTS_VALID:4`. No network request ran, and no packet,
response, course manifest, or capture changed in this phase.

The repeated-course guard closes a bypass of the existing one-request bound;
it does not add or alter any allowed host, operation, target, method, or
authority field. A response must now bind the exact packet SHA-256 and contain
exactly one quoted approval. Capture additionally requires the exact response
byte grammar, a bound digest of the current recorder, and an exact packet-bound
course-selection block matching the course manifest. This makes the approval
and manifest binding bidirectional and prevents a different target or status
from being substituted before the first request. Capture then atomically writes
a repository-wide attempt receipt keyed by packet SHA-256 before traffic. It
flushes and syncs the receipt and its registry directory before the request;
first creation also syncs the registry's parent. A host or filesystem without
that durability refuses before traffic. The recorder also reserves each
destination before its request, so neither the consumed v001 packet, copied
packet directory, nor alternate safe output directory can spend authority
again. The existing safe-descendant output bound remains unchanged.

The request opener now installs an explicit empty proxy handler and refuses
every redirect. Ambient proxy credentials therefore cannot be injected after
the recorder's header check, and a 30x cannot escape the named GitHub host or
change the authorized method.

The final code-simplifier checkpoint found no safe reduction. Address
detection, normalization, persisted-body validation, authority/response
binding, durable one-shot reservation, and the bounded request opener are
separate fail-closed boundaries rather than duplicate policy. The direct
security counterexamples in the test module intentionally mirror those
boundaries; abstracting them further would obscure which failure is proven to
occur before traffic.

## Independent critical review — 2026-08-24

A read-only capability-critical review tried to falsify the unattended repair,
including malformed address boundaries, duplicate or reordered authority and
course-selection JSON, response and single-issuance bypasses, crash-order
durability, copied packet/output paths, ambient proxy credentials, and redirect
escape. The findings were repaired and replayed. The final verdict reports no
blocking findings and no verification failures: all 82 focused tests,
`py_compile`, the four-capture no-write
validation, the documentation gates, and `git diff --check` pass without a
change to any v001 packet, response, manifest, or capture.

The review's one high-risk residual is the BPR-0298 immutable-evidence
authentication obligation described below. It blocks eventual immutable
`VALID:5` closeout, not committing this operation-scoped privacy repair. The
live 403 shape also remains deliberately unverified until fresh attended
authority exists.

Critical review also reproduced a pre-existing BPR-0298 limitation outside
this repair: non-GraphQL records retain the digest of the raw response while
persisting a canonicalized body, so `--validate-existing` cannot correlate a
later edit of that minimized body with the raw digest. The new GraphQL record
does correlate its persisted body and digest and rejects an inconsistent edit,
but a self-consistent body-and-digest substitution still needs external
authentication. BPR-0298's still-pending immutable closeout owns authentication
of every minimized record against its owning commit; BPR-0445 does not change
the older records' schema or artifacts.

## Attended packet — 2026-08-24

The fresh packet is prepared at
`docs/verification/reviews/rfc0017-anonymous-probe-v002/operator-packet.md`.
Its SHA-256 is
`8ff060e82c04f30010ad11e5410200d8797d8f1c5b08557768ded86ed0eafdd0`
(7,144 bytes). Its packet-bound selection and manifest name exactly one
`graphql-refusal` course with expected status 403, and its canonical authority
identity equals v001 so a successful fifth capture can join the four immutable
records under one authority.

No operator response, capture, or attempt receipt exists, and no network
request ran. The card is parked for the operator's fresh exact response. The
consumed v001 response is not reused. Approval permits one attempt; rejection
or any non-exact response leaves the live phase unexecuted.
