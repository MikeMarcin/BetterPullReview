---
id: "RFC-BPR-0013"
title: "Performance, Resource Limits, Resilience, and Recovery"
status: "Proposed"
area: "BPR"
milestone: "M6 Production Readiness"
depends_on: ["RFC-BPR-0004","RFC-BPR-0005","RFC-BPR-0007","RFC-BPR-0008","RFC-BPR-0010","RFC-BPR-0012"]
pack_version: 2
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

Budgets, benchmark hardware, cancellation, cache policy, offline states, rate limits, recovery, fuzzing, logging, and known scale boundaries are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
