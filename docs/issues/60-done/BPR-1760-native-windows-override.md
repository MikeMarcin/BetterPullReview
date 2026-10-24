---
schema: "implement-rfc.card.v1"
id: "BPR-1760"
title: "Verify process-wide mimalloc on native Windows x64"
status: "Done"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["win"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC04","outcome":"pass","revision":"0ccf31bc4a668a9cc71be93afb6664a0bc70225b","evidence":{"kind":"observation","detail":"Native Windows 11 MSVC dynamic-CRT probes in all four selected images, actual installed/assessment CDB C/C++/Qt ownership and free return, Runtime audit and controls; standalone CLI assessment c5693fd05e9a673381cae5aa76c355db33e39182. Exact scope and identities in docs/verification/RFC-BPR-0093.md."}},{"criterion":"AC06-Windows","outcome":"pass","revision":"0ccf31bc4a668a9cc71be93afb6664a0bc70225b","evidence":{"kind":"observation","detail":"4096 cross-module iterations, cross-thread release, allocation failure controls, shipped installed self-test and Debug/installed CLI/MCP public courses. Global AC06 and final package/security/performance acceptance remain open."}}]
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

## Native Windows acceptance

Mandate r2 / `windows-m2-native`: native Windows allocator behavior is accepted.
The [verification ledger](../../verification/RFC-BPR-0093.md#windows-milestone-result--mandate-r2)
binds the commands, exact candidates and digests. Runtime product candidate is
`0ccf31bc4a668a9cc71be93afb6664a0bc70225b`; tests-disabled CLI candidate is
`c5693fd05e9a673381cae5aa76c355db33e39182`.

All four Debug product probes pass with actual dynamic CRT interception,
Qt/loaded-module ownership, cross-module/thread release, failure behavior and
negative controls. External CDB observes C/C++ ownership in all four
tests-disabled installed/assessment images, Qt allocation/free returning to
Qt, and disabled-redirection refusal. The runtime's shipped self-test, the
CLI's real Review Cycles course and native MCP process course pass. The full
Runtime install audit and missing/extra DLL controls pass; every deployed Debug
runtime equals its producer, including the no-relink refresh regression.

This accepts AC04 and the Windows portion of AC06. It does not accept final
AC07, standalone CLI release inventory, MSI installation/trust, broader native
qualification, AC08, or the remaining Mac/Ubuntu evidence. The CLI assessment
uses exact Qt/CRT files from the audited runtime; final CLI packaging remains
BPR-1762's obligation. The retained global isolation-manifest failures name
existing tooling/three unmeasured rows; no passing full-suite claim is made.

All native task-image processes have exited. No owner desktop was driven or
captured, no review server exists, and no signing, push or publication ran.
