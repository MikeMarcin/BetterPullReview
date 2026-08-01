---
id: BPR-0052
title: Simplify the RFC-BPR-0003 change set
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0048, BPR-0049, BPR-0050, BPR-0051]
---

# Simplify the RFC-BPR-0003 change set

Run the code-simplifier skill over the completed RFC slice. Remove duplicated
epoch policy, authority drift, broad Git/filesystem/persistence access,
unnecessary serialization, retained whole-history copies, full Qt model
resets, unsafe process/config convenience paths, stale comments, and
avoidable complexity without changing behavior.

Replay core, host, real-Git, smart-transport, catalog/recovery, Qt,
composition, security, sanitizer, documentation, and target-native
available-platform verification after simplification.

## Audit progress

- Removed a redundant direct `BetterPullReview::Application` link from the
  real maintenance integration target. Both adapter dependencies already
  publish that application dependency; the generated link command now names
  the static application library once and the real core/host/Git/SQLite test
  passes 1/1 in 29.10 seconds.
- Simplified stable-row move/update handling by retaining the exact current
  index instead of scanning the model a second time, and removed an identical
  second insertion-index clamp. The scale-specific Qt comparison matrix passes
  3/3 with its persistent-index, exact-signal, and zero-routine-reset checks.
- Kept the explicit core reducers, host recovery phases, installed-Git policy,
  cache/catalog identity checks, platform filesystem branches, and SQLite CAS
  paths intact. Independent mechanical and semantic audits agree that their
  verbosity encodes fail-closed behavior rather than removable duplication.
- The semantic audit found that accepted history pages can accumulate to the
  catalog's 100,000-epoch limit in the Qt selector, producing up to 200,000
  anchor rows and repeated linear scans. This requires a bounded/virtual
  selector design and direct integration evidence, not a behavior-preserving
  micro-refactor. BPR-0052 remains open until that work is tracked and
  resolved.
