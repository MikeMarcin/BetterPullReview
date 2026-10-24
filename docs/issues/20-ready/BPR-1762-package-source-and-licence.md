---
schema: "implement-rfc.card.v1"
id: "BPR-1762"
title: "Reconcile mimalloc runtime and corresponding source in packages"
status: "Ready"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1760", "BPR-1761"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Reconcile mimalloc runtime and corresponding source in packages

## Scope and acceptance

Integrate the actual selected mimalloc runtime file graph with BPR's macOS,
Windows and Ubuntu package closure and standalone CLI install (AC07). Update
the exact dependency inventory, installed MIT notice, SPDX and separate
versioned corresponding-source asset. Preserve native signing/hardening,
load-path, source digest and release refusal checks; prove removal and reject
missing or extra runtime files. This is package qualification, not release
publication or a security-owner vulnerability disposition.

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
