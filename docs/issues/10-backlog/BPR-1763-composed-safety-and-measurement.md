---
schema: "implement-rfc.card.v1"
id: "BPR-1763"
title: "Measure composed allocator behavior and preserve tool paths"
status: "Backlog"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1760", "BPR-1761"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Measure composed allocator behavior and preserve tool paths

## Scope and acceptance

Run representative product workflows and allocator-boundary stress after
native interception is known (AC06). Verify sanitizer/fuzzer selection and
ordinary allocation-failure behavior. Compare matched Release workloads for
launch, large-review navigation and memory/close/prune on the same hardware
with the developer baseline switch (AC08). Re-run unchanged existing memory
budgets and keep BPR-0967's pre-existing failures distinct. Record conditions,
candidate identities, raw outcomes and limitations; no speedup is presumed.

## Scheduling amendment — mandate r4, 2026-10-24

The owner's instruction to keep working without an Intel Mac authorizes the
independent package and measurement work. Accepted [[BPR-1758]], [[BPR-1760]]
and [[BPR-1761]] remain prerequisites; full Mac acceptance no longer blocks
starting this card. The live distribution milestone already depended only
on Windows/Linux. The checked plan amendment makes the pending
`pinned-macos-integration` dependency explicit at final closeout. Every AC07
and AC08 requirement, authored outcome, work selector and exit remains intact;
this card cannot be Done while its own required evidence is incomplete.

Source basis: the independent native Windows/Linux installed courses in the
[ledger](../../verification/RFC-BPR-0093.md), RFC AC03 versus AC06–AC08, and
the separate package/instrumentation/developer-baseline paths in
`packaging/`, `cmake/BprMimallocProvider.cmake` and `CMakePresets.json`.
Native Intel/signed-installed Mac AC03 remains on [[BPR-1759]] and final
[[BPR-1764]] still depends on it. No acceptance or release approval is waived.
The check receipt is under `build/mimalloc-distribution/amendment-check.json`.
