---
schema: "implement-rfc.card.v1"
id: "BPR-0045"
title: "Implement RFC-BPR-0003 application host orchestration"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0041","BPR-0042","BPR-0043","BPR-0044"]
completed: "2026-08-01"
---

# Implement RFC-BPR-0003 application host orchestration

Add a dedicated epoch application host rather than expanding the GitHub host
into a cross-feature controller. Feed it a typed accepted-pull-request
revision from the RFC-BPR-0002 application boundary, not a Qt projection.

Own Git/cache/catalog ports, workers, bounded observations, explicit observed
time, staging, serialized completion, cancellation, identity switching,
failure-atomic catalog/ref commit, cache residency, lifecycle, and incremental
presentation mapping. Every operation carries application session, profile,
host, account, repository, pull request, provider snapshot, identity/PR/epoch
generation, request, and expected catalog revision.

Mint a fresh nonzero 256-bit source-observation identity from operating-system
entropy for every accepted observation, including a semantically corresponding
retention rehydration. Retry a catalog-incarnation-wide collision with a newly
minted identity; never derive the identity from provider fields, process-local
counters, catalog ordinals, or durable semantic revision values. BPR-0044
persists the host-issued identity and rejects reuse but does not generate or
prove its entropy.

Enforce the recoverable phase order: validate and durably complete the spool;
persist the BPR-0044 operation record with expected-pack, pre-import inventory,
and preexisting-pack facts; only then call BPR-0042 `index-pack`; independently
verify objects/connectivity; create exact staging refs; commit catalog
visibility and retained refs under the documented linearization policy; and
exact-delete or recover the spool and request residue. No code path may publish
catalog-visible epoch state before every earlier phase agrees.

Manual-executor and fake-port tests must reorder every terminal path and prove
that only an exact core acceptance decision can commit or publish epoch state.

## In-slice contract remediation

The implementation audit found boundary gaps that must close in this card;
they are not grounds for narrowing the host claim:

- expose the catalog's exact next never-reused ordinal rather than deriving it
  from the latest visible epoch;
- distinguish source-observation and operation-identity collisions so entropy
  retry cannot misclassify recovery conflicts;
- let the catalog return one bounded canonical current-viewer review
  association, including retained-anchor availability, without a history scan;
- obtain prior-head lineage through the cache/process boundary and retain that
  exact observation with the prepared operation;
- admit only a source-less accepted-revision handoff, then construct the core
  source identity from the explicit entropy port;
- expose provider/Git/SQLite-free incremental projection updates; and
- add restart-safe exact spool/install/staging/final inspection before claiming
  recovery for a durable phase.

Association-only and semantic no-op paths must use the core-owned preparation
effect decision and perform no ordinal reservation, network, pack import, or
ref mutation. Prune and whole-cache deletion remain explicit lifecycle work;
if the current core intent algebra cannot authorize them in this slice, track
that missing public authorization rather than silently treating adapter-only
methods as a completed product path.

The audit allocated the missing lifecycle authorization to BPR-0055 and the
missing bounded core/host history-page algebra to BPR-0056. Those cards remain
required RFC-BPR-0003 work; BPR-0045 does not claim either product path merely
because the catalog adapter already exposes narrow component methods.

## Result

Implemented a dedicated source-free epoch application host and provider/Git/
SQLite-free incremental projection boundary. The host accepts the exact
RFC-BPR-0002 authority handoff, completes durable recovery before minting a
source identity, obtains each identity from the production operating-system
CSPRNG, observes UTC through an explicit system-clock port, and submits only
typed bounded observations to the epoch core. Core preparation policy now
prevents unchanged and association-only revisions from reserving an ordinal,
fetching, importing, or mutating refs.

Append orchestration journals the completed spool and pre-import inventory
before `index-pack`, verifies installed objects and prior-head lineage, creates
the exact staging pair, records the core-authorized candidate, journals the
point of no cancellation, promotes the exact retained pair, finalizes catalog
visibility, and cleans only matching residue. Exact recovery inspection covers
spool, installed pack/index, commit/tree closure, staging refs, and final refs.
Pre-PONR ambiguity aborts only the exact operation; PONR and later ambiguity
continues noncancelably. Applied-then-error association, promotion, transition,
finalization, and cleanup outcomes are resolved from exact durable
postconditions so catalog visibility always produces a success terminal.

One bounded host-lifetime actor per repository authority owns a FIFO and at
most one cache session. Same-repository PR replacement cleans the old context
under its old session before closing it and opening the complete new PR
request; unrelated actors can run independently. Every completion returns to
the serialized decision lane, exact request/generation/authority checks reject
stale work, projection exceptions are contained, and shutdown quiescence
includes jobs, deferred ingress, pending offers, decision turns, and actor
queues. Reentrant projection feedback is deferred without recursive
publication. Workers never touch core or Qt state.

The implementation audit also hardened catalog collision classification,
durable head-lineage persistence, raw-ref/commit recovery inspection, exact
success cleanup distinct from abort history, and the installed-Git architecture
allowlist. BPR-0055 retains core-authorized prune/cache-delete product paths;
BPR-0056 retains bounded history paging; BPR-0047 retains public composition;
BPR-0048 retains the broader cross-component failure matrix; and BPR-0053
retains all target-native Windows execution.

## Simplification disposition

The required behavior-preserving pass extracted the system entropy/clock API,
reused one recovery-operation identity helper, retained named exact-result
predicates, and corrected the actor/FIFO contract comment. The apparently
duplicated abort and success cleanup guards remain intentionally separate:
they authorize different physical mutations and postconditions. The explicit
phase checks and mandatory-success recovery branches were also retained
because collapsing them would hide durable linearization and ambiguity rules.

## Completion evidence

- A clean macOS arm64 Qt-enabled Debug rebuild compiled and linked all 121
  targets. The clean native matrix passed 60/60 in 237.85 seconds on the
  completed production tree.
- After adding direct projection-exception and reentrant-ingress cases, the
  final `application_epoch_host` contract passed 20 consecutive executions in
  0.13 seconds.
- A clean Qt-free/core-only rebuild compiled all 53 steps and passed 27/27
  tests in 33.37 seconds, including its independent 104-case architecture
  mutation run.
- The composed component replay passed 8/8: core epoch policy, application
  catalog/runtime/host contracts, SQLite storage/behavior, installed-Git
  process, and installed-Git cache. The physical cache contract completed in
  237.74 seconds and the process contract in 51.97 seconds.
- The architecture boundary mutation contract passed 104/104. Core,
  application, Git/cache, catalog, runtime/dependency, and documentation
  boundary scans passed; `git diff --check` was clean.
- The production runtime-port contract directly exercised nonzero native macOS
  entropy and bounded system UTC. Guarded Linux and Windows implementations
  compile; MinGW object/link inspection selected `BCryptGenRandom` through
  `Bcrypt`. No Windows executable ran, and BPR-0053 retains every native
  Windows claim.
- Independent correctness and concurrency reviews found no remaining
  production blocker. The final documentation audit identified and closed the
  two projection-test gaps. The required behavior-preserving simplification
  extracted the runtime-port API and shared recovery identity helper while
  preserving explicit durable phase and cleanup branches.
- The documentation validator, documentation structure and issue-graph tests,
  workflow-absence scans, architecture scans, and whitespace checks passed.
  Tracked and on-disk GitHub Actions workflow counts were 0/0.
- A later diagnostic parallel rerun was discarded because unrelated system
  load exceeded 440 and time-bounded Git/native fixtures crossed their wall
  limits. It is not counted as verification. The installed-Git process
  contract subsequently passed directly under that load; the final host tests
  above cover the only source changes made after the clean 60/60 run.

## Documentation disposition and deferred boundaries

Updated the system and Git/epoch architecture, GitHub handoff contract,
security model, build/test guide, fixture catalog, issue board, and
RFC-BPR-0003 verification ledger. The closed BPEC v1 description now records
18 tables and 16 indexes, source/operation collision classification, durable
head lineage, exact physical recovery inspection, source-free handoff,
production entropy/clock ports, repository actors, cache-session switching,
projection containment, and shutdown quiescence.

BPR-0047 still owns public application composition and persistence through the
selected executable. BPR-0048 owns the broad combined fault/reordering and
concurrency-stress matrix. BPR-0055 owns core-authorized prune/cache-delete
maintenance, BPR-0056 owns bounded history paging through core/host/projection,
and BPR-0053 owns target-native Windows execution. No RFC-BPR-0004 diff work,
RFC-BPR-0005 presentation claim, RFC-BPR-0006 theme selection, GitHub mutation,
or GitHub Actions workflow is included in this card.
