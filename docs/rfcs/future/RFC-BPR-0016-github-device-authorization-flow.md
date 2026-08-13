---
id: RFC-BPR-0016
title: GitHub Device Authorization Flow
status: Deferred
area: BPR
milestone: Post-Pack v2
depends_on: [RFC-BPR-0002]
pack_version: post-v2
---

# RFC-BPR-0016: GitHub Device Authorization Flow

## Status and scope

This is a deferred, external-authority-dependent RFC for work after the
canonical fourteen-RFC pack. It is not part of pack v2, does not extend that
pack's implementation order, and does not block RFC-BPR-0002 or any other
pack-v2 RFC.

RFC-BPR-0002 ships native HTTPS with an operator-supplied personal access
token as its production authentication path. This RFC adds a second
credential-acquisition path over that same transport. It does not replace the
token prompt, does not change the credential vault boundary, and does not
relax any redirect, certificate, or proxy rule established by RFC-BPR-0002.

The deferral here is not upstream capability. GitHub already supplies the
device authorization grant. The blocking input is external authority: a
registered application identity that only the product owner can create. Until
that identity exists and is recorded, every device-flow choice remains fixed
at a `device_flow_unavailable` diagnostic, in the same shape as the existing
zero-launch `cli_unavailable` result.

## Motivation

Enrolling a host today means the operator leaves the application, decides
which token type to mint, decides which scopes that token needs, creates it in
a browser, copies it, and pastes it into an unlabeled password field. Every
one of those steps is a decision the application is better positioned to make
than the person making it, and each one is a place to grant too much or too
little authority.

The failure modes are not hypothetical. An over-scoped token grants the
application more than reviewing a pull request requires. An under-scoped token
fails at an arbitrary later point, and the resulting diagnostic describes the
request that failed rather than the scope that was missing. A long-lived
classic token outlives the reason it was created. A pasted secret has, by the
time it reaches the field, passed through a clipboard shared with every other
process on the machine.

The device authorization grant removes the entire minting step. The
application declares the scopes it needs, the operator confirms an identity in
a browser they already trust, and the resulting credential arrives without a
clipboard, without a scope decision, and without the operator ever handling
token material.

Device flow specifically — not the authorization-code redirect — because the
redirect variant requires a loopback listener to receive the callback. A local
HTTP listener is a prohibited design in
[RFC-BPR-0015](RFC-BPR-0015-github-cli-production-enablement.md) and remains
prohibited here. The device grant polls an outbound endpoint instead and needs
no inbound socket at all.

## Goals

- Add device authorization as a second credential-acquisition path over the
  existing native HTTPS transport.
- Acquire credentials without the operator selecting scopes, minting a token,
  or moving secret material through a clipboard.
- Keep the acquired token inside the application host and the platform
  credential vault, exactly as an operator-supplied token is kept today.
- Retain manual token entry as a supported, first-class path.
- Present a device-flow choice that fails closed and visibly on any host where
  the flow is not available.

## Non-goals

- Replacing or removing the manual token prompt.
- Enterprise administration, organization policy, or application installation
  management.
- Browser cookie access, session reuse, or any embedded web view.
- Changing the credential vault, its platform adapters, or its storage
  boundary.
- Relaxing any RFC-BPR-0002 transport rule.
- GitHub CLI enablement, which remains
  [RFC-BPR-0015](RFC-BPR-0015-github-cli-production-enablement.md).

## Eligibility gates

Implementation may begin only after all of the following exist:

- **A registered application identity.** The product owner registers an
  application on GitHub.com with the device authorization grant enabled, and
  records its client identifier, its exact requested scope set, and the
  account or organization that owns it. The client identifier is a public
  value compiled into the binary. No client secret may exist in the shipped
  application, which constrains the registration to a public-client
  configuration.
- **A resolved application-kind decision.** OAuth App and GitHub App differ in
  ways this RFC cannot decide unilaterally, and the choice changes the
  required design. A GitHub App issues short-lived user tokens with refresh
  tokens and offers per-repository permissions, but requires an installation
  on every organization whose pull requests are reviewed, which is an
  administrator action outside the operator's control. An OAuth App issues a
  non-expiring token and reaches every repository the operator can already
  reach, at the cost of coarser scopes and no refresh story. The registration
  cannot be created without deciding this, and the token-lifetime section
  below is conditional on it.
- **Recorded wire contracts.** The device-code endpoint, the token endpoint,
  the pending, denied, expired, rate-limited, and disabled error bodies, and
  the exact success body are captured as sanitized fixtures from the real
  service. Documented behavior is not sufficient; the fixtures must come from
  the endpoints actually called.
- **RFC-BPR-0002 Auto-Verified.** This RFC reuses that RFC's transport,
  redirect, certificate, and vault guarantees rather than restating them, so
  it cannot close before the guarantees it depends on are themselves closed.

GitHub Enterprise Server requires an application registered on each
enterprise instance. That registration is an enterprise-administration action
this RFC explicitly does not own. Until an instance supplies one, GHES
profiles keep the manual token path and report `device_flow_unavailable`.

## Architectural allocation

- **Core:** owns the device-authorization interaction state as a bounded
  sequence of typed observations — requested, awaiting authorization,
  authorized, denied, expired, unavailable — and decides which host commands
  are legal in each. It never sees the client identifier, an endpoint, a
  device code, a token, or a poll interval.
- **Host:** owns the device-code request, the polling schedule and its
  bounds, the token exchange, the transition from token material to the
  existing credential vault enrollment, and every structured diagnostic. The
  token is created, used, and stored here and crosses no boundary outward.
- **Qt adapter:** owns the presentation of the user code, the verification
  address, the remaining validity, the browser handoff, and cancellation. It
  receives a projection. It never receives the access token and never calls
  the provider directly.
- **Flow:** a device-authorization intent is decided by the core, becomes a
  typed host request, and returns a revisioned observation the core accepts
  before Qt renders it. Success enters the existing profile-enrollment path at
  exactly the point the manual prompt enters it today, so vault storage,
  identity observation, and capability probing are unchanged.

## Required design

### Acquisition

The host requests a device code from the registered client identifier and the
fixed scope set, then polls the token endpoint until the grant resolves. Both
endpoints are reached through the RFC-BPR-0002 native HTTPS transport under
its existing rules: system trust and proxy configuration, no certificate-error
bypass, unknown-authority redirects rejected before any credential or body is
forwarded, and every query-bearing redirect failing closed even when it names
the configured authority.

The scope set is fixed at compile time and is the minimum that satisfies the
product's read and review-publication surface. It is not operator-editable,
because an operator-editable scope set reintroduces the decision this RFC
exists to remove.

### Browser handoff

The application opens the verification address in the system browser. The
system browser, never an embedded web view: an embedded view would place the
operator's GitHub credentials inside this application's process, which is
strictly worse than the paste flow it replaces and defeats the purpose of
delegating authentication at all.

The address opened is derived from the compiled host profile and validated
against it before the call. A verification address supplied by the response is
not followed unless it matches the profile's expected authority.

Opening a browser can fail, and failure is not an error state. The user code
and the verification address remain displayed and copyable so the operator can
complete the flow on any device. The flow does not depend on the handoff
succeeding.

### Polling discipline

Polling is bounded in every dimension. The host honors the server-supplied
interval, applies the additional backoff the service requests when it reports
rate limiting, stops on denial, stops on expiry, and stops at a total
wall-clock bound regardless of what the service returns. Polling is
cancellable from the UI and on application shutdown, and cancellation leaves
no retained device code.

A device code is single-use and short-lived. It is retained only for the
polling window, is never written to settings, logs, diagnostics, SQLite, or
evidence, and is erased when the flow resolves in any direction.

### Token handling

The access token enters the existing `github_credential_material` path and is
enrolled through the existing vault. It receives no separate storage, no
separate lifetime, and no separate erasure rule. From the moment of exchange
it is indistinguishable to the rest of the system from a token the operator
pasted.

Token lifetime handling is conditional on the eligibility-gate decision. If
the registration is a GitHub App, the design must additionally own refresh
before expiry, refresh failure as a distinct recoverable state, and refresh
token storage under the same vault rules. If the registration is an OAuth App,
no refresh path exists and the credential is replaced only by re-enrollment.
The RFC cannot be implemented against both; the gate decides which.

### Unavailability

Every host without a registered application identity presents the
device-flow choice and returns a fixed `device_flow_unavailable` diagnostic
before any request is constructed or sent. This mirrors the existing
`cli_unavailable` treatment: the choice stays visible so the intended
authentication surface is explicit, and selecting it produces a stable typed
refusal rather than a partial attempt.

## Prohibited designs

- Starting a local HTTP listener, callback server, broker, or daemon.
- Embedding a web view, browser engine, or any in-process credential entry for
  GitHub itself.
- Shipping, deriving, or obfuscating a client secret in the application.
- Reading, importing, or reusing browser cookies or browser session state.
- Placing a token, refresh token, or device code in process arguments,
  environment, logs, diagnostics, SQLite, UI text, fixtures, or evidence.
- Polling faster than the interval the service supplies, or continuing to poll
  after denial, expiry, or the total bound. The sole exception is one
  digest-bound, owner-attended, capture-only early poll used by BPR-0276 to
  record GitHub's `slow_down` contract. That probe is bounded to the designated
  application, never enters product code, sends no repository request, resumes
  compliant timing immediately, and grants no general rate-limit exception.
- Allowing an operator-editable scope set, or silently widening scopes.
- Following a verification or redirect address that does not match the
  compiled profile authority.
- Treating a failed browser handoff as a failed authorization.
- Using GitHub Actions for build, test, package, release, or verification.
- Enabling any partial device-flow mode while an eligibility gate is
  unsatisfied.

## Acceptance criteria

- A GitHub.com profile enrolls through the device grant end to end against
  recorded contracts, and the resulting credential is stored by the existing
  vault with no settings or SQLite copy.
- The core, Qt, and projection surfaces contain no token, refresh token,
  device code, client identifier, or endpoint, proven by boundary tests rather
  than inspection.
- Pending, denied, expired, rate-limited, and disabled responses each produce
  a distinct typed diagnostic, and none of them leaves a retained device code
  or a partially enrolled profile.
- Polling honors the supplied interval and the requested backoff, stops at the
  total bound, and cancellation from the UI and at shutdown terminates polling
  and erases retained material.
- Certificate failure, unknown-authority redirect, and query-bearing redirect
  fixtures are rejected on both device-flow endpoints before any credential or
  body is transmitted.
- A verification address outside the compiled profile authority is refused,
  and no browser is opened for it.
- A failed browser handoff leaves the flow live with the code and address
  still presented, and the flow completes when authorized elsewhere.
- Manual token entry remains available and functional on every profile,
  including profiles where device flow succeeded.
- Every profile without a registered application identity, including all GHES
  profiles, returns `device_flow_unavailable` with zero requests sent.
- Build and verification evidence is produced locally or on target-native
  release-operator environments, with no GitHub Actions involvement.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Device grant enrolls a credential | Tests-disabled public composition against recorded contracts | Run the flow from clean start and prove the vault holds the credential and no settings or SQLite copy exists |
| No token crosses the host boundary | Core and Qt adapter boundary tests | Prove core and projection types cannot name token, refresh token, device code, client identifier, or endpoint |
| Terminal states are typed and clean | Host contract tests per error body | Exercise pending, denied, expired, rate-limited, and disabled fixtures and assert a distinct diagnostic and zero retained material each |
| Polling is bounded | Host polling contract with a controlled clock | Assert interval compliance, requested backoff, total-bound termination, and cancellation from both UI and shutdown |
| Redirects and certificates fail closed | Native transport contract on both endpoints | Exercise unknown-authority, query-bearing redirect, and certificate-failure fixtures and prove no transmission |
| Verification address is bound to the profile | Host address validation contract | Supply an off-authority verification address and assert refusal with no browser invocation |
| Browser failure is not authorization failure | Qt handoff contract with a failing opener | Assert the flow stays live, the code and address stay presented, and later authorization completes it |
| Unavailability is zero-request | Public self-test and provider counters | Select device flow on an unregistered profile and require `DEVICE_FLOW_UNAVAILABLE_OK` with zero transport calls |
| No secret in the shipped binary | Release binary inspection | Prove the executable contains the public client identifier and no client secret |

## Documentation impact

Authentication modes, the credential boundary, the device-flow interaction
states and their diagnostics, the fixed scope set and its justification, the
registered application identity and its owner, the browser handoff and its
failure behavior, and the GHES unavailability rule are mandatory.

The documentation ledger must identify functional-core, application-host, and
Qt-presentation-adapter ownership; typed intents, events, observations,
requests, and deltas; authority; concurrency and generation; and any
evidence-backed no-impact region.
