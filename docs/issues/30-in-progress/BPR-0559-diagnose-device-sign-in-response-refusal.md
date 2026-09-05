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

## Pending exact v002 observation

Source slice `9414ffc93b70aae51104b20c81a974b890eecf5a` is committed. The [v002 checkpoint](../../verification/reviews/device-session-bootstrap-diagnostic-v002/operator-packet.md), SHA-256 `17536739901f87628c518f0ac2687fec983e6ababe55fc0f58bf260a65199a80`, binds the exact 10,659,808-byte Release executable `ecf3f273f48e8f8a1dd72e15e85ed401584c8aef8669beea8c9010559669d304`. No v002 approval or launch has occurred. All unattended diagnostic work is complete; the actual root-repair criteria remain open. The card parks awaiting the owner. No temporary worktree or inspection logs remain; the existing Release root is retained for this pending consumer.

## Exact v002 approval received

The owner supplied the exact [v002 operator response](../../verification/reviews/device-session-bootstrap-diagnostic-v002/operator-response.md). Packet and binary digests were reverified, and native app inventory showed no existing instance. The integrator resumes the one sign-in-only diagnostic run with explicit Sign out and quit.

The approved v002 executable was launched once with no arguments after another digest check, excluding the specified 24 environment variables and discarding child output. Native UI showed no connected host and the expected GitHub.com Native HTTPS profile; the window was raised successfully. Operator device/browser authorization is pending. Continue this existing run; do not launch again.

## v002 result: GraphQL error envelope

The [v002 attended result](../../verification/reviews/device-session-bootstrap-diagnostic-v002/attended-result.md)
records the exact fixed diagnostic: capability discovery detected a GraphQL
error envelope. Copy message, explicit Sign out, terminal disconnected UI and
normal exit 0 were observed. No PR was opened or publication attempted.
The category locates the `envelope.contains("errors")` branch after native
metadata admission and JSON parsing. The error contents were not retained;
no particular GraphQL rejection or root repair is established. V002 authority
is consumed. Continue bounded offline query/error-contract investigation.

## Exact GraphQL code discriminator verified

The non-rate error branch now reports sixteen exact allowlisted code tokens
from `type`, `extensions.type` or `extensions.code`, using fixed Qt text.
Every entry and supplied marker must agree; unknown or conflicting markers
remain generic or mixed/unclassified. Empty/non-array error fields and malformed
entries have separate fixed categories. Messages, paths, locations and unknown
strings are never displayed or copied. This reports a recognized token without
asserting a root cause or a stable provider vocabulary. Existing rate-limit
precedence, refusal semantics, schema acceptance and credential cleanup are
unchanged.

Dev and tests-disabled Release incremental builds pass. Worker checks passed
5/5 in 5.37 seconds; integrator reproduction passed 5/5 in 3.85 seconds:

```sh
ctest --test-dir build/dev --output-on-failure -R '^(github_capability_probe|github_provider|qt_composed_github_scale_(100|125|200))$'
```

Tests cover all three marker positions, agreement across records, conflicting
and unknown markers, synthetic secret values, malformed/empty shapes, unchanged
rate precedence and supported normalization, production provider forwarding,
fixed visible/copied text and replacement clearing. The complete six-file diff
and rendered FORBIDDEN fixture were inspected. This render is synthetic, not the
live v002 result. Scoped strict tidy across five TUs, six-file formatting,
code-simplifier review and diff hygiene pass. The initial build exposed a
test-only QJsonObject/QJsonDocument helper mismatch, corrected before final
passing verification. No full matrix, additional independent review or live
request ran; the narrow change reuses the existing accepted diagnostic pipeline.

All six consumed `/tmp/bpr0559-codes-*.log` files were reviewed and removed.
Existing dev/Release roots and ignored renders remain reused. No temporary
worktree was created. Root-repair acceptance remains open. Further live
observation requires approval for the exact changed executable.

## Pending exact v003 observation

Source refinement `f8c205f9c8cd1b6085957af53005e853c6e79534` is committed.
The [v003 checkpoint](../../verification/reviews/device-session-bootstrap-diagnostic-v003/operator-packet.md)
binds the exact 10,660,096-byte Release executable
`dd1f41a413f7f8d79a9a22190068a283e22b251d7f6650bfd33571033624f15c`.
No approval or launch has occurred. The card parks awaiting the owner after
all available offline work. A recognized code may narrow diagnosis, but an
unknown code can still leave the cause unresolved. Existing live approvals
are consumed; no PR or publication is included. The existing Release build
is retained for this pending consumer. No temporary worktree or inspection
logs remain. The portable and Mac ready lanes contain no other startable card.

## Exact v003 approval received

The owner supplied the exact [v003 operator response](../../verification/reviews/device-session-bootstrap-diagnostic-v003/operator-response.md). Packet and executable digests and length match. Native inventory showed no existing instance. The integrator resumes this single sign-in-only diagnostic run, followed by explicit Sign out and quit. No PR or publication is authorized.

The approved v003 executable was launched once with no arguments after a second packet/binary digest check, removing the specified 24 variables from the child environment and discarding child output. PID 67244 is the active diagnostic process. Native UI showed the GitHub.com Native HTTPS profile, no connected host and empty code fields. A subsequent Raise action reported that the user changed the app; foreground activation is therefore not claimed. Continue this existing run only. Operator authorization and its terminal result are pending, followed by explicit Sign out and quit.

## v003 completed with generic GraphQL error

The [v003 result](../../verification/reviews/device-session-bootstrap-diagnostic-v003/attended-result.md)
records the generic GraphQL-error envelope again. No recognized code was
reported. Copy message, explicit Sign out, terminal disconnected UI and normal
exit 0 completed. The exact authority is consumed; no PR or publication ran.
The actual error text/code remains unavailable, so no root-repair acceptance
criterion is met. Do not add another speculative code list or ask for a blind
retry. Next bounded task: design a useful safely redacted observation of the
actual server error, consistent with the provider/presentation boundary,
before preparing any additional live checkpoint. No new packet is pending.

## Logging design registered

The owner requested a logging RFC with AllTheThings-style verbosity and privacy
options. [RFC-BPR-0027](../../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md)
and its backlog entry/closeout cards now specify that work, including a useful
sanitized unfamiliar GraphQL error fixture. This is design registration only;
no logger is implemented, no further live run is authorized and BPR-0559's
root-repair criteria remain open.

## Basic logger follow-up

BPR-0602 (`3d109fd4`) now preserves sanitized capability error prose in a local
Quill log, with credential-free native/provider/file/composition evidence. This
removes the need for another UI-only diagnostic category. It does not identify
the previous live failure or authorize another sign-in; this card remains open.

## Pending logged v004 observation — 2026-09-05

The owner resumed diagnosis after BPR-0602. The live log path does not yet exist;
previous attended errors predate the logger. Read-only query inspection found
no concrete validation defect: the query uses standard introspection and its
recorded GitHub.com schema fixture already validates it. No speculative source
change or additional code whitelist is warranted.

The [v004 packet](../../verification/reviews/device-session-bootstrap-diagnostic-v004/operator-packet.md)
(SHA-256 `a12ba71a6b9ccc2f41157bf486c8c27a27b0cc31deadc3d240b310486b122538`) binds the existing 11,066,048-byte logging-enabled
Release executable `baf16706e2e0427e29b7c9ba2b36b22c6c3377d8c86d9fd3da74f9b799ed46a4`. Prior BPR-0602 verification is reused because
source/configuration/fixture inputs have not changed. No v004 approval or launch
has occurred. The packet permits one sign-in-only observation and local log
reading, explicit Sign out and quit; no PR or publication. Root-repair criteria
remain open pending the actual sanitized server error. This card parks awaiting
the exact owner verdict. Existing Release build is retained for that consumer;
no temporary worktree or new evidence/build root was created.

## Exact v004 approval received

The owner supplied the exact [v004 response](../../verification/reviews/device-session-bootstrap-diagnostic-v004/operator-response.md). Packet and executable hashes and executable length match; native inventory showed no app instance. Resume this single sign-in-only run and sanitized local-log reading, followed by explicit Sign out and quit. No PR or publication is authorized.

The exact approved v004 executable launched once at 2026-09-05T08:51:41Z, PID 3736, with no arguments, the specified 24 environment variables removed and child stdout/stderr discarded. Log baseline was zero bytes. Native UI showed GitHub.com Native HTTPS, no connected host and empty device-code fields. The new log exists and contains startup. A Raise action reported user UI movement; foreground activation is not claimed. Continue this existing process only; operator browser authorization and its terminal result are pending, then explicit Sign out and quit. No relaunch is authorized.

## v004 identifies the concrete query-policy failure

The [logged result](../../verification/reviews/device-session-bootstrap-diagnostic-v004/attended-result.md)
identifies `INTROSPECTION_LIMIT_EXCEEDED`. GitHub allows each introspection field
twice per query; our document repeats fields/args 17 times, enumValues three and
inputFields four. Viewer succeeds and capability HTTP 200 fails before schema
acceptance. Explicit Sign out, disconnected UI, normal exit 0 and logged shutdown
completed. V004 authority is consumed; no PR/publication ran. Continue offline
query repair and an exact repetition-limit regression. Do not weaken capability
validation or add another speculative diagnostic enum.
