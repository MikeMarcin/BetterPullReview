---
schema: "implement-rfc.card.v1"
id: "BPR-0275"
title: "Record RFC-BPR-0016 dependency and application authority"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
---

# Record RFC-BPR-0016 dependency and application authority

Collect digest-bound product-owner decisions before registration or code work.

## Required decisions

- Whether a narrow available-target implementation exception may override the
  RFC's implementation-only dependency on RFC-BPR-0002 Auto-Verified. A grant
  must not waive BPR-0036, change RFC-BPR-0002's status, or authorize RFC-0016
  closeout before BPR-0035 is done.
- GitHub App or OAuth App, with the rejected alternative and tradeoff recorded.
- Exact owner account or organization, installation visibility, requested
  repository/account permissions or OAuth scopes, token-expiry/refresh policy,
  device-flow enablement, and absence of a shipped client secret.
- Whether the attended session authorizes actual registration and bounded
  sanitized wire capture, or only approves the design.

## Required final state

- The exact decision subject and approval statement are committed with matching
  SHA-256 identities.
- Each accepted statement is preserved byte-for-byte as `operator-response.md`;
  an exact-statement validation record names the packet, subject digest,
  validation result, and commit. A packet template alone never satisfies a gate.
- Any granted prerequisite exception updates the RFC and BPR-0276 dependency in
  the same commit; a denied exception leaves BPR-0035 intact.
- No client identifier, endpoint response, token, device/user code, or product
  enablement is introduced by this card.

## Decision packets

- `docs/verification/reviews/rfc0016-prerequisite-exception-v001/`
- `docs/verification/reviews/rfc0016-github-app-registration-v001/`

Both decisions are independent. A response to one grants no authority from the
other.

The packet files remain immutable decision-time evidence, including their
original `pending` labels. Canonical outcomes are the adjacent byte-preserved
responses and validation records.

## Accepted decisions

- The available-target implementation exception is accepted exactly as bound
  by subject
  `sha256:d0d021ce54207c4ee44771a3e1b4e1bf787e4ff9aaafa99f15fa52afa6c9647b`.
  BPR-0035/BPR-0036 and RFC-BPR-0002 closeout remain mandatory.
- The selected application is the exact approved GitHub App owned by
  `MikeMarcin`, with the exact installation, permission, refresh, registration,
  and bounded sanitized-capture authority bound by subject
  `sha256:d3cfb906170799ee7c90a8290cabee1b1e7e6afef40937a08f7b21bd5b42d178`.
  Repository mutation, release/distribution, GHES registration, and
  secret-bearing evidence remain unauthorized.

## Result

- Response/authority commit:
  `e36c2b4aff66f59fe17d3cb1349e13071543bd53`.
- Validation-record commit:
  `7f74b9f404d858991ee0a96421e7da3829bcd891`.
- Both records replayed with `OPERATOR_RESPONSE_VALID`; the record commit
  differs from its authority parent only by the two adjacent
  `validation-record.json` files.
- The prerequisite exception, RFC eligibility wording, and BPR-0276 dependency
  changed atomically with the preserved responses. No registration, traffic,
  credentials, codes, endpoint response, or product enablement occurred.
