---
id: BPR-0559
title: Diagnose device sign-in response refusal after browser authorization
status: In Progress
rfc: RFC-BPR-0016
depends_on: []
---

# Diagnose device sign-in response refusal after browser authorization

Reproduce and repair the response-handling failure observed in the v007 attended run. Browser authorization completed, while the app reported unusable GitHub data before session establishment. Identify the actual failing contract; do not infer the failure stage from the generic message.

## Acceptance

- A concrete source-backed cause is discriminated by a focused offline fixture or contract test.
- The valid supported response establishes the intended session; malformed and authority-mismatched responses still refuse without weakening token/identity validation.
- Focused adapter/host and one composed-path checks cover the repaired boundary.
- Record uncertainty honestly; no credentials, live retry, or raw token response logging. A changed publication build needs its own exact approval.

Observed source: `2a043335`; [v007 attended result](../../verification/reviews/designated-live-review-publication-v007/attended-result.md).

## Current source-backed finding

Read-only scouting traces the captured title to GitHub `session_failed` and
the body to `malformed_response`. Token-exchange parsing alone does not set
that session state. Account identity, capability discovery, or later session
acceptance can refuse with the same message; no safe live response metadata
was retained to discriminate them. Nullable user display name and missing
OAuth scopes are already handled correctly. A capability GraphQL error is a
hypothesis, not an established cause.

Next bounded work adds safe typed bootstrap-stage context to the existing
host diagnostic projection and copyable UI, with focused host/composed tests.
No raw response, credential, remote error text, or new network operation is
permitted. This improves the next observation; it does not itself prove the
live response failure repaired. Root-cause acceptance remains open until the
actual failing contract is known.

## Safe diagnostic instrumentation verified — 2026-09-04

The host now attaches a finite bootstrap stage to the exact core-accepted
session failure, carrying it through deferred credential cleanup. Stale
terminals cannot label a replacement session. Qt renders local enrollment,
account identity observation, capability discovery, or session acceptance
with the existing safe diagnostic; Copy message copies that same displayed
text. Provider replies, credentials and remote error messages do not cross
the projection. No parser, retry or authority policy was relaxed.

Worker dev and tests-disabled Release incremental builds pass. The worker's
final four selectors passed 4/4 in 3.72 seconds; integrator reproduction:

```sh
ctest --test-dir build/dev --output-on-failure -R '^(application_github_host|qt_composed_github_scale_(100|125|200))$'
```

passed 4/4 in 3.13 seconds. Host cases distinguish viewer and capability
refusal, deferred cleanup and stale replacement; the composed Qt window
covers all four prefixes, copying, precedence and clearing. The rendered
capability failure was inspected and exposes both message and copy control.
Strict tidy passes for all five changed TUs and final host/test repeat;
seven-file formatting, code-simplifier review, documentation and diff hygiene
pass. No app composition source/pin changed; no full matrix or live check ran.

Initial implementation verification failures were corrected: a misplaced
initializer failed compilation, and a regression test used an unavailable
fixture Public preview transition instead of ordinary profile activation.
Neither result is represented as passing. All thirteen consumed
`/tmp/bpr0559-*.log` files were removed after results and diffs were reviewed.
The existing dev/Release roots and ignored renders are reused.

This slice improves observation only. The live response refusal remains
unidentified. A separate sign-in-only attended check of the exact new Release
binary is required; v007 publication authority is spent and cannot be reused.

## Pending sign-in-only observation

The [diagnostic checkpoint](../../verification/reviews/device-session-bootstrap-diagnostic-v001/operator-packet.md) is complete and ready for the owner. Packet SHA-256 `bfab453132fc6b5f2958555be2a575b6ad7542a187da83b794482099ded2c3fb` binds the exact 10,656,768-byte ordinary Release executable `5e27a59a8e4c0c663b36626860da9368ab62b2ca5bf56e09d63c25d92c0ac1ec`. No response has been supplied, no diagnostic run launched and no publication authority created. The card parks awaiting the owner; the actual root cause and repair criterion remain open. The existing Release build is retained for this active consumer. No temporary worktree or inspection logs remain.

## Approved diagnostic observation — 2026-09-04

The owner supplied the exact [one-run approval](../../verification/reviews/device-session-bootstrap-diagnostic-v001/operator-response.md). Packet and binary hashes match. The integrator resumes this card for the sign-in-only observation and cleanup; no PR or publication is authorized.

The approved ordinary executable was launched once with no arguments after a second digest check, with all 24 specified environment variables removed from its child environment and child output discarded. Native UI showed the expected GitHub.com Native HTTPS profile and no connected host. Operator browser authorization is pending; do not launch again. The active run must finish by collecting its safe terminal result, explicitly selecting Sign out and quitting. Documentation gate passed after approval recording.

## Diagnostic process closed before observed result

The owner reported no visible app. The process exited with code 0 and native app inventory confirmed no remaining instance. See the [recorded outcome](../../verification/reviews/device-session-bootstrap-diagnostic-v001/attended-result.md). No connection result or explicit Sign out was observed. The exact one-launch authority is consumed; no retry was made. Root diagnosis remains pending authorization to reopen the same build.

## Capability discovery failure observed and cleaned up

The owner authorized one reopen with “try again”. The [retry result](../../verification/reviews/device-session-bootstrap-diagnostic-v001/retry-authorization-and-result.md) records matching owner/UI evidence that capability discovery failed. Copy message and explicit Sign out were exercised; terminal disconnected UI and normal exit 0 were observed. No PR opened or publication attempted. This card resumes bounded offline diagnosis of capability transport/normalization; the precise refusing contract remains unknown.

## Native transport plus schema normalization discriminator

The existing HTTPS fixture previously returned only `{}` and Retry-After metadata for capability success; it did not normalize a usable schema. The new capability-success course sends the exact compiled query to a credential-free TLS loopback GitHub.com authority, validates canonical query/body and request headers, returns the existing supported public-schema fixture with the complete GraphQL rate tuple, and passes the native reply into production normalization. All four capabilities are supported and all schema facts match. The prior five-header passthrough case remains as capability-metadata.

The existing native target build, changed-TU strict tidy, formatting, Python compilation and diff hygiene pass. Worker selected checks passed 3/3 in 13.67 seconds; integrator reproduction of `ctest --test-dir build/dev --output-on-failure -R '^(native_https_transport_contract|native_https_transport_fixture|github_capability_probe)$'` passed 3/3 in 13.14 seconds. Full final two-file diff and code simplification were reviewed. Loopback certificates/proxies were cleaned by the fixture; consumed native-check temporary logs were removed. Existing build roots remain reused.

This closes an offline coverage gap; it does not reproduce the live refusal or prove provider/session acceptance. Next bounded work must expose the fixed refusal category within capability discovery (transport metadata, GraphQL envelope/schema, provider correlation, or host cache/acceptance) without raw provider text, secret logging, relaxed validation, changed retry behavior or another live launch. The root-repair acceptance remains open.

## Finite capability refusal diagnostics verified

Capability-specific error records now carry a closed category through native
HTTPS, normalization, provider and host handling. Existing predicates, core
failures, retries and cleanup authority remain unchanged. Native failures use
a private capability-only terminal; other surfaces retain their existing
provider errors. Fixed Qt sentences appear in the existing copyable message,
and invalid enum values become an explicit unavailable-detail fallback.
Deferred cleanup preserves the original accepted failure's detail; stale
replacements cannot inherit it.

Incremental dev and tests-disabled Release builds pass. Worker affected checks
passed 8/8 in 23.26 seconds, final provider/Qt identifier recheck 4/4 in 3.73
seconds, architecture boundary 1/1 in 6.07 seconds, scoped strict tidy across
the changed TUs and final affected repeats, formatting, Python compilation and
diff hygiene. Integrator reproduction of the eight-selector command below
passed 8/8 in 20.54 seconds:

```sh
ctest --test-dir build/dev --output-on-failure -R '^(native_https_transport_contract|native_https_transport_fixture|github_capability_probe|github_provider|application_github_host|qt_composed_github_scale_(100|125|200))$'
```

Nine native refusal scenarios, normalizer/provider categorization, host local
and deferred/stale errors, and fixed visible/copied Qt messages are covered.
The integrator inspected the entire twenty-file source/test change and the
rendered GraphQL-error detail. Code simplification extracted the host's optional
diagnostic and combined identical fixed fallback branches. Qt coalesces the
duplicate numeric-header fixture on this runtime; it correctly reports observed
invalid metadata rather than inferring duplicate provenance.

Initial verification failures were corrected: incomplete Qt/helper declarations,
a missing native forwarding branch for the new terminal (also independently
found in review), a duplicate-header test expectation inconsistent with Qt's
observed normalization, duplicate fallback branches flagged by tidy, and an enum
identifier colliding with the architecture guard. The guard was preserved.
The final named host-provider-error category distinguishes invalid error records
from invalid success rate state. No known blocking review finding remains.

Closeout review runtime: native rfc-critical-reviewer (gpt-6-astra, xhigh).
The review was limited to unchanged admission/retry/authority/cleanup behavior,
exact diagnostic attribution and absence of raw-value disclosure. Opposite-family
review was unavailable. No live root-repair claim or full matrix is made.

Documentation initially failed during unrelated RFC-directory reorganization,
then exposed missing historical freeze recognition for answered v007. New
validation records bind the unchanged v007 and diagnostic-v001 artifacts to
commits containing those exact bytes, using the existing verifier convention.
No answered packet, response, source manifest or verifier was rewritten.
Documentation now passes and all 62 documentation tests pass. All 21 consumed
`/tmp/bpr0559-detail-*.log` files were reviewed and removed; loopback resources
were cleaned by the fixture. Existing dev/Release roots and ignored renders
remain reused. The root cause remains open pending a separately authorized
observation of the exact new diagnostic executable.
