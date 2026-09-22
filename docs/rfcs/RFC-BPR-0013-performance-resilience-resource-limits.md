---
id: "RFC-BPR-0013"
title: "Performance, Resource Limits, Resilience, and Recovery"
status: "Proposed"
area: "BPR"
milestone: "M6 Production Readiness"
depends_on: ["RFC-BPR-0004","RFC-BPR-0005","RFC-BPR-0007","RFC-BPR-0008","RFC-BPR-0010","RFC-BPR-0012"]
pack_version: 2
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# RFC-BPR-0013: Performance, Resource Limits, Resilience, and Recovery

## Summary

Harden the full application for large reviews, cancellation, bounded memory, API limits, hostile input, restart recovery, parser fuzzing, and responsive cross-platform operation.


## Motivation

The complete feature set must remain responsive and safe on very large pull requests, unreliable networks, hostile content, repeated refreshes, process cancellation, and application restarts.

## Goals

- Define and enforce latency, memory, allocation, file-size, line-length, API, and cache budgets.
- Make Git, network, diff, syntax, persistence, queue, and AI jobs cancellable and stale-result-safe.
- Add incremental loading, cache eviction, backpressure, and rate-limit handling.
- Add crash/restart, offline, schema migration, and partial-failure recovery.
- Fuzz every custom parser and mapping boundary.
- Establish repeatable large-review benchmarks.

## Non-goals

- Premature distributed processing or a background hosted service.
- Hiding slow or unsupported content behind infinite spinners.
- Running repository code for indexing or performance analysis.

## Architectural allocation

This RFC hardens the architecture rather than adding a parallel performance subsystem.

- **Core:** uses compact interaction state, deterministic in-place transitions where appropriate, retained output storage, explicit request identities/generations, semantic cancellation/supersession, and changed-domain deltas.
- **Host:** owns worker scheduling, backpressure, caches, arenas/pools, I/O retries, resource budgets, recovery, persistence, and the serialized completion lane. It provides a deterministic manual executor for tests.
- **Qt adapter:** coalesces presentationally redundant input, paints only visible rows, applies incremental model/projection updates, and remains idle when nothing changes. Workers never touch Qt.
- Routine operations must scale with the active set plus the changed set. Structural gates include no whole-PR scan for ordinary selection/receipt changes, no model reset for a bounded update, no blocking I/O on the GUI thread, and no untagged async completion.

## Performance budgets

The live implementation must establish platform-specific budgets for launch, initial PR metadata, first file render, file/change navigation, drawer interaction, theme switch, large-file fallback, memory ceiling, and cancellation. The RFC does not hard-code numbers before measurement; it requires checked-in benchmark thresholds justified by target hardware.

## Resource policy

- Load metadata and visible file content first.
- Prefetch only bounded likely-next content.
- Cache immutable Git/diff/syntax values with explicit budgets and eviction.
- Fall back to plain text or truncated display for pathological content while preserving access to raw file metadata.
- Cancel superseded requests and discard results whose identity no longer matches the current selection.

RFC-BPR-0004 supplies dedicated scale gates for large-file semantic diff,
measured-axis rebuilds, visible/near-visible ribbon selection, subpixel input,
reverse-drift, stale completion, and lifecycle stress. Routine scrolling must
not recompute semantic diff or scan/paint the entire file; environment and
structural counters accompany latency results.

## Acceptance criteria

Owner amendment r4 on 2026-09-21 defines the terminal scope below. The original
criteria remain immediately afterward, with every unsatisfied part transferred
explicitly in the [current ledger](../verification/RFC-BPR-0013.md). This is a
change to acceptance, not evidence that the transferred requirements passed.

- Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content.
- The recorded macOS large-review paths demonstrate input, scrolling, navigation and cooperative measurement under their stated workloads; current public selection and unavailable-content recovery remain correct.
- Default host cache ceilings and eviction remain enforced, and historical close/prune ownership evidence retains its exact candidate and sampled-memory limits; current memory-budget failures remain explicit follow-up work.
- Primary and secondary rate-limit policy preserves bounded backoff, Retry-After, cancellation, identity and typed queue state through the existing provider, host and Qt contracts.
- Offline and recovery policy retains typed unavailable or stale state and rejects stale-session completions; complete cached-review offline/restart presentation remains explicit follow-up work.
- Recorded process-kill courses recover Git cache, SQLite and exact owned agent workspaces without corruption or duplicate mutations, within their stated platform and candidate boundaries.
- Custom parser and mapping boundaries retain deterministic malformed-input and property regression evidence; complete fuzz targets and corpora remain inventoried as explicit follow-up work.
- The existing composed logging and backpressure courses preserve metadata-only diagnostics under their tested error conditions.
- The recorded core, host and Qt paths demonstrate bounded scaling, serialized completion, incremental updates, cancellation and stale-result rejection without claiming application-wide coverage.

## Original acceptance criteria — retained after r4 amendment

These original wider requirements remain product follow-up obligations. They no
longer gate this RFC's terminal status under the owner's explicit r4 direction.

- Checked-in benchmark fixtures cover large file counts, large files, many comments, many epochs, and mixed binary/generated content.
- UI input, scrolling, and navigation remain responsive while background work runs.
- Memory and cache usage remain within documented budgets and recover after closing/pruning reviews.
- Primary and secondary rate-limit simulations produce bounded backoff and useful UI state.
- Offline mode uses clearly labeled cached data and never implies freshness.
- Process-kill tests recover Git cache and SQLite state without corruption or duplicate mutations.
- Fuzzers cover diff parsing/mapping, Markdown, settings/themes, API normalization, Git output parsing, and AI structured output.
- Logging and diagnostics remain metadata-only under large/error conditions.
- Architecture benchmarks and instrumentation prove active-set/changed-set scaling, serialized decision application, incremental Qt updates, deterministic cancellation, and stale-result rejection.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Large review remains responsive | Real app benchmark | Navigate/scroll/cancel under load and record frame/input latency |
| Stale results cannot apply | Job identity integration | Rapidly switch host/PR/file/epoch while jobs complete out of order |
| Recovery is correct | Kill/restart harness | Terminate during refresh, DB write, Git fetch, and AI session |
| Limits degrade gracefully | Pathological fixtures | Oversized line/file/comment/theme inputs produce explicit fallback |
| Architecture scales incrementally | Structural benchmark suite | No routine full scan/reset/blocking I/O; jobs are tagged and completions serialized |

## Documentation impact

### Owner amendment r4 — 2026-09-21

The owner requires RFC0013 to close for the same RFC0014-unblocking reason as
RFC0008–0012. The [bounded assignment](plans/rfc-bpr-0013-hardening/mandate-owner-closeout-r4.md)
adopts the existing plan and changes only its final verification milestone.
The six completed milestones, original source plan and historical observations
remain intact. All canonical predecessors are now Auto-Verified.

The broad performance-budget paragraph above is a follow-up requirement beyond
the recorded hardware/course evidence. BPR-0967 owns remaining platform/input/
memory measurements and the current Mac loaded-close/resources failures at the
unchanged 160 MiB sampled-growth limit. BPR-0968 owns full rendered primary and
secondary backoff; BPR-0969 owns cached-review offline/restart freshness;
BPR-0970 owns complete parser/mapping fuzz targets and corpora; BPR-0971 owns
the pathological logging/privacy inventory; BPR-0972 owns wider structural
and active-work evidence. These are standalone Backlog work, not passes.

BPR-0817 retains the Linux pre-existing-parent visibility/exit decision and its
RFC0033 association. BPR-0825 retains the Windows shared-descriptor decision.
BPR-0940 retains composed GitHub/window-layout failures; BPR-0944 retains
comparison-basis and partial-pane work; BPR-0945 retains strict-tidy diagnostics.
Only their RFC0013 association is removed. Their status, evidence and pending
decisions remain unchanged. None requires another owner loading test here.

Current exact-candidate checks and prior candidate-specific results are recorded
separately. No current all-platform performance, fuzz, full-suite, native-package,
human or release acceptance is implied by Auto-Verified. The broader original
plan instruction to finish every card is superseded only for these explicitly
transferred obligations. RFC0014 retains its own acceptance requirements.

### Owner closeout amendment — 2026-09-20

The owner accepts the completed selected-diff milestone with the unusual
UnrealEngine#1773 comparison-basis and partial-pane anomaly explicitly deferred
to BPR-0944. That card includes preserving the available right-side file and
placing the missing-earlier explanation in the left pane. The current both-pane
refusal is not accepted as a fixed anomaly. BPR-0794/0795 are Done under this
amendment; no new target read, owner retest or attended candidate is required.

That narrower amendment left RFC0013 Proposed because its predecessors and
wider acceptance were then open. The original full-fuzz, broader native-budget
and remaining composed gaps were not proved. The later r4 amendment above
explicitly supersedes that stopping boundary and transfers the unproved work;
it does not alter the original observations.

### Retained implementation and review history

The selected-diff milestone retains the historical v005 rejection and the
[independent sparse-import repair](../verification/rfc-bpr-0013-sparse-special-blobs-20260920.md).
The owner subsequently observed a rendered real-target diff in repaired r21;
no repeat owner loading test is requested. Mandate r20 adds dark intraline
legibility, persisted tab metrics, CLI connection measurements, progressive
Overview and semantic drawer colors. The
[usable-review evidence](../verification/rfc-bpr-0013-usable-review-20260920.md)
binds the four product commits, failing-before regressions, live read-only
latency measurements, safe rasters and signed tests-disabled r22 native
source/anchor proof through the safe identical composition. Metadata now paints
before full pagination; authoritative snapshots still wait for complete validation.
Private-target source/anchor cross-check remains distinct from owner-observed
loading. The [continuation checkpoint](plans/rfc-bpr-0013-hardening/checkpoint-rfc0013-selected-diff-d5-r20.json)
binds the expanded mandate and recipe revision 2. BPR-0934 folder collapse stays separate.
These bounded results do not close the RFC or its remaining prerequisites.

Owner follow-ups in signed r22 add mandate `r21-drawer-and-exact-material`.
The [r23 selected-material evidence](../verification/rfc-bpr-0013-selected-material-20260920.md)
closes BPR-0941..0943: neutral file names with independently colored details,
non-actionable folder gestures, and explicit missing modified-file material.
Independent live read-only diagnosis establishes provider merge-base metadata
versus an exact selected base tip without the path. The accepted epoch contract
is unchanged; BPR-0944 owns that separate product decision. Signed r23 proves
the typed refusal and intact-file recovery through the safe production sparse
CLI course. Original exact-target readable-source acceptance remains open for
the diagnosed refusal; no owner retest or new attended candidate is requested.
The [current checkpoint](plans/rfc-bpr-0013-hardening/checkpoint-rfc0013-selected-diff-d6-r21.json)
and [handoff](plans/rfc-bpr-0013-hardening/handoff-rfc0013-selected-diff-d6-r21.json)
retain this mandate, command quiescence and all unfinished obligations.

Budgets, benchmark hardware, cancellation, cache policy, offline states, rate limits, recovery, fuzzing, logging, and known scale boundaries are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
