---
id: BPR-0275
title: Record RFC-BPR-0016 dependency and application authority
status: Backlog
rfc: RFC-BPR-0016
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
- Any granted prerequisite exception updates the RFC and BPR-0276 dependency in
  the same commit; a denied exception leaves BPR-0035 intact.
- No client identifier, endpoint response, token, device/user code, or product
  enablement is introduced by this card.

## Pending packets

- `docs/verification/reviews/rfc0016-prerequisite-exception-v001/`
- `docs/verification/reviews/rfc0016-github-app-registration-v001/`

Both decisions are independent. A response to one grants no authority from the
other.

Status: `awaiting-user` for both exact statements.
