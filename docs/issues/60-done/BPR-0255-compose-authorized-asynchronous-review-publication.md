---
schema: "implement-rfc.card.v1"
id: "BPR-0255"
title: "Compose authorized asynchronous review publication"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0227","BPR-0254"]
---

# Compose authorized asynchronous review publication

BPR-0254 safely constructs the exact designated live provider but leaves it
dormant. Publication still belongs to the synchronous fixture coordinator,
the draft anchor is hard-coded to right line 1, and the provider port discards
the native transport's stop token. The next portable boundary is an
application-owned asynchronous host that can bind a real semantic selection
without making live publication generally available.

## Required final state

- A review-mutation application host owns the coordinator/provider effect,
  request identity, one monotonic stop source per accepted request, exactly-once
  terminal admission, supersession, stale-completion refusal, and bounded
  shutdown quiescence. Qt dispatch returns without waiting for provider or
  network work.
- Mutation observation and submission ports propagate the host's
  `std::stop_token` through the coordinator, live provider/session, and native
  HTTPS transport. Cancellation before or between viewer, target, and POST
  produces a typed terminal and no later write.
- Selected-diff observation/publication retains the exact resolved left and
  right repository paths, including rename/copy and reverse comparisons. No
  presentation path is substituted for a resolved repository path.
- The shipped diff workbench emits the selected `core::semantic_selection`.
  Composition binds its comparison digest, selected file, workbench generation,
  selected-diff key, side, line, path, commit, and current designated head before
  creating a draft or mutation request.
- This slice supports only right-side selections whose resolved commit is the
  exact designated head. Left-side, absent-side, unbound, stale, mismatched, or
  superseded selections refuse with a typed result; they are never silently
  remapped to right line 1.
- The production async host may retain the exact provider/coordinator but has no
  execution grant by default. A non-forgeable application capability is required
  before any effect is queued. No boolean, environment variable, CLI switch,
  persisted nonce, or ordinary Qt gesture can create that capability.
- Fixture composition may supply only its testing grant and proves the real
  QAction/selection-to-host path. Tests-disabled production proves the missing
  grant causes zero observation, submission, credential, or network work and
  that there is no direct provider/coordinator bypass outside the async host.
- Shutdown revokes admission, requests stop, drains only owned effects and
  decision completions, withholds stale presentation, and releases provider and
  lease authority only after the host is quiescent.

## Non-goals

- Do not issue a live request, acquire or use a write credential, or execute the
  designated mutation lane.
- Do not implement the BPR-0142 one-shot grant factory or weaken BPR-0235's fixed
  harmless runner/canary.
- Do not support left/base-side GitHub review comments in this slice.
- Do not claim live GitHub, attended web-UI, Linux, Windows, provider-restart
  reconciliation, or RFC closeout evidence.

## Implementation record

On 2026-08-11 the portable slice composed a serialized review-mutation
application host with one stop source per accepted request, supersession,
stale-terminal withholding, and shutdown quiescence. Selected-diff
publications now retain epoch-sealed provenance for both exact resolved
endpoints. The Qt workbench emits its semantic selection, and composition
admits only the exact right/current-head endpoint before deriving the provider
path and commit. One host-owned drain keeps provider effects serial even under
a parallel executor.

Selection changes during publication retain the latest emitted selection
without replacing the publishing draft. Returning from A to B to A before the
terminal therefore adopts A. The matching terminal always clears pending state
and reconciles server projection before the deferred selection is presented. A
single unavailable result may retain its exact draft identity for safe retry;
different anchors and authorities cannot restore it, and success, invalidation,
composition retirement, and shutdown clear it. Typed editor clearing removes
stale selection-bound text and actions without emitting a semantic edit.

The execution authority is an opaque application capability with no production
issuer. The tests-disabled product supplies an empty grant, while the fixture
build alone can mint its testing grant. The epoch endpoint issuer is a private
host seam, both fixture issuers are macro-sealed, and production syntax probes
prove external translation units cannot acquire their constructor access.
Source and binary cuts require all publication to pass through the asynchronous
host and forbid direct
provider/coordinator observation, submission, or publication. A full-source
digest locks the composition TU. A deterministic sorted path plus LF-normalized
content-digest manifest binds every production app TU/header, independently of
the exact file inventory and composition semantic checks. No live lane,
credential, or network request was used.

## Verification

- Application-host unit tests cover missing-grant zero effects, exact accepted
  right-side binding, left/absent/arbitrary-path refusal, cancellation at every
  provider phase, a parallel-executor maximum concurrency of one,
  supersession, stale terminal withholding, retry identity, and shutdown
  quiescence.
- Selected-diff host tests cover exact forward/reverse rename/copy endpoint
  paths and absence without presentation-path fallback.
- Qt tests prove distinct side/line selections emit distinct semantic intent,
  the presented draft anchor equals the accepted selection, and typed clearing
  removes stale draft/receipt state while disabling every action.
- A shipped-executable fixture contract drives QAction to semantic selection,
  exact path/current-head binding, async host, fixture provider, typed terminal,
  and reconciliation. Its delayed A-to-B-to-A course holds the ambiguous A
  terminal, clicks B and returns to A, then proves A is the final
  workbench/deferred/draft anchor and retry identity after reconciliation and
  pending retirement. Construction, grant, selection, host-dispatch,
  generic-port/coordinator indirection, pointer-to-member access, spaced or
  commented member access, fragmented includes, second-app-TU authority, and a
  second live factory call fail independently.
- Tests-disabled Release, composed-host, live-isolation, architecture,
  documentation, scoped format/tidy, and `git diff --check` gates remain
  authoritative.

## Completion record

- The final focused portable matrix passed 12/12 after rebuilding the app,
  application hosts, Qt workbench/editor, and GitHub provider targets.
- The delayed public course proves stale-head refusal, an indeterminate A write,
  A-to-B-to-A selection while its terminal is held, server reconciliation,
  pending retirement, exact retry identity restoration, one duplicate receipt,
  and final server/Qt agreement.
- Production-flags syntax probes prove external translation units cannot mint
  epoch endpoint provenance or the fixture execution grant. All direct-bypass
  attacks fail, including generic-port/coordinator indirection,
  pointer-to-member spellings, comments/whitespace, fragmented includes, a
  second production app TU, and a second factory call.
- The tests-disabled Release app contains the asynchronous host, coordinator,
  live provider, and provider factory while excluding the testing issuer and
  fixture/reconciliation authorities. The configured non-production manifest
  remains free of live mutation authority across 98 binaries.
- Scoped format and warnings-as-errors tidy, documentation validation, and
  `git diff --check` passed. Independent critical rereview returned CLEAR with
  no remaining blocking or high-risk findings.
- No production execution-grant issuer, credential access, network request,
  live mutation, Linux replay, or Windows replay was introduced or executed.
