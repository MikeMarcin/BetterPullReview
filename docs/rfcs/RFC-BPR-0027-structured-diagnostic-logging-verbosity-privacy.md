---
id: "RFC-BPR-0027"
title: "Structured Diagnostic Logging, Verbosity, and Privacy"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0001","RFC-BPR-0002"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# RFC-BPR-0027: Structured Diagnostic Logging, Verbosity, and Privacy

## Status and scope

**Auto-Verified under owner amendment r3, 2026-09-27.**
All nine product criteria and all three human judgments are accepted. M1–M5
implementation is landed on main. The owner removed the obsolete global
canonical-order rule and explicitly directed RFC-0027 closeout; its declared
RFC-0001/0002 dependencies are Auto-Verified. [BPR-0601](../issues/60-done/BPR-0601-close-rfc-0027.md)
and the [final ledger](../verification/RFC-BPR-0027.md#owner-amendment-r3-and-terminal-closeout)
bind the complete acceptance and actual main landing. Native developer, package
and crash evidence keeps its exact limitations. Historical phase descriptions
below record provenance; they do not defer the implemented advanced phase.

Registration baseline: `55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`.
Entry: [[BPR-0600]]. Foundation: [[BPR-0602]]. Advanced phase: [[BPR-0603]].
Closeout: [[BPR-0601]]. Full acceptance closes this RFC under its explicit
dependencies; the global canonical-order rule was removed by amendment r3.
Its logging foundation can be implemented without the later Settings center or
multiple-profile system; those integrations use the same typed ports when
available, without creating reverse dependencies.

## Authorized implementation phases

**Historical phase 1 — BPR-0602:** pin Quill; explicitly own its backend lifetime;
write startup/shutdown and GitHub viewer/capability operation outcomes to one
fixed owner-local log file; retain a bounded useful capability GraphQL error
summary before response disposal. Use a nonblocking bounded queue, capped
records and a fixed file byte ceiling. When the file fills, stop recording and
report that state; no rotation or automatic deletion is implemented. The file
survives quit and is directly readable at a documented path. Logging failure
must not change sign-in, retry or cleanup behavior.

Only the static capability query's bounded error message/code fields may supply
remote diagnostic prose in this phase. Filter active credential reflections and
credential-shaped material within the GitHub adapter before a summary reaches
the logging port. Exclude authentication bodies, headers, URLs, requests,
arbitrary extensions, source/review content and all other response bodies.
Retained prose may still identify the service or account; local logs are not
anonymized or automatically safe to share. This distinction replaces the full
phase-2 anonymization default for the limited initial implementation.

**Phase 2, implemented under mandate r2 — BPR-0603:** rotation/age/aggregate retention, identity
anonymization, Standard/Verbose and privacy/Off Settings controls, recent-context
ring, preview/export/Clear/Open-logs UI, expanded operation coverage and later
profile integration. The full acceptance criteria below remain the RFC closeout
contract. BPR-0602 has its own bounded acceptance and may finish while phase 2
and the overall RFC remain deferred. No Settings dependency blocks phase 1.

## Phase-1 implementation record

BPR-0602 implements the foundation in `3d109fd4`: Quill v13.0.0, an explicit
application port and logging adapter, composition lifetime, and sanitized native
capability-error detail. The file is `<ApplicationDataRoot>/logs/diagnostics.log`,
append-only with a 16 MiB ceiling and 2,048-byte summaries. Availability/full/drop
states are typed; there is no Settings indicator or stderr fallback yet.

BPR-0642 extends this fixed-file foundation to pull-request loads. Each
window-local load records a start, correlated paged operation and HTTP status
observations, and one accepted or failed terminal with a typed GitHub or
snapshot-validation failure. Numeric window-session and request fields join
interleaved windows; operation names are closed vocabulary, and the added
events admit no provider text or review content. This narrow load trace does
not implement phase-2 controls or authorize a live service read.

Focused native/provider/sanitizer, real file, blocked-stderr cleanup and composed
application checks pass on macOS. Development and tests-disabled Release builds
pass; the core-only graph omits Quill. See [[BPR-0602]] for exact evidence and
[local logging instructions](../development/build-and-test-reference.md#basic-local-diagnostics).
Windows file logging is explicitly unavailable; Linux native evidence, advanced
controls and full RFC closeout remain deferred. No new live sign-in ran during
that foundation slice. The subsequent approved BPR-0559 observation used this
log to identify GitHub's introspection repetition limit; `b14a69d0` repairs the
query plan with focused offline evidence. BPR-0605 subsequently observed live
sign-in success through that log and the native UI, followed by explicit Sign
out. Its result separately records an unintended signed-out relaunch during a
post-quit UI inspection; strict one-launch compliance is not claimed.

## Problem and intended outcome

BPR-0559 required repeated attended sign-ins and copied UI messages to locate a
capability-discovery failure. The UI properly hid arbitrary provider data, but
there was no persistent diagnostic channel behind it. The parser discarded the
actual GraphQL error, and the attended launcher discarded stdout/stderr.
Successive enum lists still could not explain the failure.

BetterPullReview needs useful local logs that a developer or an authorized
coding assistant can read directly. User-facing messages remain concise.
A local log must connect an operation to its actual failure, build and request
context without requiring another binary just to add a recognized error code.
Writing the current generic diagnostic to a file does not satisfy this RFC.

The scope is structured application diagnostics with Quill, independent detail
and privacy controls, bounded retention, and deliberate local export. It covers
startup/shutdown, provider operations and normalization, authentication stages,
Git/cache/storage failures, cancellation and task lifecycle. High-frequency
rendering/search details are opt-in and bounded. Logs are not the review ledger,
an audit authority, crash dumps, telemetry, or a capture of source and reviews.

## AllTheThings reference and deliberate differences

Read-only reference: `/Users/jaeger/Documents/GitHub/AllTheThings`, commit
`7edd398571a867452e9fd9b7dc5ae6b73d0cd645`; the four inspected files were clean.

| Reference source | Observed behavior to adapt |
|---|---|
| `Sources/AllTheThings/SettingsWindowController.swift` | Diagnostics card with **Standard / Verbose** detail selection. |
| `Sources/AllTheThings/AppSettings.swift` | Standard default; accepted level changes persist and update the logger. |
| `Sources/ATTCore/DiagnosticLogging.swift` | Typed privacy-tagged fields, JSONL events, asynchronous writing, rotation, bounded field sizes, recent diagnostic context, raw and anonymized export. |
| `Sources/AllTheThings/AppDelegate.swift` | Separate export actions, an explicit warning before raw export, clear-local-logs action; 50 MiB total and 30-day retention configuration. |

AllTheThings uses a custom Swift logger, not Quill. Its UI uses two detail
choices although its underlying severity enum has more levels. Its export
anonymization is separate from recording detail. BetterPullReview adopts these
product distinctions, adds explicit control of locally retained identifying
details, and does not copy its Swift singleton, anonymizer, path-heavy payloads
or index snapshots. Reference behavior is not evidence that BPR has shipped it.

## User controls

Expose **Diagnostics** through Help immediately when this feature ships. When
RFC-BPR-0018 is present, its Storage & Privacy page hosts the same controls;
Help raises that page. Do not require the full Settings redesign to obtain logs
for an otherwise unusable sign-in. All actions remain available while signed
out or while a provider is failing.

| Control | Proposed behavior |
|---|---|
| Record local diagnostics | On by default; Off stops new file records and clears recent context after an acknowledged barrier. It does not silently delete older files. |
| Diagnostic detail | **Standard** by default; **Verbose** adds bounded operation detail and timing. Severity and detail are distinct: warnings/errors remain recorded whenever logging is enabled. |
| Identifying details | **Anonymized** by default; **Include locally** retains classified account/repository/host/path identifiers in future local records. Explain the effect before accepting the change. This never enables credential or review-content logging. |
| Open logs folder | Reveals the actual host-selected log directory; shows an explicit unavailable state when it cannot be opened. |
| Export anonymized diagnostics | Default sharing action, with a preview of scope, time range, size and categories. Produces a new local file after fresh anonymization. |
| Export with identifying details | Separate action with an explicit disclosure and confirmation. Exports only detail already retained; cannot recover identifiers removed at capture time. |
| Clear local diagnostics | Confirms the current scope, pauses/drains accepted work, removes owned logs and recent context, then reports completion or exact failure. Already exported files remain outside this action. |

Verbose does not turn privacy off; including identities does not turn verbosity
up. Labels explain local recording versus export separately. Changes reflect
accepted host state, including failures, rather than optimistic widget values.
Normal preferences persist; unknown settings/schema values default to Standard
and Anonymized and report migration/refusal safely. Off remains Off at restart.

Before BPR profiles exist, preferences and logs belong to the existing default
environment. RFC-BPR-0024 later binds them to each BPR profile: profile-local
files, preferences, queues and export selections; its windows share that host's
service. Export never silently combines profiles. App-wide startup failures
use a separate minimal metadata-only stream with no profile payloads. Concurrent
processes have separate session files and coordinated pruning, not shared-file
append races.

## Event contract and diagnostic usefulness

Use versioned JSONL records with a stable event name/category, severity, UTC
time, monotonic elapsed time, session identity, per-session sequence, build
identity, operation identity and optional parent/correlation identity. Stable
local event names are distinct from remote error codes. Record request start,
terminal outcome and cancellation so unmatched operations and dropped records
are visible. Log identities are diagnostic only and grant no authority.

Standard retains operation names, endpoint templates, status, duration, bounded
rate-limit facts, safe error explanations, cleanup outcomes and correlation.
For example, `github.capability_probe.failed` must include the probe version,
HTTP status, parsing/normalization stage and useful sanitized GraphQL error
information. It must distinguish a service rejection from malformed data.
Verbose additionally retains bounded intermediate steps, retry decisions,
selected schema locations and performance context. No full transport dump is
an available level.

Parse the bounded GraphQL error fields at the provider boundary before the
normalizer drops the response: error code/type, message, source location and
path where relevant. Classify their fields, remove forbidden material, apply
the selected identifying-detail policy, then enqueue the sanitized event.
Preserve useful error prose and public schema symbols; do not replace every
unfamiliar error with another enum fallback. Free text is untrusted and may
contain private values. Unclassifiable spans are omitted with an explicit
reason; redaction is not claimed to make arbitrary provider text harmless.
The implementation must define and test this sanitizer against representative
unknown errors before a live run. Failure of sanitization cannot release the
original value through another sink.

Authentication events record stage/outcome and correlation only: device-code,
token-exchange and refresh response bodies are never logged. The credential
owner sanitizes any admitted provider error before it leaves that boundary;
credentials are not passed to a general logger for later filtering. A permitted
GraphQL schema-probe error is a distinct contract from an authentication-body
capture. UI diagnostics and publication/session acceptance remain unchanged.

## Privacy and export contract

Fields have explicit, typed classes rather than an unstructured format string:

- **Operational metadata:** local event names, endpoint templates, counts,
  timings, status and bounded schema/contract facts.
- **Identifying context:** account/repository/host names, local paths, request
  IDs and other classified identifiers. Default capture replaces these with
  opaque correlation labels. Include locally may retain them.
- **Provider error detail:** bounded untrusted diagnostic fields, sanitized and
  classified at their adapter before the logging boundary.
- **Never recorded:** tokens, authorization/cookie headers, device/user codes,
  token responses, private keys, passwords, credential-bearing URLs, source
  blobs, diffs, PR/comment bodies, unpublished drafts, notes, prompts and agent
  output. No verbosity or privacy option overrides this class.

Sanitize and cap before Quill or any queue, temporary export buffer, stderr
fallback or secondary sink receives an event. There is no raw-response fallback
on parser, logger or exporter failure. Do not promote remote strings into the
operational class merely because they look like identifiers.

Default anonymization uses opaque labels stable within a local session, with
fresh mappings per export. Do not use unsalted hashes of usernames or paths,
or describe punctuation-preserving scrambling as an anonymity guarantee.
Labels preserve joins within the selected export without establishing identity
across exports. Error prose and operational metadata can still reveal context;
call the result anonymized, not guaranteed anonymous.

Export flushes a bounded snapshot, reprocesses every field under the selected
export policy and leaves originals intact. Anonymized export never includes a
raw local file as an attachment. Unknown/malformed records are skipped with
counts or cause an explicit refusal; they are never copied through verbatim.
Preview and confirmation refer to the same frozen selection. Exporting is a
local write only: no automatic upload, support message, repository attachment
or transmission to an agent. An authorized local assistant can read the same
permitted files directly without asking the user to relay UI text.

Changing privacy affects new records and clears queued/recent identifying
context under a host-owned transition barrier. Existing files are not claimed
to have changed; show their recorded mode and provide Clear. Tightening privacy
must prevent an older queued event from appearing after acknowledgement under
the new mode. Sign-out clears transient diagnostic context for that account;
retained files follow retention/Clear and their scope remains explicit.

## Quill integration and architecture

Use [Quill](https://github.com/odygrd/quill) behind a narrow logging capability.
Its [asynchronous backend and structured sinks](https://quillcpp.readthedocs.io/en/latest/overview.html)
provide transport and formatting, not privacy policy. At implementation, pin
an audited release, record license/provenance and configure queue/lifecycle
behavior explicitly; library defaults are not acceptance evidence. This RFC
adds no downloaded dependency or CMake target.

The core decides. The host performs. Qt presents.

- Core code retains deterministic decisions and typed outcomes; it acquires no
  logger, clock, thread, filesystem, Quill type or hidden global dependency.
- Application contracts define bounded diagnostic events, preferences and
  narrow recording/export capabilities. Effect adapters produce sanitized
  observations where the useful failure facts exist; hosts record lifecycle
  and accepted transitions. Observing an event does not change core legality.
- A dedicated effect adapter owns Quill, clocks, file I/O, rotation, retention
  and export work. The composition root explicitly owns startup, injection,
  quiescence and shutdown. Export/file-opening actions run through host ports.
- Qt presents accepted controls, status and export previews. It does not open
  files, format raw responses, own Quill or perform privacy decisions.
- Existing credential/native-transport logging guards must remain effective.
  Add only the narrowly typed sanitized boundary needed by this RFC, with
  negative tests; do not remove broad guards to permit printf-style dumps.

Start logging before fallible application services and flush after their
quiescence on ordinary quit. Logging failure cannot block the serialized core
lane, prevent Sign out, change retry policy or publish a successful session.
Crash recovery tolerates a partial final line and reports incomplete records;
this does not promise a durable final event after sudden termination.

## Resource and failure bounds

Proposed defaults: 5 MiB per file, 50 MiB total local files, 30 days maximum age,
4,096 pending events and 8 MiB queued bytes, 16 KiB per encoded event, 4 KiB per
string, 32 array values, and at most one second of ordinary flush latency.
A Standard recent-context ring may retain already-sanitized Verbose events for
three minutes, bounded to 512 events and 2 MiB. Off, Clear and privacy changes
also govern this ring. It is disclosed and only included by explicit export
selection; it is not a hidden raw-data recorder. Caps apply before allocation
or copying into the logger where the source boundary can enforce them.

At overload, drop Verbose/low-severity records first and retain bounded reserved
capacity for warnings/errors. An error storm is still bounded: coalesce/drop
with counters rather than allocating an unlimited priority queue or blocking
the UI. Emit loss summaries when space returns; if the sink never recovers,
show loss/unavailability in the diagnostics status. No recursive logging of
logging failures. Startup pruning and rotation respect aggregate caps across
sessions. Disk-full, permission, rotation, corrupt-record and shutdown failures
have explicit tested behavior and no insecure alternate path.

Use OS-appropriate per-user application log storage with owner-only access where
supported, outside the repository. Reject unsafe file targets and cross-profile
symlink/reparse traversal. Export destinations are selected through the host's
save capability. Signing and packaging must account for the backend on each
claimed platform; test-only seams do not enter production builds.

## Acceptance criteria

- AC-01: Standard/Verbose, independent identifying-detail privacy, Off, Open logs, Clear and export controls are reachable while signed out; accepted settings survive restart without increasing disclosure on invalid data.
- AC-02: A composed failed GitHub sign-in yields a correlated local log with useful sanitized unknown GraphQL error detail, readable directly without UI copying, while UI wording and session/retry authority remain unchanged.
- AC-03: Forbidden credentials and review content never reach queues, sinks, recent context, exports or fallback output under any detail/privacy combination, including malformed provider data and sanitizer failure.
- AC-04: Standard and Verbose retain their declared distinct detail; privacy changes, Off, Clear and sign-out honor acknowledged barriers and do not retain hidden transient context.
- AC-05: Anonymized and identifying-detail exports use the exact previewed scope, preserve useful event joins, handle malformed records safely, disclose irreversible capture-time redaction and never upload automatically.
- AC-06: Event, queue, ring, file, aggregate and age bounds hold under overload, error storms, concurrent sessions, disk-full and rotation failures without blocking core decisions or Sign out.
- AC-07: Startup, cancellation, shutdown, partial-tail recovery and logger failure produce truthful availability/loss status without claiming unflushed events survived a crash.
- AC-08: Core/host/adapter/Qt boundaries, explicit Quill lifetime, safe file ownership and scope separation hold through the shipped composition, with no raw logging bypass in protected transports.
- AC-09: The selected Quill dependency is pinned with license/provenance and packaging evidence; each claimed platform has native file, lifecycle and diagnostic-control evidence.

## Verification and implementation entry

[[BPR-0600]] inventories actual diagnostic loss points and translates this design
into bounded source-owned slices before implementation. Prioritize a useful
sanitized GraphQL failure record plus local file access as the first direct
course, then retention/privacy/export controls and platform lanes. Use synthetic
provider/credential fixtures first; a live sign-in is a separate attended gate,
not permission supplied by this RFC. [[BPR-0601]] maps every criterion to direct
checks and retains not-run status until evidence exists.

Relevant documentation changes at implementation: GitHub host contracts,
security/threat model, resource limits, build/dependency notes, developer
troubleshooting, privacy/retention and Settings ownership. This design supersedes
neither historical diagnostic packets nor their exact one-run authority.

## Non-goals

Remote telemetry, automatic support uploads, a log analytics service, arbitrary
user-selected sink plugins, source/review-content capture, crash dumps, changing
GitHub authorization, and replacing domain stores are outside this RFC.

## BPR-0971 implementation clarification — 2026-09-25

The standalone [completed diagnostic audit](../verification/bpr0971-completion.md)
implements the existing unknown-error and forbidden-content requirements with
closed explanation forms and exact public symbols from compiled capability
queries. Unknown codes, arbitrary symbols and unmatched/trailing prose receive
explicit omission reasons. Representative unfamiliar codes retain useful known
field/type/repetition explanations. This corrects the former arbitrary-text
retention; it does not waive privacy or complete later Settings/retention work.

## Owner amendment r2 — 2026-09-26

The owner now requests full RFC-BPR-0027 implementation and landing on main
after each accepted milestone. This supersedes the earlier advanced-phase
deferral, preserves all full acceptance criteria and the BPR-0971 sanitizer
clarification, and grants no live sign-in or PR mutation. BPR-1215–1219 bind
the remaining work and native/attended acceptance.


## M5 attended UI amendment — 2026-09-27

Under mandate r2, the owner accepted Open logs/Clear for source `7ab11993`
and requested two presentation changes in session
`c1565558e5fd3fdc277e62622604873b`, hand-back cursor 23: Settings should resemble
AllTheThings’ visible paired selectors, and exports should follow its simple
Help actions. This authorizes the compact Settings rows and direct Help export
dialog, retaining the same frozen preview, privacy and local-write boundaries.
The identifying dialog’s explicit Save confirms the displayed scope and policy;
Cancel remains the default. This amendment changes presentation, waives no
acceptance criterion, and requires new controls/export judgments. The accepted
files decision remains bound to its unchanged copied candidate.


## Owner amendment r3 — 2026-09-27

The owner directed: “the canonical order rule is outdated and should be removed
from the AGENTS.md. Go ahead and do that and close out 0027”. This removes the
global rule, rather than granting a one-RFC exception. Project sequencing now
uses the supported `dependency` mode. Explicit RFC/card dependencies, authored
milestone prerequisites, separately directed scheduling holds and actual
acceptance remain required. No product criterion, native claim or release
boundary changes. The r2 ordering hold is resolved; all three human Accept
judgments and their candidate identities remain unchanged.
