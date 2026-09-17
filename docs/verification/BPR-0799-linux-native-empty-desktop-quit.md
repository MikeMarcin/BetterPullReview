# BPR-0799 Linux native empty-desktop quit evidence

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2`, task
  `task-20260917-lnx-evidence`. Station `bpr-linux` (`stn-3f7fcc92`, `wsl2`),
  branch `linux-lane`. BPR-0622's evidence is pushed at `2285f4a0`.
- **State.** The automated and native courses are recorded below.
  - B1's first criterion fails on Linux under a group-writable data-root
    parent, which a umask of `0002` produces. The card cannot be Done until that
    defect is resolved or explicitly ruled out of scope.
  - The card stays in Backlog, unmoved. Every lane move rewrites its link in
    `docs/rfcs/README.md`, and this dispatch forbids tracker README edits.
- **Next.** BPR-0731, from the same `app-release` and `dev` trees.

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
