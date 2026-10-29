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

## Current verification, 2026-10-29 UTC

Mac Debug and Release each pass the expanded core/host tests, the controlled
public regression for github.com and GHES, and the original threaded/reaction
course (4/4 each). Coverage includes full-scope retirement, reused numeric
requests across overlapping hosts, success/failure/refusal/cancellation and
unknown terminals for both conversation operations, duplicates, hard shutdown,
and held description/comment terminal quiescence. Release affected composition
closure passes 5/5, including positive inventory and hostile bypass controls,
description/comment courses and the network-isolation manifest. Core-only
conversation and both architecture checks pass (3/3).

Strict LLVM 22 analysis passes all six changed translation units. Its original
public-test lambda naming failure remains recorded; the naming-only correction
passes a focused rerun. Clang-format 22 passes all eight changed C++ files after
the separate whitespace correction. Documentation structure, 67 documentation
cases, two allocation cases and the secret scan pass. Production source bytes
are unchanged between product commit `344ab410d`, naming commit `02516413e`
and whitespace commit `a74c9a875`; compiled images retain their actual source
and build identities independently.

A causal negative control compiles a copied host translation unit changing only
`retire_conversation_changes` from drain to hard close, linked ahead of the
candidate library. The public course then fails at the original phase-9
predicate: one resolution, zero completions and pending state after refresh.
Its expected-failure driver passes; this is discrimination evidence.

The original Ubuntu N qualification is terminal and its complete raw archive
is collected and hash verified: Release 848 passed/10 failed; Debug 836
passed/13 failed/1 skipped. The optimized-only Debug cache-byte skip and eight
separate Debug timing omissions receive no credit. Threaded reply, description
and comment courses fail at the same phase-9 resolution before edit acceptance.
These original-source failures remain unchanged; repaired Linux closure is
pending a retained-source/producer inventory and coordinator reconciliation.

The isolated Windows repair checkout completed source `a74c9a875` builds in
both configurations after the original Ubuntu task became terminal. Normal
MASK registration/readiness and the three bootstrap checks passed. Release
passes the core, host, original threaded/reaction, description and comment
courses (5/6); the new controlled course initially refuses fixture startup
inside containment. Debug functional rows were unreached by that fail-fast run.
Paired same-image Release and Debug controls then pass directly and refuse
contained startup at the exact `fixture application starts` predicate. Only
the measured new registration and its verifier pin change; no Win32 cause is
inferred. Registered new Release and complete Debug closure remain pending.

Raw receipts and streams remain in the external release run under
`lifecycle-*` and `qualification-repairs-r1/`. Original Windows 40-second and
243.197-second failures remain failures. No shared mechanism with the five
agent-completion failures has been demonstrated. A separate test-only bounded
recovery proposal is handed to the coordinator without running it in this
milestone. Final numbering, package/native trust and security obligations
remain outside this issue. No numbered cohort is allocated here.
