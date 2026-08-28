---
id: BPR-0298
title: Record anonymous GitHub service contracts
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0297, BPR-0405, BPR-0445]
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

## The upstream half landed, so this is parked rather than queued — 2026-08-20

[[BPR-0405]] is done: `scripts/record_rfc0017_anonymous_contracts.py` exists
with the interface the ledger commits to, its 36 hermetic tests pass, and
`docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-packet.md`
carries the authority block the script parses. [[BPR-0297]] was already done.

So no card is in front of this one any more, which is what `10-backlog/` meant.
What is in front of it is a person and a service, which is `40-parked/` under
`awaiting-live-github`.

Two things must happen in order, and neither is a card:

1. The operator returns the packet's exact statement. Until then the script
   refuses to issue a single request — that refusal is tested, not asserted.
2. The five authorized requests run once, against a 60-per-hour unauthenticated
   budget, and their sanitized responses are written under the packet's own
   directory.

### What this card still owns, unchanged

The conditional-`304` accounting and the synthetic rate-exhaustion fixture are
still this card's, for the reason settled at `2e998d7`: both need values only
the first capture can supply — a real `ETag`, and real budget headers to shape
an exhaustion from. [[BPR-0405]]'s packet says so rather than implying its
authority covers them.

One thing to check before running: the packet names `octocat/Hello-World` pull
request 1 and states plainly that it could not confirm the pull request is live,
because confirming it would have required the request being authorized. If it is
absent the capture records a 404 and the target should be replaced with a public
pull request of the operator's choosing — which changes the authority digest,
and therefore invalidates any capture already taken under the old one. That is
the mechanism working.

## The checkpoint is presented — 2026-08-23

A Windows session with the operator present picked this card up from
`40-parked/` to run its attended checkpoint. Preparation, all unattended:

- The packet's bound-evidence row for the test file was stale: done
  [[BPR-0422]] moved the tests from `tempfile` to the contained temporary
  support after the packet was authored. The row was re-bound to the live
  `sha256:840db7fa…` (13,862 bytes) before any decision was requested; the
  recorder script itself is byte-identical to the packet's original binding.
- The recorder's 36 hermetic tests pass on this host (Python 3.14.6).
- `course-manifest.json` now binds the refreshed packet and names the five
  authorized courses with their expected statuses: `rate-limit` 200,
  `graphql-refusal` 401, `absent-not-disclosed` 404, `pull-request` 200,
  `pull-request-comments` 200. A different live status on the two
  pull-request targets records honestly, fails `--validate-existing`, and
  reopens the target choice per the packet's own text.

The operator's exact statement remains the only authority that can start
traffic; nothing above changes that.

## The five authorized requests ran — 2026-08-23

The operator returned the packet's exact statement in the live session; it is
recorded verbatim in `operator-response.md`, and the capture ran exactly once
(`RFC0017_ANONYMOUS_CONTRACTS_CAPTURED:5`, evidence commit `4d250ab6`).

Four contracts match their courses and are committed:

- `rate-limit` 200 — the anonymous core budget is 60, and the same body
  records `graphql {limit: 0, remaining: 0}`;
- `absent-not-disclosed` 404 — Not Found with no existence disclosure;
- `pull-request` 200 — `octocat/Hello-World#1` is live (closed, base
  `master`, head `patch-1`), and a real `ETag` and `Last-Modified` are now on
  record for the future conditional-304 authority;
- `pull-request-comments` 200 — one review comment, bounded by `per_page=1`.

The fifth recorded the real contract and could not be committed.
`graphql-refusal` returned **403, not the expected 401**: GitHub's
unauthenticated GraphQL contract is a zero-quota refusal
(`x-ratelimit-resource` `graphql`, limit 0), corroborated by the committed
rate-limit body. But GitHub's refusal template embeds the caller's public IP,
which this card's own required final state forbids persisting and the
sanitizer has no rule for — its dropped-not-redacted stance covers headers
only. The capture file was deleted uncommitted, the course manifest was
trimmed to the four validated courses (`RFC0017_ANONYMOUS_CONTRACTS_VALID:4`),
and [[BPR-0445]] now owns the scoped sanitizer rule.

The card moves to `10-backlog/` behind [[BPR-0445]]: the GraphQL recapture is
its next live action and needs both the repaired rule and a fresh exact
authorization — the v001 approval was single-issuance and is consumed. The
conditional-304 and synthetic-exhaustion work stays here and now has the real
values it was waiting for.

## The GraphQL sanitizer is ready — 2026-08-24

[[BPR-0445]] completed the no-traffic repair and its hostile synthetic tests.
Any future `graphql-refusal` course now expects the observed 403 zero-quota
contract; the 401 above is retained only as the historical v001 expectation.
The repaired recorder normalizes exactly one caller IPv4 or IPv6 address,
records that normalization explicitly, and persists no raw-body digest. The
four existing captures remain unchanged and validate as `VALID:4`.

The fifth course has not been recaptured. BPR-0445's current attended artifact
is `docs/verification/reviews/rfc0017-anonymous-probe-v002/operator-packet.md`.
It still requires the fresh exact operator authorization and exactly one live
GraphQL request before this card can inherit a five-course manifest or claim
`VALID:5`.

That paragraph is now history: on 2026-08-27 the v002 issuance was consumed
without traffic by the Windows durability barrier, its byte-frozen successor
`docs/verification/reviews/rfc0017-anonymous-probe-v003/operator-packet.md`
was approved and captured from WSL, the fifth record joined
`rfc0017-anonymous-probe-v001/captures/`, and the restored five-course
manifest validates `RFC0017_ANONYMOUS_CONTRACTS_VALID:5`. Done [[BPR-0445]]
carries the full account; this card inherits the five-course record it was
waiting for.

Independent review of the sanitizer repair also made an older unfinished
requirement concrete: the four non-GraphQL records persist canonicalized bodies
but retain digests of the original response bytes, so `--validate-existing`
cannot by itself detect later minimized-body tampering. Their committed Git
blobs remain unchanged, but this card's required immutable-closeout
authentication of every minimization record and owning commit is not yet
implemented. The same external binding must protect the eventual normalized
GraphQL record from a self-consistent body-and-digest substitution. That
obligation remains here; it is not part of BPR-0445's operation-scoped privacy
rule.
