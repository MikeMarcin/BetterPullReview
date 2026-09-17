---
schema: "implement-rfc.card.v1"
id: "BPR-0799"
title: "Linux: observe the native empty-desktop quit"
status: "Done"
rfcs: ["RFC-BPR-0033"]
depends_on: []
platforms: ["linux"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"RFC-BPR-0033 B1, carried as not run since the profile-chooser milestone","acceptance":["B1"]}
acceptance: [{"criterion":"On target-native Linux, closing the last window of the last open profile reaches quiescence through the existing drain and Quit path with exit code 0, and presents no chooser on the way out","outcome":"pass","revision":"1c6eaf21","evidence":{"kind":"observation","detail":"On WSLg (Wayland on Weston), the tests-disabled app-release image sha256 9b9ef069…f4a ran with a fresh XDG data root. Under umask 0002, caption closes (WM_SYSCOMMAND/SC_CLOSE to the RAIL window, no kill) exited 0 three times, 40-47 ms after the close, and File > Quit exited 0. Under umask 0022, two caption closes exited 0. Each run logged op=application outcome=shutdown. A RAIL window watch saw no Profiles or other BetterPullReview window after any close. The pre-fix image under umask 0002 exits 1. Logs: task-20260917-lnx-build/native/course-candidate2-umask002.json, course-candidate2-umask022.json and course-0817-prefix-umask002.json."}},{"criterion":"The restore record distinguishes that close from an explicit Quit: the next launch opens a window whose identity differs from the explicitly closed one","outcome":"pass","revision":"1c6eaf21","evidence":{"kind":"observation","detail":"Same umask 0002 native course, read from profile-registry.sqlite3 after each exit. Caption closes dropped the row, and the next launch opened a new identity: window 1, then window 2. File > Quit kept window 3 at generation 1, and the next launch restored window 3 at generation 2."}},{"criterion":"native_empty_desktop_policy() is observed answering quit on the target, rather than assumed from the #if","outcome":"pass","revision":"1c6eaf21","evidence":{"kind":"command","detail":"ctest --preset dev -R application_profile_management_public_path on the stock GCC 15.2 dev tree passed in 2.88 s with profile-management-public-path:pass (task-20260917-lnx-build/focused-0817.ctest.log). It ran under a root-owned 1777 /tmp with TMPDIR unset. Line 269 asserts app::native_empty_desktop_policy()() == quit on non-Apple platforms. Only that native assertion is cited; the course's injected-policy close section is not."}}]
---

# BPR-0799: Linux: observe the native empty-desktop quit

## Outcome

RFC-BPR-0033 B1 says that on Linux, closing the last window of the last open
profile quits through the existing drain and Quit path, and that the restore
records still tell that close from a Quit as RFC-BPR-0024 requires. On this
pack's macOS lane only the **injected** policy outcome was ever proven:
`native_empty_desktop_policy()` answers `stay_resident` on macOS, so the quit
branch is never taken there.

Every closeout row in this pack has carried that as **not run, never as
passing**, and said in as many words that no card carrying `platforms` metadata
existed to hold it. This card is that home. It was allocated on main in d8 so
the Linux lane has somewhere to record the observation.

## Acceptance

- On target-native Linux, closing the last window of the last open profile
  reaches quiescence through the existing drain and Quit path with exit code 0,
  and presents no chooser on the way out.
- The restore record distinguishes that close from an explicit Quit: the next
  launch opens a window whose identity differs from the explicitly closed one.
- `native_empty_desktop_policy()` is observed answering `quit` on the target,
  rather than assumed from the `#if`.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run**: no
Linux observation exists for this behaviour, and none is to be inferred from
the injected policy course.

## Boundaries

No GitHub or GHES mutation, credential access or provider change is authorized
by this card. macOS residency is [[BPR-0796]]; Windows is [[BPR-0798]].

## Linux station evidence, 2026-09-17

Station `bpr-linux` recorded this evidence during task
`task-20260917-lnx-evidence` (`dsp-001-r2`), in
[BPR-0799's Linux native evidence](../../verification/BPR-0799-linux-native-empty-desktop-quit.md).
The builds are product source `9bbe3604`, GCC 15.2; the tests-disabled image's
SHA-256 is `da30fc69…f058`.

- **Native policy (criterion 3): observed.**
  `native_empty_desktop_policy()() == quit` holds on the target. It is asserted
  inside `application_profile_management_public_path` on `dev`, whose close and
  relaunch course uses an injected quit policy.
  - That test **fails** under the stock `/tmp` with `FAILED: and an orderly exit
    code`.
  - It passes with a private `TMPDIR`.
- **Close from Quit (criterion 2): observed natively.** The tests-disabled
  image ran on the WSLg display, where Qt chose Wayland, and was closed through
  the caption close.
  - A caption close drops the window row, and the next launch opens a new
    identity: 1, then 2.
  - An explicit Quit keeps window 3, and the next launch restores 3 at a new
    generation.
- **Quit with exit code 0 and no chooser (criterion 1): passes at umask
  `0022`.** Exit 0 came 43–45 ms after the close, with the `op=application
  outcome=shutdown` diagnostic and no chooser window seen.
  - **It fails at umask `0002`.** The data root's parent is created 775, and
    closing the last window then exits **1**.
  - **Cause.** The agent workspace neighbourhood beside the data root is refused
    for a group-writable parent (`prepare_workspace_neighbourhood`). Startup
    recovery then marks cleanup failed.
  - **Negative control.** A parent pre-created 755 under the same umask exits 0.
  - The same refusal is behind the automated failure under `/tmp`.

Not Done. Criterion 1 fails under umask `0002`. Ubuntu's `USERGROUPS_ENAB yes`
plausibly gives desktop logins that umask; no real desktop session was
observed. The defect is reported to the integrator and needs a design decision
in the Unix agent workspace path.

The card was not moved on the lane: any move rewrites its link in
`docs/rfcs/README.md`, which this dispatch does not permit.

## Linux evidence and block, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

bpr-linux observed B1 at `17f495fe` on WSL2 Ubuntu with WSLg. The full record is
[the Linux evidence](../../verification/BPR-0799-linux-native-empty-desktop-quit.md)
(station commit `6d649ecd`, merged on main in `a9076de1`).

- **Criterion 3 — pass.** `application_profile_management_public_path` asserts
  `native_empty_desktop_policy()() == quit` on Linux. The course's close and
  relaunch section uses an injected policy, and those assertions are not cited.
- **Criterion 2 — pass.** A caption close gives the next launch a new window
  (1, then 2). An explicit Quit keeps window 3, and the next launch restores it.
- **Criterion 1 — depends on umask, blocked.** Under umask `0022`, the window
  exits 0 about 44 ms after a caption close, logs `op=application outcome=shutdown`,
  and shows no chooser. Under umask `0002`, which is a plausible Ubuntu
  user-private-group desktop default, the application creates its data folder
  `775`, refuses its own agent workspace neighbourhood, and exits **1**
  ([[BPR-0817]]).
- **Close transport.** Qt chose Wayland under Weston, which lacks
  `_NET_CLOSE_WINDOW`, so the close used the WSLg window's caption-close
  message. The process was not killed.

The card stays Blocked until [[BPR-0817]] is fixed. Criterion 1 must then be
rerun under umask `0002`.

## Linux rerun after BPR-0817, 2026-09-17 (task-20260917-lnx-build, dispatch dsp-001-r2)

bpr-linux reran B1 on the stock, tests-disabled `app-release` image of
`1c6eaf21` (SHA-256 `9b9ef069…f4a`). That image includes
[[BPR-0816]]'s GCC fixes and [[BPR-0817]]'s owner-private data root. The run
used the WSLg display, where Qt chose Wayland. The full record is
[the Linux evidence](../../verification/BPR-0799-linux-native-empty-desktop-quit.md),
under "Rerun after BPR-0817".

- **Criterion 1: pass.** The process was never killed.
  - **Umask 0002, fresh data root.** Three caption closes and one File > Quit
    each exited 0; the closes exited 40–47 ms after the close was posted. No
    chooser window appeared, and each run logged `outcome=shutdown`.
  - **Umask 0022.** Two caption closes each exited 0, 39–42 ms after the close.
  - **Folders.** The application created the data root's parent at 0700, and
    the agent workspace neighbourhood exists.
  - **Pre-fix control.** The same umask 0002 course on the pre-fix image exits
    1.
- **Criterion 2: pass.**
  - Caption closes drop the row, and the next launch gets a new identity:
    1, then 2.
  - File > Quit keeps window 3.
  - The next launch restores 3 at generation 2.
- **Criterion 3: pass.** `application_profile_management_public_path` asserts
  `native_empty_desktop_policy()() == quit` on Linux. It now passes under the
  stock `/tmp`: root-owned, 1777, `TMPDIR` unset. The close and relaunch section
  of that course is injected-policy evidence and is not cited.
- **Residual, carried by [[BPR-0817]].** An install whose
  `$XDG_DATA_HOME/BetterPullReview` already existed as 0775 still exits 1 on
  close. The owner has not yet decided whether that should change.
