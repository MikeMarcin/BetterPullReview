# BPR-0799 Linux native empty-desktop quit evidence

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-2`), mandate r2, task `task-20260917-lnx-build`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`.
- **State.** All three criteria pass natively at `1c6eaf21`, the
  [BPR-0817](BPR-0817-linux-owner-private-data-root.md) fix. The results are
  recorded at the end of this file, under "Rerun after BPR-0817,
  `task-20260917-lnx-build`".
  - Criterion 1 holds under umask 0002 with a fresh data root and under umask
    0022, on the stock tests-disabled image.
  - The card moves from Blocked to Done.
- **Residual, not in B1.** A data root whose parent already existed as 0775
  still exits 1. That is BPR-0817's parked owner question.
- **Earlier task.** The sections after this one were recorded by
  `task-20260917-lnx-evidence` at `9bbe3604`, with demoted diagnostics. They
  stand as recorded.

## Station, revision and builds

- **Station and task.** Station `bpr-linux` (JaegAW, WSL2 Ubuntu 26.04 x86_64,
  kernel `6.18.33.2-microsoft-standard-WSL2`) ran task
  `task-20260917-lnx-evidence`.
- **Revision.** Product source `9bbe3604a19416033e0543523a54d3fabab27b4a`. That
  is dispatched base `17f495fe` plus the one-line Linux compile fix recorded in
  [BPR-0622's Linux evidence](BPR-0622-linux-native-evidence.md).
- **Compiler.** GCC 15.2 with four diagnostic classes demoted from error to
  warning. That file lists them and the GCC failures behind them.

| Build | Preset | Executable | SHA-256 |
|---|---|---|---|
| Tests disabled | `app-release` (Release, `BUILD_TESTING=OFF`) | `build/app-release/src/app/better_pull_review` | `da30fc69b465b1bff053bc29451082e4c28311c1d45ded1b9e3eb81a0e7bf058` |
| Developer | `dev` (Debug, `BUILD_TESTING=ON`) | `build/dev/tests/end_to_end/profile_management_public_path` | `cf92713cec631c186ee08d18aa64f9fabf3c534a2e39f51b4413dae7eb18c499` |

**BPR-0804.** The Linux `app-release` graph compiles
`src/app/src/github_application.cpp` cleanly under GCC 15.2 with `-Werror`,
with no diagnostic of any class. That file is byte-identical at `17f495fe` and
`9bbe3604`.

The limitation: at `17f495fe` with unmodified flags, that translation unit was
never reached. Upstream libraries fail first on the GCC classes listed in
BPR-0622's file, so the clean compile was observed in the evidence build.

## Automated half: `application_profile_management_public_path`

The registered test ran on `dev` with its registered environment
(`QT_QPA_PLATFORM=offscreen`, `QT_STYLE_OVERRIDE=Windows`, inside
`better_pull_review_network_sandbox`):

```sh
ctest --preset dev -j1 -V -R '^application_profile_management_public_path$'
```

| Run | `TMPDIR` | Result |
|---|---|---|
| 2026-09-17T06:21:00Z | unset (`/tmp`: tmpfs, `root`, mode 1777) | **Fail**, 2.26 s: `FAILED: and an orderly exit code` |
| same binary, run directly | unset | **Fail**, same message |
| same binary, run directly | ext4 directory owned by the station user, mode 1777 | **Fail**, same message |
| same binary, run directly | ext4 directory under the station user's home, mode 755 | Pass |
| same binary, run directly | tmpfs directory `/tmp/bpr-0799-diag-*`, mode 700 | Pass |
| ctest, as registered | tmpfs directory `/tmp/bpr-0799-ctest-*`, mode 700 | **Pass**, 2.85 s, `profile-management-public-path:pass` |

**What this course establishes on Linux.**

- **The native policy.** The course's `expect` exits at the first failure.
  Every run reached line 400, so the native assertion at line 264 passed on
  every run: `app::native_empty_desktop_policy()() ==
  app::empty_desktop_outcome::quit`. That is the Linux answer observed on the
  target, not assumed from the `#if`.
- **The close and relaunch course uses an injected policy.** It installs
  `EmptyDesktopPolicy = [] { return app::empty_desktop_outcome::quit; }`, not
  the native policy, and on a fixture composition. So its close-quits, no
  chooser, exit code 0 and new-window-identity assertions are injected-policy
  evidence only. They are not native quit evidence.

**The failure is Linux-specific and environmental. It is not in the
empty-desktop policy.**

- **Where the data root lands.** The course creates its data root as a
  `QTemporaryDir` directly under `TMPDIR`. On a stock Linux host that is `/tmp`,
  which is root-owned and mode 1777.
- **Why startup recovery fails.** The Unix composition places the agent
  workspace neighbourhood beside the data root (`github_application.cpp:4796`),
  so its parent is `/tmp`. Ordinary startup recovery calls
  `recover_installed_agent_workspaces`, which calls
  `prepare_workspace_neighbourhood` (`installed_git_process.cpp:3003`). That
  function refuses a parent not owned by the effective user or writable by
  group or other, and returns `repair_required`.
- **Why the exit code is 1.** The composition records that refusal as
  `AgentWorkspaceCleanupFailed`. At shutdown the profile's `CleanupFailed`
  makes `profile_desktop` set `ExitCode = 1` (`profile_desktop.cpp:853`).
- **Why other platforms pass.** macOS gives each user a private `TMPDIR`.
  bpr-win recorded this test passing at `d587b5f1`.

## Native half: the tests-disabled image on the WSLg display

### Method

- **Launch.** `build/app-release/src/app/better_pull_review` was launched
  directly with `DISPLAY=:0` and `WAYLAND_DISPLAY=wayland-0`, and with
  `QT_QPA_PLATFORM` unset.
  - The process mapped `plugins/platforms/libqwayland.so`, so Qt chose its
    Wayland platform on Weston. It was not offscreen.
  - On Windows the window is an `msrdc` `RAIL_WINDOW` titled
    `BetterPullReview - Default profile - BetterPullReview (Ubuntu)`.
- **Scratch root.** Each course used a new scratch root with `XDG_DATA_HOME`,
  `XDG_CONFIG_HOME`, `XDG_CACHE_HOME` and `XDG_STATE_HOME` inside it, so the data
  root is `xdg_data_home/BetterPullReview/BetterPullReview`.
- **Close gesture.** The graceful close is the window manager's caption close:
  1. The harness waits until the profile registry holds the window row, then
     waits 3 s more.
  2. It posts `WM_SYSCOMMAND`/`SC_CLOSE` to that exact `RAIL_WINDOW` handle
     through `user32!PostMessage`. That is the message the caption close button
     produces.
  3. RDP RAIL hands it to Weston, which delivers the toplevel close to Qt. No
     signal or kill was sent to any application process.

  Two other routes were not available:
  - **X11 `WM_DELETE_WINDOW`.** The application has no X11 window under its
    default platform.
  - **`wmctrl -c`.** Weston WM advertises no `_NET_CLOSE_WINDOW`, and `wmctrl`
    is not installed.
- **Chooser watch.** From the close until 8 s later, a UI Automation loop
  enumerated every top-level `RAIL_WINDOW` about every 20 ms. It recorded any
  title naming `BetterPullReview` or starting with `Profiles`, the chooser's
  window title.
- **Recorded outcomes.** The exit code comes from the harness's `wait()` on the
  launched process. Window identity was read from a copy of
  `state/v1/profile-registry.sqlite3` and its WAL: the
  `profile_registry_windows` rows and `profile_registry_meta.last_window_id`.

### Results

Umask `0022` is the station shell's default. All times are UTC.

| Course | Window while open (id, generation) | Close sent | Window gone | Exit | Other windows seen after close | Diagnostics | Rows after exit (`last_window_id`) |
|---|---|---|---|---|---|---|---|
| Launch 1, caption close | `1`, generation 1 | 06:21:54.542 | 06:21:54.613 | **0** at 06:21:54.585 | none | `op=application outcome=shutdown` at 06:21:54.556 | none (`1`) |
| Relaunch 2, caption close | `2`, generation 1 | 06:22:07.214 | 06:22:07.283 | **0** at 06:22:07.259 | none | shutdown at 06:22:07.230 | none (`2`) |
| Relaunch 3, explicit **Quit** | `3`, generation 1 | File > Quit at 06:22:47.899 | — | **0** at 06:22:47.932 | — | shutdown at 06:22:47.912 | **`3` retained** (`3`) |
| Relaunch 4, caption close | **`3`, generation 2**, the Quit window restored | 06:22:52.389 | 06:22:52.463 | **0** | none | shutdown at 06:22:52.405 | none (`3`) |

- **Stdout.** Every run's stdout was empty. No `STARTUP_FAILED` and no
  `PROFILE_ALREADY_OPEN` appeared.
- **Close to exit.** The process exited 43 ms and 45 ms after the close was
  posted in courses 1 and 2.
- **Relaunch 3.** Its Quit was triggered through the AT-SPI `Press` action of
  the `Quit` menu item, so that run alone had
  `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`.
- **Relaunch 4.** Its exit time was read after the 8 s window watch, so only
  its exit code is meaningful.

**Against the card's acceptance, at umask `0022`.**

- **Criterion 1: quit, exit code 0, no chooser.** Closing the last window of
  the only open profile quit with exit code 0 each time.
  - The window left the display within 69–74 ms of the close.
  - The shutdown diagnostic was written, and no chooser window was seen.
  - The native process cannot show from outside that it took the drain and
    Quit path. Two things support it: the orderly `outcome=shutdown` line, and
    the injected-policy course asserting
    `last_empty_desktop_outcome() == quit` with `is_quiescent()`.
- **Criterion 2: the restore record tells a close from a Quit.** It does:
  - A caption close removes the window row, and the next launch opens a new
    identity: 1, then 2.
  - An explicit Quit keeps the row, and the next launch restores the same
    identity at a new generation: 3 at generation 1, then 3 at generation 2.
- **Criterion 3: the native policy answers `quit`.** The automated half above
  observes it.

### Defect: a group-writable data-root parent makes every Linux close exit 1

| Course | Umask | Data-root parent (`xdg_data_home/BetterPullReview`) | Neighbourhood created | Exit after caption close |
|---|---|---|---|---|
| `scratch-0799-release-umask002` | `0002` | 775, created by the application | no | **1** |
| `scratch-0799-release-umask002-parent755` (negative control) | `0002` | 755, created before launch | yes | 0 |

Both runs still quit with no chooser and wrote `outcome=shutdown` with
`failure=none`. Only the exit code differs. It follows the data-root parent's
group-write bit through the same neighbourhood refusal described in the
automated half.

- **How common this is.** `/etc/login.defs` here sets `USERGROUPS_ENAB yes`.
  With that setting, `pam_umask` gives a user-private-group login session umask
  `0002` unless something else sets the umask. The station shell here does not
  pass through that and has `0022`. So a stock Ubuntu desktop login plausibly
  creates `~/.local/share/BetterPullReview` as 775. This station did not observe
  a real desktop login session.
- **What the user loses.** In that state the agent workspace neighbourhood is
  never created, and the failure is silent in the diagnostics log.
- **Disposition.** Reported to the integrator, not fixed. The refusal is a
  deliberate safety rule in BPR-0622's Unix ownership path. Where the
  neighbourhood lives, or how a refused neighbourhood counts at shutdown, is a
  design and security decision, with migration consequences for existing
  journals. No card was minted.

## Limitations

- **Compositor.** WSLg's compositor is Weston inside the WSL utility VM, not a
  GNOME or KDE session. Its close reaches Qt as a Wayland toplevel close, which
  is the path the course exercised.
- **Chooser watch.** It samples top-level windows about every 20 ms, plus
  enumeration time. A chooser mapped and unmapped faster than that would be
  missed. The automated course asserts `!management_window()->isVisible()`
  directly, but only under the injected policy.
- **Trial run.** A first harness, `scratch-0799-release-1`, is not evidence. It
  looked for an X11 window that never existed, so it closed nothing and left
  the first process running. A second process on the same root printed
  `PROFILE_ALREADY_OPEN` and exited. The first process was then closed through
  the same caption close and exited.
- **Out of scope.** No GitHub or GHES request, sign-in, credential read or
  vault write took place. Scratch roots and course JSON are kept outside the
  checkout under `/home/mmarcin/bpr-station-logs/task-20260917-lnx-evidence/`
  and are not committed.

## Rerun after BPR-0817, `task-20260917-lnx-build`

### Revision, image and host

- **Revision.** Product source `1c6eaf21`. That is base `20a72bf7`, which
  includes BPR-0808, plus [BPR-0816](BPR-0816-linux-gcc15-stock-build.md)'s GCC
  fixes (`4883dbd9`) and [BPR-0817](BPR-0817-linux-owner-private-data-root.md)'s
  owner-private data root (`1c6eaf21`).
- **Build.** Stock presets under GCC 15.2 `-Werror`, with no demoted class:
  `cmake --preset app-release --fresh` and `cmake --preset dev --fresh`, then
  builds.
- **Image.** The tests-disabled `build/app-release/src/app/better_pull_review`
  (Release, `BUILD_TESTING=OFF`) has SHA-256
  `9b9ef069aba611ef77d927720b2c21b1366267105f1b1c1f98bf30cbea172f4a`.
- **Host.** Same host and WSLg display as above. Qt chose Wayland on Weston.
  The Secret Service `login` collection read `(<false>,)` before the course.
- **Harness.** `task-20260917-lnx-build/tools/course_0817.py`.
  - It sets the process umask before it creates anything, then builds a new
    scratch XDG root under that umask.
  - Each launch waits for its window row plus 3 s. It is then closed either by
    posting the caption close (`WM_SYSCOMMAND`/`SC_CLOSE`) to its RAIL window,
    or by pressing File > Quit through AT-SPI. No signal or kill was sent.
  - After each caption close, it samples every BetterPullReview or Profiles
    RAIL window for 8 s. Registry rows and directory modes are copied after
    each exit.

### Criterion 1: quit with exit code 0 and no chooser

| Course (UTC, 2026-09-17) | Umask | Launch | Close sent | Exit | Close to exit | Windows seen after close | Diagnostics |
|---|---|---|---|---|---|---|---|
| `candidate2-umask002` run 1 | 0002 | 08:21:31.698 | caption close 08:21:35.394 | **0** at 08:21:35.436 | 42 ms | none | `outcome=started`, then `outcome=shutdown` |
| `candidate2-umask002` run 2 | 0002 | 08:21:44.421 | caption close 08:21:48.099 | **0** at 08:21:48.139 | 40 ms | none | as above |
| `candidate2-umask002` run 3 | 0002 | 08:21:57.158 | File > Quit (AT-SPI `Press`) | **0** at 08:22:00.439 | — | — | as above |
| `candidate2-umask002` run 4 | 0002 | 08:22:01.441 | caption close 08:22:04.901 | **0** at 08:22:04.948 | 47 ms | none | as above |
| `candidate2-umask022` run 1 | 0022 | 08:22:14.032 | caption close 08:22:17.691 | **0** at 08:22:17.730 | 39 ms | none | as above |
| `candidate2-umask022` run 2 | 0022 | 08:22:26.785 | caption close 08:22:30.434 | **0** at 08:22:30.476 | 42 ms | none | as above |

- **Windows seen.** "None" means that apart from the closing window, which the
  watch sees once as it goes, no BetterPullReview or Profiles window appeared.
  Stdout was empty in every run.
- **Fresh data root under umask 0002.** After exit, the application had created
  `xdg_data_home/BetterPullReview` at **0700**, the data root at 0700 and
  `BetterPullReview-agent-workspaces-v1` at 0700. The harness's own
  `xdg_data_home` was 0775.
- **Negative control.** The same course on the pre-fix image (`ef3f782b`,
  SHA-256 `307c1793…`) exits **1**, and the parent is 0775 with no
  neighbourhood (`course-0817-prefix-umask002.json`).

### Criterion 2: the restore record tells a close from a Quit

The umask 0002 course above shows it:

- **Caption closes.** Run 1 opened window `1` at generation 1, and its close
  dropped the row. Run 2 opened a **new** identity, `2`, and its close dropped
  that row too.
- **Quit.** Run 3 opened window `3`, and its File > Quit **kept** the row:
  `3`, generation 1.
- **After Quit.** Run 4 restored **`3` at generation 2**. Its caption close
  dropped the row, with `last_window_id` still 3.

### Criterion 3: the native policy answers `quit`

`application_profile_management_public_path` passes on the stock `dev` tree
at `1c6eaf21`: 2.88 s, `profile-management-public-path:pass`
(`focused-0817.ctest.log`).

- **Environment.** It ran under the host's root-owned 1777 `/tmp` with
  `TMPDIR` unset. The failure recorded earlier, "and an orderly exit code",
  is gone.
- **What it asserts.** On non-Apple platforms the course asserts
  `app::native_empty_desktop_policy()() == app::empty_desktop_outcome::quit`
  (`profile_management_public_path_tests.cpp:269`).
- **What it does not show.** Its close and relaunch section still uses an
  injected policy. Only the native assertion is cited.

### Disposition

All three criteria pass on target-native Linux at `1c6eaf21`, and the card
moves to Done.

**Limitation carried to BPR-0817.** Suppose `$XDG_DATA_HOME/BetterPullReview`
already existed as 0775, for example from an install made before `1c6eaf21`
under umask 0002.

- The neighbourhood is still refused, and closing still exits 1
  (`course-candidate2-umask002-parent775.json`).
- Whether that exit should be 0 is BPR-0817's parked owner question.

**Other limitations.**

- **Session.** WSLg runs Weston, not a GNOME or KDE session. Umask 0002 came
  from the harness, not a `pam_umask` desktop login.
- **Chooser watch.** It samples about every 20 ms.
- **Logs.** Logs and course JSON are under
  `/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/native/` and are not
  committed.
