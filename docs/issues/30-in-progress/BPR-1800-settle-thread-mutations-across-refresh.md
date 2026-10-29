---
schema: "implement-rfc.card.v1"
id: "BPR-1800"
title: "Settle admitted thread mutations across an accepted refresh"
status: "In Progress"
rfcs: ["RFC-BPR-0008", "RFC-BPR-0091", "RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v040-typed-mutation-lifecycle-r1", "milestone_plan": {"id": "thread-resolution-refresh-lifecycle", "path": "docs/rfcs/plans/thread-resolution-refresh-lifecycle/milestone-plan.json"}}
---

# Settle admitted thread mutations across an accepted refresh

The controlled public composition admits Resolve while a second Refresh read is
held. Accepting that read retires the mutation host; releasing the successful
resolution terminal then leaves the card pending indefinitely. This is a product
interaction, not a Debug timing exception. Original Windows 40-second and
243.197-second failures remain failures. No real GitHub write was used.

## Acceptance

- AC1: Establish the typed ownership and lifecycle contract, with a controlled
  public regression that holds the read and terminal independently. The admitted
  mutation settles across accepted same-context refresh in Debug and Release.
- AC2: Preserve exact request, authority, account, document and head binding.
  Success, failure, refusal, cancellation/unknown, stale/duplicate terminals,
  genuine context retirement and shutdown are safe, with no duplicate write.
- AC3: Run the affected native Windows functional course on repaired source
  after the shared Windows/WSL machine is quiet. Preserve original failure
  artifacts and keep the five other agent-course failures separate unless an
  actual common mechanism is demonstrated.
- AC4: Run focused affected integration, format and documentation checks;
  intentionally commit the repair and upkeep, record exact evidence and pending
  release obligations, and return at this milestone boundary.

## Authority and evidence

The release owner authorized implement-rfc repairs for actual release failures.
The coordinator assigned this sole non-delegating milestone under mandate
`release-v040-typed-mutation-lifecycle-r1`. Its initial source is
`b1dcb8ba2482ffed96712764ee05efe830431e50`; allocation-only commit
`b29afb66c` was consumed as `7eb7e05ad`. External evidence is under
`/Users/jaeger/.codex/release-runs/v0.4.0-20261028/qualification-repairs-r1/`:
`threaded-refresh-controlled-r1`, `windows-threaded-reply-native-r1` and
`windows-agent-completion-native-r1`. No numbered cohort is allocated here.

## Engineering checkpoint and pinned-source review

The core owns stable conversation scope, full request/session/generation keys,
and exact-once settlement. An accepted same-context refresh retires admission
while the old host drains admitted conversation effects and terminals. A
changed complete target/actor binding or core authority/source scope retires
pending ownership and hard-stops old hosts; shutdown overrides any drain.
Quiescence now includes resolution, reaction, description and comment-edit
records, and held conversation refusals. No Qt surface or provider write
catalog changes; no new library or general infrastructure was introduced.

Self-review of the entire pinned delta from matching `b1dcb8ba2` through this
issue's repair commit found only `src/app/src/github_application.cpp` changed
inside the existing production manifest; no intervening pinned-source commits
exist. Reviewed unchanged per-gesture grant issuance, exact target/actor/source
binding, hard retirement, late/duplicate correlation and stale fixture retry
rejection. The production constructor/grant branch digest remains
`16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1`.
Full-source and manifest pins are resealed with this recorded review, followed
by the actual positive inventory and hostile bypass controls. This is
self-review, not independent or native acceptance.

Initial evidence: Debug core/host tests pass; the controlled public course
passes both recorded hosts in 6.15 seconds; the original threaded/reaction
course passes in 8.29 seconds. The first controlled driver read a cached
phase-8 diagnostic without advancing the probe to phase 9; that failed run
remains recorded separately, and the corrected driver observes the same held
read and terminal conditions before accepting success. Release and Windows
verification are not yet supplied.

## Current verification, 2026-10-29 UTC

- Mac Debug: the expanded core/host tests, controlled public regression and
  original threaded/reaction course pass (4/4, final r3). This includes
  overlapping hosts reusing a numeric request, every scope-field retirement,
  duplicate success/failure/refusal/cancellation/unconfirmed terminals for
  both conversation operations, and held edit-terminal quiescence.
- Mac Release: those four courses pass (4/4). The affected composition closure
  also passes (5/5): source/seal hostile controls, round-trip composition,
  description editing, comment editing and the network-isolation manifest.
- Core-only: configure and focused build pass; conversation policy and both
  architecture checks pass (3/3). Clang-format 22 checks all eight touched C++
  files. Documentation structure, 67 documentation cases and two allocation
  cases pass. Strict LLVM 22 analysis is still pending.
- A causal negative control compiles a copied host translation unit that
  changes only `retire_conversation_changes` from drain to hard close and
  links it ahead of the exact candidate application library. The controlled
  course fails at the original predicate: one resolution, zero completions
  and pending state after refresh. Its expected-failure driver passes; this
  is discrimination evidence, not a successful product course.

Raw evidence and receipts remain in the external release run under
`lifecycle-*` and `qualification-repairs-r1/lifecycle-retirement-negative-r1/`.
All Mac functional commands are terminal. The original Ubuntu N qualification
is still running (PID 2883647, source `144de719a`); no Windows build/replay has
started while it is active. Required repaired-source Windows acceptance and
final release/numbering/security obligations remain pending. No shared cause
with the five other Windows agent-course failures has been established.

Strict LLVM 22 analysis completed the production and core/host test translation
units successfully. Its only failure was the new public test's local lambda
name `release_read`; the naming-only correction to `releaseRead` is committed
separately and receives a focused static/build/public recheck. The earlier
static failure remains in `lifecycle-tidy-r1.log`. No product behavior changes.

The original Ubuntu N qualification is now terminal and its raw archive is
collected and hash verified: Release 848 passed/10 failed; Debug 836 passed/13
failed/1 skipped. The Debug cache-byte course reports optimized-only skip;
the eight separate Debug timing omissions receive no credit. Threaded reply,
description editing and comment editing all stopped at the same unresolved
phase-9 resolution before edit acceptance. This is original-source failure
evidence, not repaired-source Linux acceptance. The isolated Windows repair
checkout is prepared from `344ab410d`; MASK preview refused its missing local
locked content, so the approved AgentSkills tap is fetched before previewing
again. No native repair build has started yet.
