---
id: BPR-0278
title: Implement device authorization wire and native HTTPS contracts
status: In Progress
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
