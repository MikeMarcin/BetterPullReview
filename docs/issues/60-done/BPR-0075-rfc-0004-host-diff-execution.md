---
schema: "implement-rfc.card.v1"
id: "BPR-0075"
title: "Implement host diff execution cache and stale rejection"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0074"]
---

# Implement host diff execution cache and stale rejection

Add exact blob materialization, immutable worker inputs, bounded asynchronous
diff execution, cache keys containing repository/PR/epochs/file/blobs/options/
algorithm revision, cooperative cancellation, serialized completion delivery,
and deterministic stale-result rejection.

Workers never touch Qt or mutable core state. Binary, missing, decode,
oversized, generated-policy, cancellation, invariant, and worker failures
remain typed and reason-coded.

## Required direct evidence

A manual executor reorders completions across repository, PR, epoch, file,
blob, options, generation, cancellation, shutdown, and cache hit/miss
boundaries. Same-path collision fixtures must miss when any exact identity
changes. Core/worker dependency scans must pass without Qt.

## Implementation

- Added a Qt/provider/effect-free `diff_transition` core boundary with exact
  application session, request, generation, full comparison key, pending and
  accepted identity, cancellation, authority invalidation, and constant-time
  stale/duplicate rejection. Every replacement intent first revokes older
  pending work, even when the replacement key, options, or algorithm is
  rejected.
- Fixed the selected diff implementation at algorithm revision `1`. The host
  rejects any other revision before cache lookup or blob acquisition.
- Added an exact blob-materialization port whose request carries context,
  side, OID, and the checked per-side byte ceiling. Worker closures own both
  byte vectors and create borrowed spans only for the synchronous pure-core
  computation call.
- Added a standard-C++ application host with separate effect and decision
  executors, stop observation before acquisition/between sides/after compute,
  exact result/key/options/digest validation, typed failure mapping, immutable
  publication, authority invalidation, and a weak lifetime gate.
- Bounded outstanding effects/terminals, cache entries, retained capacity
  bytes, and every compute limit. Cache lookup and deterministic FIFO
  eviction remain on the decision lane; exact hits do no materialization and
  still traverse serialized completion acceptance.
- Preserved validated binary and unavailable canonical results with their
  honest content state and diagnostic. Missing objects, adapter-declared
  limits/policy, cancellation, identity mismatch, invariant, submission, and
  worker failures remain typed terminals.
- Projection feedback is deferred through a fixed-snapshot application turn.
  Feedback created while processing that snapshot is scheduled onto a later
  decision turn, so delivery never nests or monopolizes the active turn.
  Shutdown and quiescence include staged/deferred work, and a pending effect
  cannot target a destroyed host.

## Direct verification

- Final clean `core-only` configure/build and complete matrix: 32/32 passed in
  76.66 seconds.
- Final developer build of both new targets plus the focused lifecycle and
  architecture matrix: 7/7 passed in 35.25 seconds
  (`core_dependency_boundary`, `core_boundary_contract`, both architecture
  boundary targets, `binary_dependency_audit_contract`,
  `core_diff_transition`, and `application_diff_host`).
- Fresh Darwin AddressSanitizer/UndefinedBehaviorSanitizer build and the same
  7/7 matrix: passed in 34.77 seconds with fail-fast runtime options. Leak
  detection remained disabled because Darwin AddressSanitizer does not
  support that mode; address and undefined-behavior instrumentation remained
  active.
- The deterministic host fixture directly covers lane separation, exact
  cache hit/miss and authority/repository/PR/anchor/file/blob/options/
  algorithm collisions, same-key and malformed-request supersession,
  reordered terminals, cancellation stages, shutdown, fixed-snapshot
  persistent reentry, pending-effect destruction, job/entry/byte/compute
  bounds, invalidation, absent-versus-empty sides, binary/unavailable results,
  typed failures, exact port echoes, submission/projection exceptions, and
  post-return size enforcement.
- Documentation validation passed with `DOCUMENTATION_OK`; `git diff --check`
  passed; `.github/workflows` remained zero tracked and zero on disk.

## Review, simplification, and remediation

The implementation worker remained within its twelve owned code/build/test
files. Independent critical review found four blocking gaps despite passing
tests: the acquisition port lacked a pre-allocation ceiling, canonical
nontext results were discarded, a malformed replacement could leave older
work publishable, and projection feedback could deliver recursively. Root
closed each gap with direct regressions. Re-review then found the first
fixture copied bytes before claiming refusal and the first deferred queue
could consume newly generated feedback without yielding; fixed-snapshot turn
processing, exact quiescence state, destructor stop behavior, and persistent
feedback evidence closed those findings. Final critical re-review reported no
remaining blocker or high-risk issue.

The conservative simplification pass retained explicit left/right acquisition
stages, typed ingress variants, core transition deltas, and cache accounting
because they encode stop boundaries, serialized ownership, and fail-closed
identity. It consolidated projection delivery and application-turn handling
without merging semantic policy into the host.

## Documentation disposition and later scope

The RFC verification ledger, diff/lifecycle fixture catalog, build guide, and
issue board now describe the live host seam and its proof limits. BPR-0079
explicitly retains the real installed-Git blob materializer, cache-session
arbitration, epoch/file/options and cache-deletion wiring, Qt lifetime, and
selected-composition evidence. BPR-0075 does not claim that fake-port evidence
selects the application path, and it adds no Qt geometry, scrolling, ribbon,
theme, packaging, credential, GitHub mutation, or hosted-build surface.
Windows target-native execution remains open and does not block this portable
core/application slice.

## Result

BPR-0075 is complete. The core decides exact diff-result acceptance, the host
performs bounded immutable acquisition/computation/caching, and late or
superseded work cannot publish. BPR-0076 remains in backlog; no later issue or
RFC work was started during this closeout.
