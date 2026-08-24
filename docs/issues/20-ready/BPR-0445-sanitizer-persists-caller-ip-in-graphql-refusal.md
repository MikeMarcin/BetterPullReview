---
id: BPR-0445
title: The sanitizer persists the caller's IP in the anonymous GraphQL refusal
status: Ready
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

- No change to header minimization, the authority schema, or the outer
  bounds the script enforces.
- Not the conditional-304 or synthetic-exhaustion authority; [[BPR-0298]]
  keeps those.
