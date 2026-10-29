---
schema: "implement-rfc.card.v1"
id: "BPR-1800"
title: "Settle admitted thread mutations across an accepted refresh"
status: "Done"
rfcs: ["RFC-BPR-0008", "RFC-BPR-0091", "RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v040-typed-mutation-lifecycle-r1", "milestone_plan": {"id": "thread-resolution-refresh-lifecycle", "path": "docs/rfcs/plans/thread-resolution-refresh-lifecycle/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1c543f372ce5a2338dc9b634cecf661f0fbf3754","evidence":{"kind":"observation","detail":"Controlled real-composition held second refresh and held mutation terminal settles exactly once for GitHub/GHES in both Mac configurations and same-image native Windows both configurations.","refs":["lifecycle-dev-final-tests-r3.log","lifecycle-release-focused-r1.json","qualification-repairs-r1/lifecycle-closure-r1/receipt.json","qualification-repairs-r1/lifecycle-linux-final-r1/lifecycle-functional-r1/receipt.json"]}},{"criterion":"AC2","outcome":"pass","revision":"1c543f372ce5a2338dc9b634cecf661f0fbf3754","evidence":{"kind":"observation","detail":"Expanded core/host exact full key/scope/gesture tests cover success/failure/refusal/cancel/unknown, overlapping reused numeric identities, duplicates, genuine retirement/shutdown, all-operation quiescence. Causal negative control restores original unresolved phase9.","refs":["lifecycle-dev-final-tests-r3.log","lifecycle-release-focused-r1.json","qualification-repairs-r1/lifecycle-retirement-negative-r1/receipt.json"]}},{"criterion":"AC3","outcome":"pass","revision":"1c543f372ce5a2338dc9b634cecf661f0fbf3754","evidence":{"kind":"observation","detail":"Shared PC serialized after original Ubuntu terminal. Native Windows a74 built and actual registered closure at metadata1c: Release6/6 via prior5+new1 with unchanged exe/dll/pdb and source-input bridge; Debug6/6, manifests bothPASS. Original failures retained; no shared five-agent mechanism claimed.","refs":["qualification-repairs-r1/lifecycle-native-r1/receipt.json","qualification-repairs-r1/lifecycle-closure-r1/receipt.json","lifecycle-agent-mechanism-comparison-r1.json"]}},{"criterion":"AC4","outcome":"pass","revision":"1c543f372ce5a2338dc9b634cecf661f0fbf3754","evidence":{"kind":"observation","detail":"Mac Release integration5/5/core-only3/3, LLVM22 sixTUs and clang-format22 touched8C++ files pass; documentation gate and67cases/allocation2 pass. Native Linux focused6+6 pass with originalN12459producerfiles+source/runtime copies unchanged. Windows manifest contracts73 and native manifests bothpass. Intentional slice commits, final doc-only upkeep, verified native PID quiescence and no own Mac background jobs. No broader release/package/trust or separate agent-failure closure claimed.","refs":["lifecycle-release-integration-tests-r1.log","lifecycle-core-only-tests-r1.log","lifecycle-tidy-r1.log","lifecycle-tidy-public-test-r2.log","lifecycle-format-final-r2.log","lifecycle-docs-final-r1.log","lifecycle-documentation-tests-r1.log","lifecycle-allocation-tests-r1.log","qualification-repairs-r1/lifecycle-linux-final-r1/lifecycle-functional-r1/receipt.json","qualification-repairs-r1/lifecycle-linux-final-r1/lifecycle-functional-r1/final-binding.json","lifecycle-native-quiescence-r1.json"]}}]
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
These original-source failures remain unchanged. Exact N source/archive/objects, compact
raw/configuration, selected runtime images and their dynamic dependencies are
retained externally. The separate Linux package graph is hash protected; the
coordinator reconciled that manifest before authorizing only the two warm test
graphs to advance to `1c543f372`. The original N reports remain unchanged. Repaired Linux source `1c543f372`
then passes the six affected functional courses in both Release and Debug
(6/6 each, no skips), after the three native bootstrap checks and source
tripwire pass. Final rehashes and the complete file-set comparison preserve
all 12,459 package files and the retained N source/runtime copies. The separate
N package graph is unsuitable for rebuilding against the advanced working
source; its archived N source is retained for reproduction. No distribution
configure, package build or signing operation ran.

The isolated Windows repair checkout completed source `a74c9a875` builds in
both configurations after the original Ubuntu task became terminal. Normal
MASK registration/readiness and the three bootstrap checks passed. Release
passes the core, host, original threaded/reaction, description and comment
courses (5/6); the new controlled course initially refuses fixture startup
inside containment. Debug functional rows were unreached by that fail-fast run.
Paired same-image Release and Debug controls then pass directly and refuse
contained startup at the exact `fixture application starts` predicate. Only
the measured new registration and its verifier pin change; no Win32 cause is
inferred. Registered closure then passes: the new Release course plus the five retained
passing rows supply 6/6 Release; all six Debug courses pass, with both native
network-isolation manifests passing. Metadata commit `1c543f372` changes only
the measured registration/pin and documentation. The selected executable, DLL
and PDB digests remain unchanged before/after configuration and after tests,
preserving the actual compiled source identity `a74c9a875`. No timing credit
or native package/trust acceptance is inferred.

Raw receipts and streams remain in the external release run under
`lifecycle-*` and `qualification-repairs-r1/`. Original Windows 40-second and
243.197-second failures remain failures. No shared mechanism with the five
agent-completion failures has been demonstrated. A separate test-only bounded
recovery proposal is handed to the coordinator without running it in this
milestone. Final numbering, package/native trust and security obligations
remain outside this issue. No numbered cohort is allocated here.

## Milestone handoff

The coordinator accepted the focused Mac, Windows and Linux functional evidence.
Product commit `344ab410d`, naming `02516413e`, whitespace `a74c9a875` and
measured Windows metadata `1c543f372` are intentional slices. Final issue and
verification-document upkeep changes documentation only; native Linux compiles
source `1c543f372`, and Windows retains the proven `a74c9a875` images under
`1c543f372` test metadata. Corresponding-source and actual numbered native
identity/trust acceptance belong to BPR-1799's next cohort. No cohort was
allocated, no published asset changed, and no attended review is required for
this nonvisual repair. Task-owned compiler/test/collection commands are terminal.
The five agent failures and their unexecuted bounded recovery plan remain a
separate release obligation. The release's existing security and native gaps
remain explicit in the external handoff.
