---
schema: "implement-rfc.card.v1"
id: "BPR-0804"
title: "A tests-disabled non-Apple build fails on an unused CLI confirmation parameter"
status: "Ready"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["win", "linux"]
metadata: {"found_by":"bpr-win during task-20260917-win-evidence (BPR-0798/BPR-0730 build)","found_at":"eeb16d1310e555d9b36f8e9d7516211d74aa2856","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0804: A tests-disabled non-Apple build fails on an unused CLI confirmation parameter

## Defect

The Windows station's `windows-app-release` build (tests disabled) of
`eeb16d13`, which is also `origin/main`, stops at:

    src/app/src/github_application.cpp(7709): warning C4100: 'aOperation': unreferenced parameter

and `/W4 /WX` (`cmake/Warnings.cmake:3`) makes it an error.

`confirm_cli_publication(aTarget, aActor, aOperation)` reads `aOperation`
only inside the `#if defined(__APPLE__)` confirmation dialog. The trailing
`#if defined(BPR_BUILD_TESTING)` branch casts it to `void`; the `#else`
branch returns `false` without touching it. So every non-Apple graph with
`BUILD_TESTING=OFF` has an unused parameter. The gate dates from
`df458d30` (BPR-0786) and was restructured by `a78eefcf` ([[BPR-0797]]).
The Linux `app-release` graph compiles with `-Wextra -Werror`
(`cmake/Warnings.cmake:11,27`) and is expected to fail the same way; that
has not been observed yet.

This blocks every Windows and Linux packaged-application proof, including
[[BPR-0730]], [[BPR-0731]], [[BPR-0798]] and [[BPR-0799]].

## Scope

- Consume `aOperation` on every graph without changing what any graph
  decides: macOS still asks, a tests-enabled graph still answers `true`,
  and a tests-disabled non-Apple graph still answers `false`.
- Owned path: `src/app/src/github_application.cpp`. No behaviour change, no
  new test registration.

## Acceptance

- `windows-app-release` compiles `github_application.cpp` with `/W4 /WX` on
  the Windows station.
- Linux `app-release` compiles it with `-Werror` on the Linux station.
- The macOS dev build compiles it unchanged, and the confirmation decision
  per graph is identical to `eeb16d13` by source inspection.

## Evidence

Not run at card creation. The Windows station is making the one-line fix
(hoist `static_cast<void>(aOperation);` above the `BPR_BUILD_TESTING`
branch) as its own `windows-lane` commit; the integrator finishes this card
on `main` after merging that commit and collecting the compile results.
