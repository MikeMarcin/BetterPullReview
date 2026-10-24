---
schema: "implement-rfc.card.v1"
id: "BPR-1761"
title: "Verify process-wide mimalloc on Ubuntu x64"
status: "Done"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["linux"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"0c40cca40e5d93205d83fd65f76af029ac937347","evidence":{"kind":"observation","detail":"Ubuntu26.04 WSL2 native four-product probes, actual installed GDB C/C++/Qt/cross-free/OOM and one-runtime origins in Runtime/TourCLI, public CLI/MCP/XCB paths, clean Git/gh/agent child traces, complete Runtime ELF audit and negative controls. Exact identities and limitations: docs/verification/RFC-BPR-0093.md."}},{"criterion":"AC06-Linux","outcome":"pass","revision":"0c40cca40e5d93205d83fd65f76af029ac937347","evidence":{"kind":"observation","detail":"4096 cross-module iterations, cross-thread release, C/nothrow/throwing failure semantics, final 16-row native integration and installed public courses. Full instrumentation, all-platform source/legal/trust, measurement and Mac native criteria remain open."}}]
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

## Native Linux acceptance

Mandate r3 / `linux-m3` accepts AC05 and the Linux part of AC06 at product
candidate `0c40cca40e5d93205d83fd65f76af029ac937347`. The
[verification ledger](../../verification/RFC-BPR-0093.md#linux-milestone-result--mandate-r3)
binds the complete native commands, failed attempts, digests and limits.

All four Debug product probes and the final 16-row integration pass. External
GDB observes actual C/C++, loaded-module and installed Qt ownership,
cross-module frees, failure behavior and one installed allocator in every
Runtime and TourCLI image. Installed CLI/MCP courses and the exact XCB
Runtime's shipped self-test pass. Installed supervisors launch actual Git/gh
and external agent probes with clean environments; an injected negative
control is refused. The complete Runtime audit and omission/extra controls
pass, with original installed bytes unchanged.

Linux required its own real SONAME producer/install entries, C++ allocator
compilation for throwing-new failure semantics, ELF hardening and standalone
CLI loader/xdiff closure. Two unrelated existing GCC build blockers received
mechanical, behavior-preserving repairs so the native candidate could compile.
The ledger records their focused verification and retained initial failures.

This accepts the Linux allocator milestone only. Final all-platform AC07,
standalone CLI legal/source release inventory, DEB/trust/ordinary-user desktop
and vault qualification, AC08 and Mac Intel/signed-installed criteria remain
open. The original station checkout stays clean. The isolated root and native
candidate are retained, all own commands/images are quiescent, and no review
server/session exists. No signing, push or publication ran.
