---
id: BPR-0278
title: Implement device authorization wire and native HTTPS contracts
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0276]
---

# Implement device authorization wire and native HTTPS contracts

## Required final state

- A narrow GitHub adapter owns exact bounded form requests and sanitized typed
  responses for device-code and token endpoints.
- Both endpoints reuse RFC-BPR-0002 system trust, proxy, cancellation, response
  limits, certificate refusal, unknown-authority refusal, and query-redirect
  refusal without weakening authenticated API behavior.
- No secret or transient code enters URLs, process arguments, environment,
  diagnostics, logs, fixtures, settings, or SQLite.
- Native loopback/hostile contracts prove exact methods, paths, headers, bodies,
  bounds, redirect/certificate failures, and cancellation for both endpoints.

## Adapter contract checkpoint — 2026-08-24

`src/adapters/github/src/github_device_authorization_contract.{hpp,cpp}` now
owns the narrow wire contract, bound to the recorded BPR-0276 fixtures:
`compile_device_code_request` and `compile_device_token_request` produce the
exact POST, URL, three-header set, and form-encoded body for each endpoint
(client id and device code validated and never compiled in — the client id is
a typed runtime input); `parse_device_code_response` and
`parse_device_token_response` return typed grants, delays, and refusals with
bare-enum failures so no diagnostic can carry a response byte. The parsers
enforce the recorded truths exactly: token-endpoint errors arrive with status
200 and only the device-code endpoint refuses with 400; `slow_down` alone
carries an interval; success field sets are closed to the recorded
`observed_fields`; `token_type` must be lowercase `bearer` with an empty
scope; `error_uri` must be `docs.github.com`; and a verification address off
`https://github.com` is refused (`off_authority_verification_uri`).

`github_device_authorization_contract` is registered network-isolated and
green: it replays all seven recorded outcomes through synthetic bodies whose
transient fields stand in for the recorded observed-but-omitted secrets,
asserts compiled requests equal every recorded `endpoint_url`,
`request_method`, and `request_content_type`, ties `valid_device_client_id`
to the recorded registration's public client id, and drives hostile matrices
(extra/missing/mistyped fields, fractional and negative intervals,
off-authority and non-https verification URIs, unofficial error authorities,
unknown error codes, non-lowercase token type, non-empty scope, oversized
bodies, wrong statuses and content types) to closed failures. Format check,
strict adapter build under -Werror, and the focused selector all pass.

Design decisions recorded for successors: the production credential-free
native-HTTPS factory the device-code POST needs is deferred to [[BPR-0279]],
which owns the request path; the Windows loopback-fixture question follows
the existing APPLE/Linux guard and stays with the [[BPR-0446]]/[[BPR-0311]]
family. Remaining in this card: the dedicated native HTTPS device suite —
`native_https_device_authorization_contract` and
`native_https_device_authorization_fixture` on the git-smart precedent —
proving both endpoints under the hostile loopback matrix.

## Native suite delivered and the card closed — 2026-08-24

A bounded `rfc-worker` lane implemented the native half inside an explicit
five-file scope; the integrator reviewed the complete diff and reproduced
every command. `native_https_transport` gains a
`perform_device_authorization` entry whose additions are confined to the new
device surface: URL construction admits exactly the two `/login` routes on
the `github_com` contract with no query and byte-equality against the
compiled contract URL; the entry admits only the deliberately credential-free
binding, so the absent `Authorization` header is structural rather than
conditional and a shipped build refuses everything until [[BPR-0279]] adds
the production credential-free factory; and the single request loop is the
existing shared machinery, with any redirect converted to a typed refusal.
Exactly five existing lines changed, each provably inert for every prior
surface: the response bound now takes the minimum of the policy and
per-request limits (defaults preserve every existing surface), and a
per-request admitted contract status — zero everywhere but the device-code
endpoint's recorded 400 — lets `device_flow_disabled` reach the parser, with
the error relaxation narrowed to Qt's exact HTTP-400 mapping. That last
addition exceeded the assignment's literal scenario list and was accepted on
review: without it the recorded 400 branch is unreachable in production.

Proof: `native_https_device_authorization_contract` covers GHES and off-path
refusals, endpoint/URL pairing swaps, the structural credential-free
invariant, and pre-cancellation; `native_https_device_authorization_fixture`
drives fourteen loopback scenarios — success, disabled, slow-down,
untrusted-certificate, cross-authority redirect, query-bearing redirect,
response-limit at the exact 64 KiB bound, and mid-response cancellation
across both endpoints — through a CONNECT-proxy/disposable-CA driver that
validates the exact request line, headers, and form body and refuses any
credential-shaped byte on either leg. The worker falsified its own greens
three ways on scratch copies (off-authority URI, exact-bound response,
inverted credential probe). Integrator reproduction: dev configure and build
clean, 2/2 new selectors, 6/6 existing native and adapter suites, format
check, `DOCUMENTATION_OK`, and `git diff --check` all pass. Windows
registration stays with the [[BPR-0446]]/[[BPR-0311]] family, and
`scripts/verify_rfc0016_test_inventory.py` remains [[BPR-0282]]'s to create.
The done move promotes exactly [[BPR-0279]].
