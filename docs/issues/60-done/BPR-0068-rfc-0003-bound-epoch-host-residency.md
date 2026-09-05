---
schema: "implement-rfc.card.v1"
id: "BPR-0068"
title: "Bound epoch host job and ingress residency"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0045","BPR-0055","BPR-0056"]
---

# Bound epoch host job and ingress residency

The RFC-BPR-0003 application host claims one bounded FIFO actor per repository,
but `authority_effect_actor::Queue`, the host `Jobs` ledger, and reentrant
`DeferredIngress` are structurally unbounded. Cancellation and supersession
request stop without retiring queued closures or job controls, so accepted
refreshes behind a stalled effect can grow resident host state until the actor
and decision lane drain.

Make the retained `Jobs` collection the explicit global epoch-host admission
ledger with a public operational capacity. Admit a job only when the control,
actor closure, worker submission, and eventual serialized completion can all
remain within that bound. Every queued, running, or completion-pending actor
operation must retain exactly one ledger entry, so the invariant bounds total
jobs, the sum of actor FIFOs, each individual FIFO, and decision completions
awaiting retirement. Release the slot before dispatching its terminal so
terminal-driven replacement work can proceed.

Do not silently erase canceled FIFO entries or weaken per-repository ordering.
Capacity refusal must produce the exact request-specific
`quota_exceeded` terminal; allocation/executor refusal must produce
`unavailable`; rejected offers must revoke their smart authority and publish
the matching bounded diagnostic. Roll back partial admission before invoking
any terminal.

Give `DeferredIngress` a separate explicit capacity. Ordinary accept/dispatch
overflow must be rejected before core acceptance with exact cleanup and one
bounded deferred diagnostic. Invalidation and shutdown are terminal-priority:
deduplicate or replace matching lower-priority deferred work as necessary,
never lose those events, and retain FIFO order among admitted ordinary
entries. Include every epoch-host-owned deque in the audit; record the similar
GitHub-host ingress surface as separate future scope rather than expanding
RFC-BPR-0003 silently.

## Required direct evidence

- A deterministic manual-executor refresh flood exceeds the job capacity
  while effects and decisions are withheld. It proves one actor drain,
  capacity-bounded jobs/closures/completions, exact quota terminals,
  stale/canceled older work unable to publish, slot reuse after retirement,
  and shutdown quiescence with work pending.
- A reentrant projection-callback flood exceeds the ingress capacity and mixes
  accept, dispatch, matching invalidation, and shutdown. It proves projection
  depth remains one, rejected leases are revoked, diagnostics remain bounded,
  invalidation/shutdown survive, and exact quiescence is reachable.
- Focused host, core, maintenance, persistence, sanitizer, architecture,
  documentation, workflow-absence, and diff-hygiene gates pass from relevant
  clean builds.

Workers remain Qt-free, no semantic state moves into the executor, and no
GitHub Actions or remote mutation is involved.

## Scheduling disposition

This resource-hardening claim remains required before final RFC-BPR-0003
Auto-Verified closeout, but it does not block replacement RFC-BPR-0004
implementation by product-owner direction. Resume it as an explicit issue
slice; do not silently broaden RFC-BPR-0004 or weaken this card's flood and
quiescence evidence.

## Implementation

- `epoch_host_limits` is a public validated record with default capacities of
  64 and hard maxima of 1,024 for the retained job ledger and ordinary
  reentrant ingress.
- `Jobs` is the mutex-protected admission ledger. An actor closure, running
  worker, or serialized completion retains the same `job_control`; completion
  retires it before dispatching its terminal and reserves that slot for one
  core-emitted dependent request until the terminal returns. The synchronous
  diff blob bridge uses the same admission, cannot consume that reservation,
  and retires before waking its caller.
- Capacity refusal reaches each core request's specific `quota_exceeded`
  terminal. The blob port has no host-capacity status and therefore returns
  generic host failure rather than falsely reporting its file-size limit.
  Recoverable admission/control/type-erasure allocation and effect-executor
  refusal remove partial admission before `unavailable`. Decision-completion
  callables are allocated during admission. Rejected offers revoke their smart
  authority without superseding the newest already admitted offer.
- Ordinary reentrant accept/dispatch work remains FIFO and cannot exceed its
  separate capacity. Overflow revokes rejected offer leases and retains one
  deferred diagnostic for the turn. Matching invalidation removes/revokes
  queued offers and uses two fixed active/pending priority slots; shutdown uses
  one fixed priority flag and supersedes ordinary work.
- `materialize()` is the sole concurrent epoch-host ingress. A close-aware
  call counter participates in shutdown quiescence, caller cancellation is
  forwarded into host job control, and a separate actor guard authorizes only
  the active application-session and pull-request authority. Replacement,
  matching invalidation, and shutdown revoke that guard synchronously; both
  admission and actor execution fail closed after revocation. The actor checks
  authority and stop state again after the cache call before transferring
  bytes. Result publication uses a non-consumable counted actor-exit
  reservation through waiter notification.
- Cancellation marks and selects controls under the job mutex into a fixed
  maximum-capacity snapshot, releases the mutex, and only then invokes
  `request_stop`. Nested sweeps exclude already-issued controls, so synchronous
  stop callbacks can reenter shutdown, the closed blob seam, or public counters
  without deadlock or recursive stack growth.
- Replacement of a context with retained append preparation pre-admits a
  cleanup job. Different-authority cleanup must be successfully queued before
  active-authority mutation or new-offer submission; same-authority cleanup is
  queued before core acceptance. Quota, allocation, or executor refusal
  preserves the active context. Empty and observation-only preparation is
  retired synchronously and cannot consume the offer terminal reservation.
  The cleanup control is protected from the replacement's ordinary authority
  cancellation sweep; shutdown cancellation remains available for recovery.
  With different repository actors, both cleanup and offer submissions must
  succeed before active cancellation/invalidation. Refusal of the second post
  preserves the old authority while the already-admitted cleanup finishes.
- The GitHub application host's similar ingress queue is deliberately
  unchanged and remains separate future scope. Its smart-authority revocation
  path now swaps its weak-transport set under lock so revocation itself does
  not allocate. Core semantics, Qt, Git/SQLite ports, provider traffic,
  dependencies, schemas, packaging, and GitHub Actions are unchanged.

## Direct evidence matrix

| Claim | Direct surface | Evidence | Current result |
|---|---|---|---|
| Global job/actor/completion residency is bounded | `application_epoch_host` with capacity 3 and withheld manual executors | Four refresh offers produce three jobs, three queued closures, one actor drain, three pending completions, one exact quota diagnostic, newest-admitted publication, terminal-driven slot reuse, and shutdown quiescence | Passed in the focused Qt-free test |
| Every actor closure uses the same ledger | Synchronous `diff_blob_port::materialize` plus history-page intent at capacity 1 | Blob work owns the only job/actor slot; the core request receives `quota_exceeded`; blob completion retires before waking | Passed in the focused Qt-free test |
| Partial admission rolls back before failure | Injected effect-executor refusal | Zero retained jobs/closures, one `unavailable` diagnostic, rejected lease revoked | Passed in the focused Qt-free test |
| Terminal cleanup cannot lose admission | Canceled prepared refresh plus reentrant blob at capacity 1 | The terminal reserves its retired slot, the blob receives generic failure, and the core discard consumes the reservation and durably aborts | Passed in the focused Qt-free test |
| Concurrent ingress remains exact | Blocking/rejecting executor plus two blob callers | Both jobs admit; refusal removes only its own closure; the later actor work remains and completes | Passed in the focused Qt-free test |
| Blob authority fails closed | Accepted comparison, saved request, matching invalidation, retained actor/cache | Materialization fails synchronously without a blob read, job admission, or actor submission | Passed in the focused Qt-free test |
| In-flight revocation cannot publish bytes | Blocking cache fake deliberately returns bytes after ignoring stop | Matching invalidation revokes during the read; the post-read guard discards bytes and the actor-exit reservation remains counted through notification | Passed in the focused Qt-free test |
| Reentrant terminal ingress closes blob access before return | Projection callback invokes matching invalidation or shutdown and then immediately materializes a saved request | Invalidation cancels/revokes synchronously; shutdown closes/cancels synchronously; neither path admits actor/cache work before deferred semantic teardown | Passed in the focused Qt-free test |
| Stop callbacks never run under or recursively grow the ledger sweep | Blocking blob fake registers a stop callback that reenters shutdown and `materialize()` | Cancellation is marked before callback invocation; nested shutdown does not restop the job, callback depth is one, closed ingress returns generic failure, and the outer call cancels/quiesces | Passed in the focused Qt-free test |
| Replacement cannot lose mandatory cleanup | Capacity-2 active prepared commit plus different-PR offer; capacity-3 prepared completion plus injected cleanup-executor refusal; successful same- and different-authority prepared-append replacements | Quota and executor refusal preserve the active authority and never queue/open the replacement; successful cases record non-stopped exact abort calls and durable `aborted` before replacement ingestion | Passed in the focused Qt-free test |
| Cross-repository offer refusal cannot retire the active authority | Sequenced executor accepts A cleanup then rejects B's distinct repository actor post | A remains authoritative with no invalidation, B is revoked/unavailable, and the protected cleanup remains bounded and drainable | Passed in the focused Qt-free test |
| Same-authority ingestion retains its terminal slot | Capacity-1 ready authority plus same-authority refresh | Empty prior preparation consumes no cleanup job; the offer terminal reservation admits the core-emitted catalog load | Passed in the focused Qt-free test |
| Shutdown counts and cancels entered blob calls | Cache fake blocks inside materialization until host stop | Shutdown stop reaches the cache call; quiescence stays false until worker and caller return, then becomes exact | Passed in the focused Qt-free test |
| Reentrant ordinary ingress is bounded and FIFO | Projection callback with capacity 3 plus ordered replacement offers | Maximum observed ordinary queue is 3, overflow is one diagnostic, final admitted snapshot proves FIFO, projection depth is one | Passed in the focused Qt-free test |
| Invalidation/shutdown cannot be crowded out | Reentrant accept/dispatch/invalidate/shutdown flood | Matching queued leases and active lease are revoked exactly once; fixed priority state survives overflow; shutdown drains to exact quiescence | Passed in the focused Qt-free test |

## Documentation ledger

| Scope | Disposition | Evidence |
|---|---|---|
| Application public limit and lifecycle | Updated | `epoch_runtime_ports.hpp`, `epoch_application_host.hpp`, system architecture, Git/epoch architecture |
| Operator/test guidance | Updated | BPR-0045 host command documents the new deterministic floods; the target name is unchanged |
| RFC-BPR-0003 verification | Updated | Component description, documentation ledger, performance plan, and direct-claim row track the implemented capacities and completed available-surface replay |
| Core, Qt, provider ingress/traffic, persistence, schema, package, remote mutation | No impact | Core/Qt/Git/SQLite/provider-ingress/package/workflow behavior is unchanged; no provider traffic is added |
| Smart-transport authority revocation | Updated | GitHub ingress policy is unchanged; revocation swaps the retained weak set under lock before canceling transports so the required cleanup does not allocate |

## Verification log

- The selected developer build and 12-target sequential component matrix pass
  12/12, including architecture, core epoch policy, GitHub/epoch hosts, SQLite
  catalog storage/behavior, and maintenance integration.
- A fresh ASan/UBSan build passes focused `application_epoch_host` 1/1. The
  earlier reusable sanitizer tree had stale removed options at regeneration;
  it did not execute a test and was superseded by this fresh build.
- The final focused host replay passes 1/1 after the fixed-priority,
  cancellation-snapshot, mandatory-cleanup, and cross-repository refusal
  repairs. The selected matrix and sanitizer replay are Qt-free and launched
  zero application instances; existing public persistence launch evidence was
  deliberately not repeated.
- Repository formatting passes. The complete strict first-party clang-tidy
  audit reported only six static-helper call-style findings in the touched
  host; after qualification, its strict touched-file rerun passes.
  Documentation, immutable-RFC, workflow-absence, and diff-hygiene gates pass.
- Independent critical review reports no blocking or high-risk nonblocking
  findings. BPR-0086 records its optional allocator-failpoint and additional
  invalid-limit boundary coverage without reopening this completed slice.
