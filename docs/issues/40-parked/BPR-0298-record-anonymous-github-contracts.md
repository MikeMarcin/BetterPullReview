---
id: BPR-0298
title: Record anonymous GitHub service contracts
status: Parked
rfc: RFC-BPR-0017
depends_on: [BPR-0297, BPR-0405, BPR-0445]
parked_on: awaiting-live-github
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

## The external binding, the derived exhaustion fixture, and the GHES disposition — 2026-08-28

First, a settled reading: the 2026-08-19 section's split — BPR-0405 owns the
injection mechanism, this card owns values — invited correction, and the
scout measured it. BPR-0405's own result deferred **both** wholesale; the
`source_kind: synthetic` enum member and the `rest_conditional_not_modified`
operation entry were type declarations with no working path behind them. Both
halves therefore landed here, in one delegated worker slice reviewed and
reproduced by the integrator:

- **External immutable-evidence binding.** `capture-bindings.json` (schema
  `bpr.rfc0017.anonymous_capture_binding` v1) binds each of the five captures'
  exact file bytes, a freshly computed minimized-body digest — the digest the
  in-record `body_sha256` never was — and an owning commit, verified by
  `git cat-file` blob byte-equality: the checked property is "this commit
  stores exactly these bytes at this path", deliberately weaker than
  latest-touching, which is documented in `verify_one_binding` with the
  threat reasoning. `--validate-existing` requires the binding when present
  behind unchanged seeded flags, printing `BOUND:5` beside `VALID:5`;
  `--write-bindings` generates it fail-closed (missing git, untracked or
  dirty captures, unreproducible existing file all refuse). The captures,
  packet, response, and manifest are byte-identical to HEAD.
- **Derived synthetic exhaustion.** `synthetic/rate-exhaustion.json` is a
  pure function of three committed captures: real budget values from
  `rate-limit` (limit 60, real reset), the real recorded 403 zero-remaining
  refusal template from `graphql-refusal` (no invented prose — the template
  names no resource, so none was substituted; the resource semantics ride
  the headers, where the real service carries them), and the core-charged
  `url`/`operation` from `pull-request`. `--validate-existing` re-derives it
  and demands byte-equality (`DERIVED:1`); no 429/retry-after exemplar was
  fabricated because no recorded value supports one — if a secondary-limit
  shape is ever required it needs its own live authority.
- **Evidence.** 119 hermetic tests green (82 baseline + 37; the "36" in the
  2026-08-23 section was true when written and superseded by BPR-0445's
  additions). The integrator reran the suite, the seeded ledger command
  (`VALID:5`/`BOUND:5`/`DERIVED:1`, exit 0), and an independent
  tamper-restore probe against the real binding: a substituted
  minimized-body digest refuses with the exact course named, exit 3, and the
  restored file returns green. The worker's 11-mutation sweep over the new
  logic caught 11/11.
- **Known residue, deliberately left**: `sanitize_capture` still writes a
  raw-byte `body_sha256` for future non-GraphQL live captures (inert for the
  empty-body 304 course; the external binding carries the real guarantee).
  Fold the decision — repair versus schema note — into the conditional-304
  mechanism slice rather than changing digest semantics silently here.

**GHES disposition — removed from record-eligibility.** No designated GHES
instance exists anywhere in the repository's records, and the v001 packet
states none is authorized. The verification ledger's capability-selection
table already rules every GHES profile "Typed public-preview unavailable,
zero requests — not enabled by RFC-BPR-0017", and the recorder itself
structurally refuses any non-`api.github.com` host before its network
function can even be imported (`test_refuses_another_host`). An anonymous
service record from a GHES instance is therefore irrelevant to closeout:
there is no enabled GHES behavior for it to witness. This dispositions the
**record** layer only — the product's zero-request GHES refusal proof
remains seeded, Pending, and owned by BPR-0300/BPR-0301/BPR-0305's
claim-matrix row, and this card makes no product claim.

**What remains on this card**: the conditional-304 request-side mechanism
(outbound `If-None-Match`/`If-Modified-Since` sourced from the committed
`pull-request` capture's real `ETag`/`Last-Modified`, empty-body 304
sanitization, tests), and then the one bounded live request under a fresh
digest-bound v004 operator packet — the attended element, prepared and
parked at the run boundary, never a stop condition before it.

## The conditional-304 mechanism is in — 2026-08-28, night

The second worker slice, reviewed and reproduced by the integrator, makes
`rest_conditional_not_modified` live end to end with no packet and no
capture created:

- **Authority schema v2**: `condition_source` (`{capture_file,
  file_sha256}`) required on a conditional target, refused on every other
  operation, and a version-1 packet cannot name the conditional operation
  at all — refusing an authority nothing could execute at parse time is
  the fail-closed reading. The capture path is bounded repository-relative.
- **Request side**: the pinned source is loaded under a containment root,
  verified byte-exact against its digest, and validated as a capture
  record before `If-None-Match` (etag required — a modification date alone
  would ask a weaker question than the authority describes) and optional
  `If-Modified-Since` go on the wire; `perform_request` accepts only those
  two names and only for this operation, with the no-proxy/no-redirect and
  credential-header hardening untouched. Sources load **before** the
  single-use receipt is reserved, so an unprovable source cannot consume
  an authorization.
- **Response side**: a non-empty 304 body refuses; the record carries
  `body: null` with the empty-bytes digest; a conditional course must
  expect exactly 304.
- **Evidence**: 151 hermetic tests (119 + 32) green under the integrator's
  own rerun; the seeded v001 command is byte-identical
  (`VALID:5`/`BOUND:5`/`DERIVED:1`); the worker's 17-mutation sweep over
  the new logic caught 17/17, including the receipt-ordering and
  containment rules. The raw-byte `body_sha256` residue is now documented
  at its assignment, semantics unchanged, per the prior section's
  disposition.
- **Integrator dispositions on the worker's two open questions**: no
  sanitizer-level status rule for the conditional operation — status is
  enforced where statuses live, in the course manifest and
  `validate_capture_record`, and a live 200 already refuses on its
  non-empty body; and the unfireable schema-version bound stays, a
  defensive check whose comment says exactly what it is.
- **Operational fact the v004 packet must state plainly**: if the live
  conditional request answers 200 — the pull request changed since the
  pinned ETag — the refusal lands after the attempt receipt exists, the
  single-use authorization is consumed, and a re-run needs a fresh packet.
  That matches the graphql-refusal precedent and is deliberate.

## The v004 packet is authored; the card parks — 2026-08-28, night

The attended element is prepared and is the current live packet:
`docs/verification/reviews/rfc0017-anonymous-probe-v004/operator-packet.md`
(sha256 `fdef8d425733ef9efaded274ea7167ba22a3a13f26a3c76c784f57647f33add2`)
with its bound `course-manifest.json`. It authorizes exactly one
credential-free conditional GET replaying the committed `pull-request`
capture's `ETag`/`Last-Modified`, expected 304, capture written under the
v004 directory and bound to the v2 authority's own digest; it binds the
committed recorder bytes (`ebbbf370…`, 76,007) and states the two
no-capture outcomes plainly, including the 200-consumes-issuance hazard.
The integrator proved the packet parses end to end against the committed
recorder with zero traffic: authority v2, course selection,
recorder-binding row, manifest binding, and the conditional request
headers built from the digest-pinned source.

The answered v001–v003 packets are frozen history, not superseded — v004
is a new gate for a new course. Every unattended obligation on this card
is done, so it parks `awaiting-live-github`: the operator's exact packet
statement and the one live request are all that remain.

## The v004 issuance ran and was refused on a body — 2026-08-28, late night

The operator returned the packet's exact statement in the live session;
the canonical response (`operator-response.md`, byte-grammar validated
against packet `fdef8d42…`) authorized the one execution. The recorder
verified the pinned condition source, durably reserved the attempt receipt
(`rfc0017-anonymous-capture-attempts/fdef8d42….json`) and the output
destination, issued the single conditional GET with `If-None-Match` from
the committed ETag and `If-Modified-Since` from the committed date — and
refused: `conditional not-modified response carries a body`. No response
byte was persisted; the reserved destination is a committed zero-byte
file; the issuance is consumed under the never-delete rule. This is
exactly the outcome the packet stated for a changed representation, and
v004 stays byte-frozen as its own record beside v002.

The refusal path does not expose the status line, so the record claims
only what was observed: the answer carried a body. The overwhelmingly
likely cause is the packet's stated 200 hazard, with a specific mechanism
this attempt has now taught: the pull-request payload embeds volatile
repository counters — stargazers, forks, watchers of `octocat/Hello-World`
— so an ETag for this representation goes stale on its own, independent of
the pull request changing. A replayed five-day-old validator was
practically guaranteed to fail. The flaw is in the course design, not the
mechanism: source pinning, header construction, single issuance, and
post-receipt refusal all behaved exactly as their tests claimed.

What follows: conditional-304 accounting needs validators captured in the
same session that replays them — a paired course, one fresh GET recording
the current `ETag` and one conditional GET replaying it seconds later —
which is new unattended recorder mechanism, then a v005 packet authorizing
exactly those two requests as one decision.

## The v005 pair ran; conditional-304 accounting is recorded — 2026-08-28, late night

The paired mechanism landed at `90754ca1` (166 hermetic tests, a 15/15
mutation sweep whose one first-pass survivor drove its own new test, the
seeded v001 validation byte-identical). The live packet is now
`docs/verification/reviews/rfc0017-anonymous-probe-v005/operator-packet.md`
(`f0c1e719…`): two credential-free GETs to the one endpoint as one
decision, fresh 200 then same-run conditional 304.

**Advance approval, recorded rather than implied.** The operator
pre-approved this issuance verbatim in the live session, before the packet
was authored — "okay well i approve the next one too, let's get a move on
this is going too slow" — and directed no further round-trip. The
canonical response binds the packet digest to its authority statement
under that direction; this deviation from quote-after-authoring is
deliberate and operator-directed, and the packet's own Advance approval
section says the same.

The run captured both courses (`CAPTURED:2`, then `VALID:2` on
revalidation):

- `pull-request-fresh` 200 — a current sanitized contract whose etag
  (`"2012e41b…"`) differs from the committed five-day-old
  `"b651e01d…"`, confirming the v004 staleness diagnosis directly —
  core remaining 57, used 3;
- `pull-request-conditional` 304 — empty body, the same etag returned,
  core **remaining 56, used 4**.

**The recorded accounting fact: an anonymous conditional GET answered
`304 Not Modified` consumed one core budget unit** — `used` moved 3→4
between the pair's requests. The service's own counters say a 304 is not
free on the unauthenticated core budget, which contradicts the common
assumption and is precisely the kind of fact this card existed to record
rather than assume.
