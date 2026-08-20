---
id: BPR-0298
title: Record anonymous GitHub service contracts
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0297, BPR-0405]
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

## The tooling half left this card — 2026-08-19

This card was parked under `awaiting-live-github`, and roughly half of its
required final state never needed the network: the recorder script, its
digest-bound authority packet, the designated-target allowlist, the sanitizer,
and the no-traffic `--validate-existing` mode. `docs/verification/RFC-BPR-0017.md:78`
gates the live course on an operator statement recorded from an
`operator-packet.md` that does not exist yet, so the tooling is not merely
separable from the capture — it is strictly upstream of it.

[[BPR-0405]] takes that half and is in `20-ready/`. This card keeps what is
genuinely live: sanitized real-service records for anonymous GraphQL, REST
review comments and paging, rate and conditional-request behaviour, and
404 non-disclosure, plus the GHES disposition.

It moves to `10-backlog/` rather than staying parked, because the honest answer
to "what is in the way" is now another card rather than a service. `40-parked/`
is not a deferring lane and takes no card with an unmet dependency; `10-backlog/`
means exactly "waiting on another card", which `depends_on` now records.

One question travelled with the split and is now settled on [[BPR-0405]]. The
synthetic rate-exhaustion fixture is specified as "shaped from the recorded
stable headers", and "recorded" means recorded here: the bullet above it names
"**observed** primary/search budget headers" as live output using the same verb,
and the reason the fixture is synthetic at all — "the live course never exhausts
or intentionally evades the shared GitHub quota" — only holds if the real headers
were observed unexhausted and the exhausted one built from their shape.

So [[BPR-0405]] owns the injection mechanism and its schema, seeded with the
documented header names, and **this card owns the fixture's values**. That is an
interpretation of this card's text, not a measurement; correct it there if a
better reading appears.
