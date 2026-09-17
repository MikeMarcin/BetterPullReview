---
schema: "implement-rfc.card.v1"
id: "BPR-0804"
title: "A tests-disabled non-Apple build fails on an unused CLI confirmation parameter"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["win", "linux"]
metadata: {"found_by":"bpr-win during task-20260917-win-evidence (BPR-0798/BPR-0730 build)","found_at":"eeb16d1310e555d9b36f8e9d7516211d74aa2856","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"windows-app-release compiles github_application.cpp with /W4 /WX on the Windows station","outcome":"pass","revision":"f2b8e682","evidence":{"kind":"observation","detail":"bpr-win cmake --build --preset windows-app-release 21/21, exe linked"}},{"criterion":"Linux app-release compiles it with -Werror on the Linux station","outcome":"pass","revision":"17f495fe","evidence":{"kind":"observation","detail":"bpr-linux GCC 15.2: github_application.cpp compiles with no diagnostics; unrelated TUs fail stock -Werror first (BPR-0816)"}},{"criterion":"macOS dev build compiles it unchanged and per-graph confirmation decision is identical","outcome":"pass","revision":"a5667197","evidence":{"kind":"command","detail":"build/r2-dev composition builds at c0f2fa31 and a5667197; diff only moves the void discard ahead of the testing branch"}}]
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

## Evidence, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

bpr-win made the fix as station commit `f2b8e682`, which moves
`static_cast<void>(aOperation);` ahead of the `BPR_BUILD_TESTING` branch with a
comment. It merged on main as `bd7096fd`. Every graph decides exactly as it did
at `eeb16d13`: macOS asks, a tests-enabled graph answers `true`, and a
tests-disabled non-Apple graph answers `false`.

| Criterion | Check | Result | Revision |
|---|---|---|---|
| `windows-app-release` compiles it under `/W4 /WX` | bpr-win `cmake --build --preset windows-app-release`, 21/21, `better_pull_review.exe` linked | pass | `f2b8e682` |
| Linux `app-release` compiles it under `-Werror` | bpr-linux, GCC 15.2: `github_application.cpp` compiles with no diagnostics (file unchanged since `17f495fe`). Other translation units fail stock `-Werror` first, which is [[BPR-0816]]; they were demoted only to reach this file ([BPR-0622 Linux evidence](../../verification/BPR-0622-linux-native-evidence.md)). | pass | `17f495fe` |
| macOS dev compiles it unchanged; per-graph decision identical | `build/r2-dev` builds the composition at `c0f2fa31` and `a5667197`; by inspection only the discard moved | pass | `a5667197` |
