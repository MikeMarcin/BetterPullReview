---
schema: "implement-rfc.card.v1"
id: "BPR-0450"
title: "Implement the device refresh wire contract and production adapter"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0279"]
---

# Implement the device refresh wire contract and production adapter

Done [[BPR-0279]] shipped refresh orchestration against a
`device_authorization_refresh_port` driven entirely by test doubles, because
compiling and parsing the refresh-token exchange is a wire-contract change
that was an explicit non-goal of that slice. The gap was surfaced by the
implementing lane rather than absorbed silently, and this card owns closing
it.

## Required final state

- `github_device_authorization_contract` gains the refresh-token exchange:
  an exact bounded form request (`client_id`, `grant_type=refresh_token`,
  `refresh_token`) against the token endpoint, and typed parsing of the
  refresh success and failure bodies under the same recorded-truth
  discipline as the existing courses. The refresh token is a validated
  runtime input that never appears in any diagnostic.
- A production adapter implements [[BPR-0279]]'s refresh port over
  `perform_device_authorization`, and the host's refresh lifecycle runs
  against it in a focused contract test registered as
  `github_device_authorization_refresh_contract` — the selector the
  RFC-BPR-0016 final command ledger already allocates.
- The recorded-fixture question is answered honestly: no attended refresh
  capture exists, so the wire shapes bind to GitHub's documented refresh
  contract and are labelled documented-not-recorded, or a future bounded
  attended capture is proposed as its own card. No fabricated
  recorded-GitHub claim is admissible.
- Hostile parser and native-transport coverage extend to the refresh
  exchange to the same standard as the existing device courses.

## Verification

```sh
ctest --preset dev --output-on-failure -R 'github_device_authorization_refresh_contract|application_device_authorization_refresh'
python3 scripts/verify_documentation.py --root .
```

## Implemented and closed — 2026-08-25

### The card's premise was one name off, and the code is the authority

There is no `device_authorization_refresh_port`. [[BPR-0279]] put
`refresh_access_token` on the existing `device_authorization_transport_port`,
reusing `device_token_exchange` and `device_token_completion` because the
server's renewal answer *is* its token answer. A read-only scout surfaced the
mismatch before any edit; this card implements what shipped rather than the
type its own text imagined. The consequence is recorded below and on
[[BPR-0281]].

### Wire contract

`compile_device_refresh_request` posts three fields to the token endpoint —
`client_id`, `grant_type=refresh_token`, `refresh_token` — with the refresh
token validated exactly as a device code is (bounded, unreserved characters
only) so material that could not have come from this contract never reaches the
wire, and form-encoded rather than interpolated.
`parse_device_refresh_response` admits one refusal code, `bad_refresh_token`,
and fails closed on every other. The success body is parsed by a
`token_grant_body` helper extracted from `parse_device_token_response`, so the
authorization and renewal successes are literally the same code and cannot
drift into accepting shapes the other refuses.

### Documented, not recorded — and structurally so

BPR-0276's attended sitting captured five courses and none was a renewal.
Filing refresh shapes in `tests/fixtures/github/device-authorization/v1/` was
therefore not an option: that manifest digest-pins exactly nine artifacts and
is bound to an owner-accepted review of a sitting that never contained a
renewal, so adding a tenth would either break the binding or forge membership.
The recorded tree is byte-identical to what BPR-0276 admitted.

The shapes live in a sibling `documented-v1/` tree with its own schema names,
`state: documented_not_recorded`, an explicit `"recorded": false`, and
`source_kind: documented_not_recorded` on every artifact. That value is
deliberately **not** added to `capture_rfc0016_device_contracts.py`'s
`ARTIFACT_SOURCE_KINDS`, which stays `{synthetic, unattested_capture}`: the
capture sanitizer must remain structurally unable to emit or accept a
documented shape. No recorded-GitHub claim is made anywhere.

The unproven surface is then narrowed rather than asserted away. The contract
test proves the documented renewal success body is field-for-field identical to
the recorded `token_success` projection, so what is genuinely unrecorded is the
request's grant-type pair and the one refusal code — and both of those are
exercised against a real TLS socket below.

### Production adapter

`github_device_authorization_transport` implements the whole transport port
over `perform_device_authorization`. Implementing only refresh was not
available: a port with two unimplemented methods is not shippable, and the
card's own requirement that "the host's refresh lifecycle runs against it"
means the host calls the other two. That is a widening of this card and a
narrowing of [[BPR-0281]], recorded on both rather than absorbed silently —
BPR-0281 keeps the composition wiring, the timer adapter, and the selection.

The adapter holds no policy: compile, hand off the decision lane, parse, post
exactly one completion. Secret material is read once to build a body and wiped;
cancellation requests a per-request stop token and does not suppress the
terminal. A refused renewal becomes the host's `expired_token` refusal, which
is the path to `refresh_rejected`; there is no deferred case, because a renewal
has nothing pending.

### Evidence

`github_device_authorization_refresh_contract` — the exact selector the
RFC-BPR-0016 final command ledger allocates — registers eight loopback
scenarios over the existing disposable-CA fixture, following the
`rfc0017_credential_free_native_https` precedent of a second selector on the
same harness with a `--suite` filter. Seven are wire courses: success,
`bad_refresh_token`, untrusted certificate, cross-origin redirect,
query-bearing redirect, response limit, and mid-response cancellation. The
proxy asserts the exact renewal body, so a wrong grant type or a leaked device
code fails there rather than in a parser. The eighth, `refresh-lifecycle`,
builds the real `github_device_authorization_transport` over a real
`native_https_transport`, drives a real `device_authorization_application_host`
through `resume_refresh`, and proves one connection, one rotation, the
*rotated* refresh token in the vault (the fixture returns a different value
than it was sent, so a vault that was never written is distinguishable from one
that was), the renewed access token crossing the enrollment boundary once, and
no retained pending material.

`github_device_authorization_contract` gains the documented-tree digest check,
the recorded-twin equality proof, exact body compilation, input refusals, both
outcome parses, and hostile shapes including every poll-exchange error code
refused as a renewal answer. `native_https_device_authorization_fixture` now
runs 22 scenarios.

Full device matrix green: 11/11 across core, host, refresh, vault, contract,
refresh contract, both native fixtures, and the three Qt scales. Format,
`check_boundaries`, `verify_documentation.py`, and `app-release` all clean.

Five falsification probes, each reverted: the poll's grant type in the renewal
request failed both the exact-body row and the loopback body check; admitting
every error code as `bad_refresh_token` failed "a poll-exchange error code is
not a renewal answer"; mapping a refused renewal to a deferred answer and
dropping the rotated refresh token each failed the lifecycle rotation row.

This unblocks [[BPR-0282]]. The done move promotes nothing on its own.
