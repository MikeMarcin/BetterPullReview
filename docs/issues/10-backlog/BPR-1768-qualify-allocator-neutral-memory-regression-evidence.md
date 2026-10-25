---
schema: "implement-rfc.card.v1"
id: "BPR-1768"
title: "Qualify allocator-neutral process and allocated-heap regression evidence"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin_rfc":"RFC-BPR-0093","origin":"Owner-directed useful-test audit; remaining unqualified memory risk is a separate follow-up, not a current resource-qualification claim","reservation":"docs/issues/_reservations/BPR-1768--BPR-1768--1126a18b58a3.json","related_cards":["BPR-1763","BPR-1764","BPR-1371","BPR-1767"],"runtime":"build/mimalloc-memory-r8/"}
---

# Qualify allocator-neutral process and allocated-heap regression evidence

## Boundary and retained risk

The owner requires mimalloc and useful passing tests. The RFC-BPR-0093 audit
separates direct cache/ownership/source/latency contracts from absolute process
footprint envelopes calibrated from particular baseline-allocator measurements.
The historical limits and actual failures remain evidence. Reporting them as
telemetry is not a memory-use reduction, an accepted leak repair, a renewal of
the v0.3.3 waiver, or proof that arbitrary process memory growth is bounded.
This follow-up owns that remaining qualification risk; it is not a new final
RFC closeout dependency under the owner's bounded mandate.

Native zone/process-heap/mallinfo counters do not cover mimalloc. A pinned
mimalloc statistics probe also underreports a 64 MiB allocation held by another
live thread until that thread collects. Heap-block visiting assumes no
concurrent threads in that heap. None is an already-qualified all-thread live-
heap oracle. Sampled native process footprint/peak includes allocator retention,
OS accounting and toolkit state; it is not equivalent to live allocated bytes.

The first repeated full-composition prototype's calibration failed fairness:
Linux baseline grew less yet failed while mimalloc's larger initial variation
made its allowance larger. That rule was rejected. A subsequent real Home →
Close review → reopen → large-source experiment within one composition observes
last-three post-collection ranges of 6.12 MiB on Mac and 6.79 MiB on Linux with
mimalloc, exceeding its predeclared 4 MiB settled range. No new green trend rule
was invented. Deliberately retained comparison owners are a separate, directly
failing ownership control. Exact producer identities, raw partial/complete
reports, failed harness attempts and decisions are retained in the
[allocator ledger](../../verification/RFC-BPR-0093.md) and the runtime root.

## Investigation and acceptance

- AC1: Adopt the exact current source, native binaries/runtime, allocator,
  toolkit, fixture, process lifetime and workload. Reuse the existing raw
  evidence and distinguish a real long-lived product workflow from restarting
  composition in a process that production would exit. Preserve current
  cache/ownership/source/latency assertions and historical numerical limits.
- AC2: Establish what an observation covers: live owned objects, allocated
  bytes, committed/resident/reusable pages, current process footprint, or native
  peak. Validate any allocated-heap oracle with held/free and cross-thread
  counterexamples; do not infer coverage from zeros, statistics labels or
  quiescent application jobs while other threads remain live.
- AC3: Propose a memory regression signal justified across the selected native
  allocators/platforms, with a representative sustained workload and a failing
  deliberate retained-owner/cache or allocation-leak control independent of
  known owner counts. Preserve all raw current/peak samples, collection costs,
  cold/warm phases and actual false positives. A noise model must be justified
  before using it; do not tune a threshold or baseline until the course passes.
- AC4: Implement and verify a causal product improvement or an explicitly
  approved, useful test amendment, retaining the original failure record and
  practical limits. No purge-delay-zero default, production collection hook or
  process/child global policy is authorized by this card alone. Any proposed
  default needs representative responsiveness/public-path verification and
  affected native/package evidence. Keep the risk unqualified until then.

## Final useful-test boundary

Native final `e75a78a72` / main `e1d222aab` verifies all four resource contract
paths on Mac/Windows/Ubuntu, with actual retained-middle-owner failures and
original plain numerical failures retained separately. These are semantic/
cache/ownership/source/lifecycle checks. They do not substitute a process-
memory regression signal or qualify arbitrary live allocations/retained pages.
The unchanged limits and large MI footprint/peak observations remain in the
[final ledger](../../verification/RFC-BPR-0093.md#corrected-held-b-barrier-and-final-native-evidence).
No trend rule or product setting is adopted, and this card stays Backlog.
The corrected held-B preparation-tail barrier and later bounded Windows close
pair do not establish a memory-use reduction or explain every timing miss.
