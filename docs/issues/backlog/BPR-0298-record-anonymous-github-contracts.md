---
id: BPR-0298
title: Record anonymous GitHub service contracts
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0297]
---

# Record anonymous GitHub service contracts

## Required final state

- A digest-bound attended packet authorizes exact credential-free, read-only,
  bounded requests against named public targets and separately names any
  disposable private-repository creation/deletion authority.
- Sanitized real-service records resolve anonymous GraphQL, REST review
  comments/paging, observed primary/search budget headers, conditional 304
  accounting, and 404 non-disclosure without credentials,
  private content, repository mutation, limit evasion, or GitHub Actions.
- Rate exhaustion and retry refusal use a locally injected synthetic response
  shaped from the recorded stable headers; the live course never exhausts or
  intentionally evades the shared GitHub quota.
- GHES behavior is either directly proved on a designated instance or removed
  from eligibility as irrelevant to the structural zero-request GHES refusal.
- All response bodies and headers are minimized and proven free of credentials,
  cookies, private content, and unstable unrelated fields.
- This card creates `scripts/record_rfc0017_anonymous_contracts.py` with the
  exact packet/response/manifest/output interface seeded in the verification
  ledger. It validates the digest-bound authority and designated targets before
  any request and refuses unlisted hosts, repositories, operations, or outputs.
  Its `--validate-existing` mode performs no traffic and revalidates every
  packet, response, manifest, capture digest, minimization record, and owning
  commit at immutable closeout head.
