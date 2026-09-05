---
id: BPR-0601
title: Verify and close RFC-BPR-0027
status: Backlog
rfc: RFC-BPR-0027
milestone: Post-Pack v2
depends_on: [BPR-0600, BPR-0602, BPR-0603]
---

# Verify and close RFC-BPR-0027

## Objective

Reconcile [RFC-BPR-0027](../../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md) against all linked
implementation cards, direct evidence and the shipped diagnostic path.

RFC base: `55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`.
Requested RFC set: RFC-BPR-0027; workstream position: 1 of 1.
Planned cards: BPR-0600, BPR-0601, BPR-0602, BPR-0603. Add subsequently allocated required slices to
this list and dependencies before closeout. Registration is not implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Standard/Verbose, independent identifying-detail privacy, Off, Open logs, Clear and export controls are reachable while signed out; accepted settings survive restart without increasing disclosure on invalid data. | Composed Diagnostics controls, four detail/privacy combinations, restart and invalid-setting fixtures | Not run; implementation deferred | None |
| AC-02: A composed failed GitHub sign-in yields a correlated local log with useful sanitized unknown GraphQL error detail, readable directly without UI copying, while UI wording and session/retry authority remain unchanged. | Credential-free native/provider-to-host-to-file failure fixture with unfamiliar code and useful sanitized message; compare UI/authority | Not run; implementation deferred | None |
| AC-03: Forbidden credentials and review content never reach queues, sinks, recent context, exports or fallback output under any detail/privacy combination, including malformed provider data and sanitizer failure. | Secret/private-content canaries across all sinks, queues, ring, export and failure fallbacks | Not run; implementation deferred | None |
| AC-04: Standard and Verbose retain their declared distinct detail; privacy changes, Off, Clear and sign-out honor acknowledged barriers and do not retain hidden transient context. | Manual-executor mode-change, Off, Clear, sign-out and queued-event transition cases | Not run; implementation deferred | None |
| AC-05: Anonymized and identifying-detail exports use the exact previewed scope, preserve useful event joins, handle malformed records safely, disclose irreversible capture-time redaction and never upload automatically. | Preview/snapshot export course with joins, mixed modes, malformed lines and zero-network observation | Not run; implementation deferred | None |
| AC-06: Event, queue, ring, file, aggregate and age bounds hold under overload, error storms, concurrent sessions, disk-full and rotation failures without blocking core decisions or Sign out. | Bounded overload/error-storm, concurrent-process, rotation, retention and disk-full fixture | Not run; implementation deferred | None |
| AC-07: Startup, cancellation, shutdown, partial-tail recovery and logger failure produce truthful availability/loss status without claiming unflushed events survived a crash. | Startup/cancel/quiescence, partial final line and sink-failure course | Not run; implementation deferred | None |
| AC-08: Core/host/adapter/Qt boundaries, explicit Quill lifetime, safe file ownership and scope separation hold through the shipped composition, with no raw logging bypass in protected transports. | Architecture guards, composed lifetime, fake filesystem attacks and profile-scope partition cases | Not run; implementation deferred | None |
| AC-09: The selected Quill dependency is pinned with license/provenance and packaging evidence; each claimed platform has native file, lifecycle and diagnostic-control evidence. | Pinned dependency/license/packaging inspection plus native logging/UI lanes on each claimed platform | Not run; implementation deferred | None |

## Completion rules

Record exact executed commands, results and producing commits for each criterion.
All required linked cards must be done, including slices allocated after entry.
Use focused changed-surface checks and a composed public path, with native and
attended claims limited to actual evidence. Preserve unsupported-platform and
crash-durability limitations. Run the documentation gate and git diff hygiene,
then reconcile RFC/index status only when canonical order and dependencies allow.

## Foundation evidence and resume

BPR-0602 completed the owner-authorized phase 1 in `3d109fd4`. Its card records
passing native/provider/sanitizer, file persistence/cap, blocked-stderr cleanup,
composed application and dependency/boundary checks. Those establish the basic
subset of AC-02, AC-03 and AC-06 through AC-09; they do not complete the full
numbered criteria above. No new live sign-in ran and no live root cause is claimed.

Next task, only when requested: BPR-0603's advanced retention, anonymization,
controls, export and native-platform work. BPR-0600 and BPR-0602 are done; this
closeout and the full RFC remain deferred.

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).
The bounded BPR-0602 confidentiality/lifetime review resolved all three findings;
no blocker remains in that subset. Opposite-family review was unavailable.
