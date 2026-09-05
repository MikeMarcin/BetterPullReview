---
id: BPR-0559
title: Diagnose device sign-in response refusal after browser authorization
status: Parked
parked_on: awaiting-owner-verdict
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
