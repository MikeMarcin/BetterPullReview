---
schema: "implement-rfc.card.v1"
id: "BPR-0279"
title: "Implement bounded device-authorization host orchestration"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0277","BPR-0278"]
---

# Implement bounded device-authorization host orchestration

## Required final state

- The application host owns device-code acquisition, a controlled-clock poll
  schedule, server interval and `slow_down`, a fixed total bound, cancellation,
  shutdown, and deterministic terminal cleanup.
- Success moves credential material exactly once into the existing
  `github_application_host::enroll_profile` vault boundary; no second storage or
  lifetime policy exists.
- If BPR-0275 selects a GitHub App, the host refreshes before expiry and stores
  refresh material under the same vault authority, with typed recoverable
  failure. If BPR-0275 selects an OAuth App, no refresh path is implemented.
- Every non-success path erases transient material and leaves no partially
  enrolled profile. Direct host tests force all reorderings and failure cuts.

## Implemented, repaired at the Release boundary, and closed — 2026-08-24

A bounded `rfc-worker` lane delivered all three stages inside its allowlist;
the integrator reviewed the complete diff, repaired one finding, and
reproduced every command. `device_authorization_schedule` is clock-free and
replayable: the grant instant counts as the last poll, waits clamp to the
earlier of next-due and deadline, `slow_down` takes the larger of the raised
interval and the server's answer so no response can speed polling up, and
degenerate bounds clamp rather than refuse. The host
(`device_authorization_application_host`) drives it through four injected
ports — transport, monotonic clock, timer, enrollment — deliberately
choosing a monotonic clock over the UTC precedent so wall-clock steps can
neither outrun an interval nor end a live flow early. Token success crosses
into the vault exactly once through `enroll_profile`; transient material
crosses the ports as move-only `github_credential_material`, so no-copy-
retained is structural; transport retries are bounded to network/timeout
(three), while certificate, redirect, malformed, and oversized answers end
the flow at once. The display projection carries exactly stage, user code,
validated verification URI, and bounded remaining validity.

Refresh material lives in a vault refresh slot: three additive port
operations keyed by the unchanged authority key, implemented on macOS as
distinct keychain services (a slot travels with every queued operation so
refresh work can never execute against the credential service), with Linux,
Windows, the unavailable vault, and the public-path fixture answering typed
`unavailable` — their enablement stays with [[BPR-0310]]/[[BPR-0311]].
Rotation stores the new refresh entry durably before the new access token
enrolls, so a crash never strands zero credentials; stale and cancelled
refreshes cannot rotate or leak; refresh failure is a typed re-enrollment
request. The bounded pending window between grant and authority binding is
host-memory only, erased on cancel, shutdown, and new-begin, and tested both
ways. A vault store failure during initial authorization fails the flow
closed rather than enrolling a token whose renewal chain was never recorded.

The integrator's review finding: the worker's production
`make_native_https_credential_free_transport` referenced a testing-gated
type, so the tests-disabled Release build — which no focused selector runs —
did not compile. The repair ships `credential_free_request` unconditionally
while keeping the permissive read match testing-only, with the shipped
configuration explicitly refusing credential-free reads: RFC-BPR-0017's
anonymous read path stays its own gated program, and the device endpoints
remain the only shipped surface the binding satisfies. `app-release` now
builds clean alongside dev and core-only.

Evidence: `application_device_authorization_host` (31 tests) and
`application_device_authorization_refresh` (15 tests) green;
`application_github_credential_vault` and both keychain suites extended and
green; 13/13 reproduction selector run; worker falsification probes
(inverted rotation order, dropped interval floor) each failed the intended
test; format, documentation, and hygiene gates clean. Follow-up ownership
recorded: the refresh wire contract is new card [[BPR-0450]] (now blocking
[[BPR-0282]]), and the production timer adapter and transport bridge are
inherited by [[BPR-0281]].
