---
schema: "implement-rfc.card.v1"
id: "BPR-1760"
title: "Verify process-wide mimalloc on native Windows x64"
status: "In Progress"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["win"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Verify process-wide mimalloc on native Windows x64

## Scope and acceptance

Implement and observe the Windows override with the actual dynamic CRT and
packaged DLL closure (AC04). Probe C, C++, Qt/loaded-library allocation and
cross-module release in the desktop and selected helpers/CLI on a native
Windows 11 x64 host. Include startup, shutdown and allocator-boundary stress
needed by AC06; record import/runtime identity rather than assuming a linked
import library proves redirection.

## Scheduling amendment — mandate r2, 2026-10-24

The owner authorized continuing independent work without a native Intel Mac.
The Windows milestone no longer waits for full `pinned-macos-integration`
acceptance. Its card still requires accepted [[BPR-1758]], which owns the
pinned provider and executable policy. `cmake/BprMimallocProvider.cmake`
selects the Windows redirect independently; RFC AC03 and AC04 are separate
native courses. No outcome, work selector, exit text or native criterion
changed. Native Intel and signed-installed macOS acceptance remain open on
[[BPR-1759]], including its retained local candidate. Linux scheduling is
unchanged. `check-amendment` passed before publishing this change; its receipt
is retained under `build/mimalloc-windows/amendment-check.json`.

Run `mimalloc-20261024`, segment `windows-m2`, mandate `r2`; recipe
`coordinator-implementer` revision 2. Source/tracker root remains
`/Users/jaeger/Documents/GitHub/BetterPullReview`, branch `main`. Sole leaf
`/root/mimalloc_windows`; prior leaf and its commands are quiescent. No push,
signing or release publication is authorized.
