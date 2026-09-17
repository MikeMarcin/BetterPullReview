---
schema: "implement-rfc.card.v1"
id: "BPR-0798"
title: "Windows: observe the native empty-desktop quit"
status: "Done"
rfcs: ["RFC-BPR-0033"]
depends_on: []
platforms: ["win"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"RFC-BPR-0033 B1, carried as not run since the profile-chooser milestone","acceptance":["B1"]}
acceptance: [{"criterion":"B1 quit through the existing drain and Quit path, exit code 0, no chooser, on target-native Windows","outcome":"pass","revision":"e93d9215045f06063b7cbdcbeadd61a25bda3e76","evidence":{"kind":"observation","detail":"windows-app-release image sha256 46619a531e8ffd51d6cd25b3c3431f7d4e15e43cc728b3986d781bc9919eca51 launched directly on JaegAW's interactive desktop on an empty default %LOCALAPPDATA%\\BetterPullReview\\BetterPullReview root; its only window 'BetterPullReview — Default profile' closed with UI Automation WindowPattern.Close (the caption close, no process kill). Exit code 0, 54 ms after the close; top-level windows polled every 20 ms from close to exit showed none but the PseudoConsoleWindow of redirected stdout, so no chooser was presented. The registry saved the close as the Quit path does (revision 2 to 4, window row removed). Automated half: `ctest --preset windows-dev -R \"^application_profile_management_public_path$\"` passed 13.57 s; its close/relaunch course uses the injected quit policy."}},{"criterion":"Restore record distinguishes the close from an explicit Quit: next launch opens a new window identity","outcome":"pass","revision":"e93d9215045f06063b7cbdcbeadd61a25bda3e76","evidence":{"kind":"observation","detail":"profile_registry_windows/profile_registry_meta read from snapshots: the closed launch held window_id 1 and its row was gone after exit; the relaunch on the same root opened window_id 2 (last_window_id 1 to 2), not the explicitly closed window 1; the relaunch also closed with exit 0 after 59 ms and no chooser."}},{"criterion":"native_empty_desktop_policy() observed answering quit on the target","outcome":"pass","revision":"e93d9215045f06063b7cbdcbeadd61a25bda3e76","evidence":{"kind":"command","detail":"application_profile_management_public_path on windows-dev (Debug, MSVC) passes 13.57 s through ctest; its non-Apple branch expects app::native_empty_desktop_policy()() == app::empty_desktop_outcome::quit on this target, read from the composition's own named policy rather than the injected one."}}]
---

# BPR-0798: Windows: observe the native empty-desktop quit

## Outcome

RFC-BPR-0033 B1 says that on Windows, closing the last window of the last open
profile quits through the existing drain and Quit path, and that the restore
records still tell that close from a Quit as RFC-BPR-0024 requires. On this
pack's macOS lane only the **injected** policy outcome was ever proven:
`native_empty_desktop_policy()` answers `stay_resident` on macOS, so the quit
branch is never taken there.

Every closeout row in this pack has carried that as **not run, never as
passing**, and said in as many words that no card carrying `platforms` metadata
existed to hold it. This card is that home. It was allocated on main in d8 so
the Windows lane has somewhere to record the observation.

## Acceptance

- On target-native Windows, closing the last window of the last open profile
  reaches quiescence through the existing drain and Quit path with exit code 0,
  and presents no chooser on the way out.
- The restore record distinguishes that close from an explicit Quit: the next
  launch opens a window whose identity differs from the explicitly closed one.
- `native_empty_desktop_policy()` is observed answering `quit` on the target,
  rather than assumed from the `#if`.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run**: no
Windows observation exists for this behaviour, and none is to be inferred from
the injected policy course.

### Observed on JaegAW at `e93d9215`

Station `bpr-win` ran task `task-20260917-win-evidence`. The producing revision
is `e93d9215045f06063b7cbdcbeadd61a25bda3e76`, which carries the [[BPR-0806]]
fix. Without that fix the tests-disabled Windows image could not start at all.

**The gesture on the shipped image.**

- The image is `windows-app-release` (tests disabled), SHA-256
  `46619a531e8ffd51d6cd25b3c3431f7d4e15e43cc728b3986d781bc9919eca51`.
- It was launched directly on the interactive desktop on an empty default root,
  `%LOCALAPPDATA%\BetterPullReview\BetterPullReview`. The owner's root was moved
  aside for the run and has since been restored.
- Its only window, `BetterPullReview — Default profile`, was closed with UI
  Automation `WindowPattern.Close`. That is the caption close, not a process
  kill.

| Criterion | Direct surface | Result |
|---|---|---|
| Quit through drain and Quit, exit 0, no chooser | The process exit code. Visible top-level windows of the process, polled every 20 ms from the close to exit. The profile registry file. | **Exit code 0**, 54 ms after the close. No window appeared other than the `PseudoConsoleWindow` of redirected stdout, so no chooser. The registry saved the close as the Quit path does: revision 2 became 4 and the window row was removed. |
| The restore record tells this close from Quit | `profile_registry_windows` and `profile_registry_meta.last_window_id`, read from snapshots | The closed launch held `window_id` 1, and its row was gone after exit. The relaunch on the same root opened **`window_id` 2** (`last_window_id` went from 1 to 2), not the closed window 1. The relaunch also closed with exit 0 after 59 ms, with no chooser. |
| `native_empty_desktop_policy()` answers `quit` on the target | `ctest --preset windows-dev -R "^application_profile_management_public_path$"` (Debug, MSVC) | **Passed** in 13.57 s. The course's non-Apple branch expects `app::native_empty_desktop_policy()() == app::empty_desktop_outcome::quit`, read from the composition's own named policy. Its later close and relaunch course drives an injected quit policy instead, and that injected run is not the evidence for the first two rows. |

**Limitation.** The diagnostics log's shutdown line cannot be recorded on
Windows, because there is no Windows diagnostics file sink ([[BPR-0701]] made no
Windows claim; [[BPR-0603]] defers it). The management course now asserts that
absence on Windows. The chooser check is a 20 ms visible-window poll, not a
window-creation hook. Window identity is read from the durable registry records,
not from anything on screen.

## Boundaries

No GitHub or GHES mutation, credential access or provider change is authorized
by this card. macOS residency is [[BPR-0796]]; Linux is [[BPR-0799]].
