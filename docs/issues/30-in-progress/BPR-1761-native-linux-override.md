---
schema: "implement-rfc.card.v1"
id: "BPR-1761"
title: "Verify process-wide mimalloc on Ubuntu x64"
status: "In Progress"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["linux"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Verify process-wide mimalloc on Ubuntu x64

## Scope and acceptance

Implement and observe the Linux override and installed ELF closure (AC05).
Probe C, C++, Qt/loaded-library allocation and cross-module release in the
desktop and selected helpers/CLI on the selected Ubuntu 26.04 x64 WSL2/WSLg
station. Show that Git, `gh` and
user agent children receive no allocator injection, and run allocator-boundary
stress needed by AC06. Distinguish an installed Linux course from a cross
build or link-only check.

## Scheduling amendment — mandate r3, 2026-10-24

The owner authorized independent work without an Intel Mac. Only Linux's
scheduling dependency on full `pinned-macos-integration` acceptance is removed.
Accepted [[BPR-1758]] remains this card's provider prerequisite. The ELF
composition in `cmake/BprMimallocProvider.cmake` is independent of the Mac native
course; RFC AC03 and AC05 are separate obligations. Every authored outcome,
work selector and exit is preserved. Native Intel and signed-installed Mac
acceptance remain pending on [[BPR-1759]], with its candidate retained.
`check-amendment` passed with only `linux-integration` affected; its receipt is
under `build/mimalloc-linux/amendment-check.json`.

Run `mimalloc-20261024`, segment `linux-m3`, mandate `r3`, unchanged
`coordinator-implementer` revision 2. Sole leaf `/root/mimalloc_linux` owns this
milestone on main at `/Users/jaeger/Documents/GitHub/BetterPullReview`.
The prior Windows leaf and commands are quiescent. Main's unrelated MASK and
BPR-1766 allocation edits are preserved. Native work uses its own source/build
root; no station checkout mutation, release, signing or push is authorized.
