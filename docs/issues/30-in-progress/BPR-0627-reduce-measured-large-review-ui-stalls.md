---
schema: "implement-rfc.card.v1"
id: "BPR-0627"
title: "Diagnose and reduce measured large-review UI stalls"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0624"]
---

# Diagnose and reduce measured large-review UI stalls

The BPR-0624 composed latest-head course passes source/interaction correctness,
but its 5 ms event-loop timer observed a 387.48 ms maximum interval during
20,000-line selection and 201.09 ms during rapid file selection. These are
measured delays, not an accepted responsiveness budget or a diagnosis of a
particular function. The idle 1,000-event wheel batch took 318.03 ms; it does
not establish scrolling responsiveness while background work runs.

Context: Apple M4 Pro, 48 GiB RAM, 14 logical processors, macOS 26.6.2 arm64,
Debug, Qt 6.10.3 Fusion/offscreen, 1600×1000 at DPR 1. Inputs: stored
`large-review-v1`, 515 files, 1,000 source threads, one current application
epoch. Final source matched after all 32 selections; 31 were issued with
outstanding diff work. A 3,517,512-byte/two-entry diff cache is an observation,
not evidence for process memory or pruning ceilings.

## Bounded acceptance

- Attribute the longest main-thread intervals to concrete work through focused
  local profiling/instrumentation; preserve background/core/Qt ownership.
- Implement the smallest justified correction to the measured blocking work.
  Preserve exact source, semantic identity, final-selection precedence and
  existing incremental scrolling invariants.
- Repeat the same selected composed course after the correction on a quiet Mac
  and compare elapsed phases, maximum/p95 intervals and work-overlap counters.
  Exercise input during active work explicitly; distinguish it from idle input.
- Document a justified local target and any remaining stalls. Do not declare a
  universal threshold from one machine or infer native Windows/Linux evidence.
  If profiling identifies separate work, split it before expanding ownership.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
All work stays local and uses synthetic fixtures. No GitHub Actions, push,
paid CI, credential use or production application launch.
