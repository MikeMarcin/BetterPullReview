---
id: BPR-0255
title: Compose authorized asynchronous review publication
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0227, BPR-0254]
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

## Verification

- Application-host unit tests cover missing-grant zero effects, exact accepted
  right-side binding, left/absent/mismatched refusal, cancellation at every
  provider phase, supersession, stale terminal withholding, retry identity, and
  shutdown quiescence.
- Selected-diff host tests cover exact forward/reverse rename/copy endpoint
  paths and absence without presentation-path fallback.
- Qt tests prove distinct side/line selections emit distinct semantic intent
  and the presented draft anchor equals the accepted selection.
- A shipped-executable fixture contract drives QAction to semantic selection,
  exact path/current-head binding, async host, fixture provider, typed terminal,
  and reconciliation; construction, grant, selection, and host-dispatch cuts
  fail independently.
- Tests-disabled Release, composed-host, live-isolation, architecture,
  documentation, scoped format/tidy, and `git diff --check` gates remain
  authoritative.
