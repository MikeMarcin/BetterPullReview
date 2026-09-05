---
id: BPR-0615
title: Connect whole-review host and public workflow
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0613, BPR-0614]
---

# Connect whole-review host and public workflow

Whole-review policy and provider recovery now exist, but the application still
selects only the source-line Comment path. Connect the complete PR-level review
workflow so Comment, Approve and Request Changes are ordinary, explicit user
actions with the account and revision shown before submission. BPR-0144 retains
the separate designated live-account checks after this implementation.

## Ownership and implementation boundary

Application: whole-review async host, typed projections and per-request
execution authority; bounded pending-request discovery through the recovery
port/store where needed. Core: compact decided control state for the existing
whole-review draft policy. Qt: a distinct whole-review summary/disposition
editor and its workbench entry. App: real provider/store/host selection,
request lifecycle, presentation and shutdown. Own their focused unit,
provider/storage, Qt and composition fixtures and CMake registrations. The
integrator owns documentation, board state and commits. Establish exclusive
worker paths before delegation; no concurrent source writers.

Preserve source-line editing and its existing publication course. Keep the
whole-review summary independent of the selected file/line. Do not migrate
unrelated JSON, expand deferred logging/schema work, change accounts, mutate
GitHub or revise a previously answered approval packet in this implementation.

## Acceptance

- The async host owns provider effects, stop sources and pending request
  correlation. Effect callbacks post typed terminals on the serialized decision
  lane; they never mutate core/Qt state. Every accepted request reaches one
  terminal, including cancellation/shutdown. Reject stale session/generation,
  draft and attempt identity before processing its summary or replacing state.
  Keep the decision service alive through host quiescence.
- Preserve the complete BPR-0613 request and one idempotency key through its
  attempt and restart recovery. Discover or restore pending requests within
  the exact account/PR scope, bounded by existing store capacity. Reopening or
  refreshing must not turn indeterminate publication into a fresh POST. Keep
  storage effects off the decision/Qt lane and recover read-only until the
  original outcome is known. Published receipts must validate against the
  original request before completing that exact core attempt.
- Resolve BPR-0614's known moved-head recovery limitation: an already admitted
  possibly-transmitted review may be found at its original head after the PR
  moves, using exact target/account identity and the frozen request/baseline.
  This read-only recovery must never authorize a write. Fresh publication still
  requires re-observation of the exact current expected head immediately before
  POST. Exercise the concrete lost-response, moved-head, original-review-present
  counterexample and assert zero recovery POSTs.
- Keep whole-review execution authority distinct from source-line authority.
  An earlier source-line approval cannot authorize a whole review. Wire the
  whole-review authority required by ordinary composition, binding target,
  acting identity, summary and disposition to the admitted request. Exercise
  refusal without authority, exact one-use admission, revocation and replay.
  Existing default-deny automated-test policy remains in force; no UI model,
  flag or fabricated approval may widen a live test course.
- Present host, owner/repository, PR number, expected head, actual acting
  identity, summary and selected disposition before Submit review. The normal
  workbench exposes this workflow without a selected source line. Comment and
  Request Changes require nonblank text; empty Approve works. Discard and
  pending/terminal enablement follow core decisions. File navigation preserves
  the PR-scoped draft; target/account changes cannot silently rebind it.
- Qt emits typed gestures and applies decided projections only. Show useful,
  selectable/copyable outcomes for refusal, pending reconciliation and success;
  do not report a timeout as definite nonpublication. Never expose credentials
  or protocol internals. Preserve focus, keyboard access, accessibility labels
  and supported narrow layouts.
- Ordinary composition selects the whole-review host/provider/store and
  retains them through pending work and shutdown. A public fixture must open
  a PR, use the normal controls for each disposition, observe the exact
  correlated result, and prove one submitted review with the expected account,
  head and prose. Cover no-anchor use, stale terminal, duplicate gesture,
  identity/head drift, failure, cancellation and restarted pending recovery.
  Retain the source-line public fixture as a regression check.

## Verification and continuation

Use focused host/core, provider/storage, Qt and real composition selectors.
Render representative normal-control states and inspect identity, disposition,
summary, status and layout; exercise keyboard/accessible controls deterministically.
Use isolated fixtures and ordinary build roots. Run affected architecture and
isolation checks, scoped formatting, documentation structure and diff checks.
No full matrix, live credentials or publication are needed for this slice.

After implementation, prepare BPR-0144's concrete live Comment/Approve/Request
Changes course using a suitable designated target/account. MikeMarcin authored
BPR-Test#1 and cannot use it to prove own-PR approval. Do not switch accounts,
create a PR or assume extra remote mutation authority. BPR-0143's four approved
mixed-client actions remain separately parked on the Computer Use restriction.
