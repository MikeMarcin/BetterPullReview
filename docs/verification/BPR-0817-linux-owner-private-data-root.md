# BPR-0817 Linux owner-private data root evidence

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-2`), mandate r2, task `task-20260917-lnx-build`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`.
- **State.** The defect fix is at `1c6eaf21`, and the card is **Parked on
  `human-review`**.
  - **Criterion 1 passes natively.** Under umask 0002 a fresh data root and
    its parent are created 0700, the neighbourhood is created, and every close
    exits 0.
  - **Criterion 3 passes on Linux** under the stock `/tmp`. It is still open on
    macOS.
  - **Criterion 2 waits on the owner.** It has two questions, listed at the end:
    the exit code for a refused workspace recovery, and where the refusal should
    be visible.
- **Follow-on.** BPR-0799's three criteria pass at `1c6eaf21`, and that card
  is Done ([BPR-0799's Linux evidence](BPR-0799-linux-native-empty-desktop-quit.md)).
- **Next, for the owner.** Answer the two questions at the end of this file.
  Then implement the recorded exit-code and visibility decision and rerun the
  pre-existing-0775-parent course. The macOS half of criterion 3 belongs to
  main.

## Revisions and host

- **Revisions.** The claim is `075c4a5e`. The fix is `1c6eaf21`, on top of
  [BPR-0816](BPR-0816-linux-gcc15-stock-build.md) at `4883dbd9`.
- **Candidate equals the commit.** Every candidate result below was produced
  from a working tree whose `src/` and `tests/` are byte-identical to
  `1c6eaf21`.
- **Builds.** Stock `dev` and `app-release` trees under GCC 15.2 `-Werror`, as
  configured fresh for BPR-0816. The rebuild after review exited 0 with no
  warning (`bpr0817-final-build.log`).
- **Image.** The tests-disabled image `build/app-release/src/app/better_pull_review`
  has SHA-256
  `9b9ef069aba611ef77d927720b2c21b1366267105f1b1c1f98bf30cbea172f4a`.
- **Host.** JaegAW, WSL2 kernel `6.18.33.2-microsoft-standard-WSL2`, Ubuntu
  26.04 LTS, Qt 6.10.3. The WSLg display runs Weston, and Qt chose its Wayland
  platform. `/tmp` is tmpfs, owned by `root`, mode 1777, with `TMPDIR` unset. The
  Secret Service `login` collection read `(<false>,)` before each native
  course.

## Cause

Startup's registry-root creation is in `github_application_startup::load`. It
called `std::filesystem::create_directories` and then narrowed only the root
with `make_storage_root_owner_private`. `create_directories` takes its modes
from the umask, so under 0002 `$XDG_DATA_HOME/BetterPullReview` came out 0775.

The Unix agent workspace neighbourhood is a sibling of the Default profile's
data root. `prepare_workspace_neighbourhood` correctly refuses a parent that
group or other can write, and that refusal became `AgentWorkspaceCleanupFailed`
and then exit code 1.

Non-Default profile roots (`<root>/profiles/<id>`) were already created
private by the adapter.

## Change (`1c6eaf21`)

All files are portable.

- **`src/adapters/sqlite/include/bpr/adapters/sqlite/storage_root.hpp` and
  `src/adapters/sqlite/src/catalog_database.cpp`.** New
  `create_storage_root_owner_private`:
  - It requires an absolute, lexically normal root. It walks to the deepest
    existing ancestor, which must be a real directory with its own canonical
    spelling.
  - **POSIX.** The existing ancestor is opened `O_DIRECTORY|O_NOFOLLOW`. Each
    missing component is created with `mkdirat(parentFd, name, 0700)`, opened
    `O_NOFOLLOW`, checked to be the effective user's directory, then set to
    exactly 0700 with `fchmod` on that descriptor and read back.
  - **Existing directories.** A component a concurrent creator made first is
    accepted as it stands. No pre-existing directory is ever changed. The root,
    however it came to exist, is accepted only when
    `validate_private_storage_root` admits it.
  - **Windows.** Keeps `create_directories` plus
    `make_storage_root_owner_private`, as before.
- **`src/app/src/github_application.cpp`.** `load()` calls the new function in
  place of the old pair.
- **Unchanged.** `prepare_workspace_neighbourhood`, its refusal rule and its
  `mkdir(S_IRWXU)`; a umask can only clear bits, so 0700 stays owner-only. The
  profile desktop's exit code, the `CleanupFailed` accounting, and the agent
  panel's repair message are also unchanged.
- **Tests.**
  - `tests/sqlite_contract/storage_root_tests.cpp`
  - `tests/scripts/agent_workspace_ownership_tests.py`
  - `tests/end_to_end/profile_startup_public_path_tests.cpp`
  - `tests/end_to_end/profile_management_public_path_tests.cpp`
- **macOS.** A folder the application newly creates on the way to its data root
  is now 0700 instead of following the umask. Existing folders are untouched.

## Automated evidence

**Before the change.** At the pre-change source (`075c4a5e`; `76733a2b`
differs only in documentation), with stock `/tmp`,
`application_profile_management_public_path` failed with `FAILED: and an
orderly exit code` (`bpr0817-worker/baseline-ctest.log`).

**At the candidate.** `ctest --preset dev -j1 -V -R <12 names>` started at
2026-09-17T08:20:17Z, took 61 s and exited with ctest rc 8
(`focused-0817.ctest.log`):

| Test | Result | What it now proves |
|---|---|---|
| `sqlite_storage_root_contract` | **Pass** | Under umask 0002, where an ordinary `create_directory` gives 0775, `fresh/a/b` is created with all three folders exactly 0700 and the catalog opens. A second call changes nothing. A pre-existing 0775 ancestor stays 0775 while its new children are 0700. A pre-existing 0775 root is refused and not repaired. Symbolic-link, dangling-link, regular-file, relative, non-normal and trailing-separator spellings are refused and create nothing. |
| `agent_workspace_ownership` | **Pass** | A missing neighbourhood under an existing user-owned 0775 or 01777 parent is refused (`failure=1`); nothing is created and the parent's mode is unchanged. Under a 0700 parent with umask 0002, the neighbourhood and registry are created exactly 0700. The existing SIGKILL courses are unchanged. |
| `application_profile_startup_public_path` | **Pass** | Under umask 0002, admitting the Default profile at a new nested `fresh-install/BetterPullReview/BetterPullReview` creates all three folders exactly 0700. Recovery creates the neighbourhood at 0700, and the closed profile reports neither `AgentWorkspaceCleanupFailed` nor `ShutdownCleanupFailed`. The existing refusal of a pre-existing shared root is kept. |
| `application_profile_management_public_path` | **Pass** under root-owned 1777 `/tmp`, `TMPDIR` unset | The data root is now `<private scratch>/data`. Every assertion is kept, including `native_empty_desktop_policy()() == quit` and the orderly exit code. |
| `sqlite_profile_storage_root`, `sqlite_profile_registry_storage`, `git_process_contract`, `architecture_boundaries`, `application_agent_crash_recovery` (BPR-0808 repair message), `application_agent_materialization_crash_recovery` | Pass | |
| `application_review_ledger_composition_contract`, `application_profile_multi_window_public_path` | Fail, **unchanged** | The same pre-existing failures as at `4883dbd9`, with identical evidence bits and assertion ([BPR-0816](BPR-0816-linux-gcc15-stock-build.md), [BPR-0806](BPR-0806-linux-storage-courses.md)) |

**Negative controls**, run by the slice owner and restored by hash:

- **The old creation code put back** inside the new function:
  - `sqlite_storage_root_contract` fails with `the outermost created ancestor
    is exactly 0700 under umask 0002`.
  - `application_profile_startup_public_path` fails with `the outermost folder
    a fresh install creates is exactly 0700 under umask 0002`. With its
    assertions made non-fatal, every later assertion also disagrees.
- **The neighbourhood refusal removed**, in a scratch copy:
  `shared-parent-0775: refused recovery created an entry`.
- **The neighbourhood `mkdir` widened to 0777**, in a scratch copy: the
  ownership driver fails.

**Other gates.**

- The clang 22 proxy passes (rc 0) on all six C++ files
  (`clang-proxy-0817.log`).
- `format_check.py` with `/usr/bin/clang-format` 22.1.8 passes, and
  `git diff --check` is clean.

## Native evidence on WSLg

The harness is `tools/course_0817.py`.

- **Setup.** The harness sets the umask before it creates anything, builds a
  new scratch XDG root under that umask, and can pre-create the data root's
  parent with a given mode.
- **Each launch.** It launches the tests-disabled image on the WSLg display and
  waits for the window row in `profile-registry.sqlite3`, plus 3 s.
- **Close.** A launch is closed in one of two ways:
  - posting `WM_SYSCOMMAND`/`SC_CLOSE` to the application's RAIL window, which
    is the caption close;
  - or pressing File > Quit through AT-SPI.

  No process was signalled or killed.
- **Chooser watch.** For 8 s after a caption close, every RAIL window titled
  BetterPullReview or Profiles is sampled.
- **Recorded.** Exit code, timing, registry rows, diagnostics, and the modes of
  `xdg_data_home`, the data root's parent, the data root, the neighbourhood and
  the ownership registry.

| Course | Image | Umask | Parent before launch | Launches | Exit codes | Parent after | Neighbourhood |
|---|---|---|---|---|---|---|---|
| Negative control, `prefix-umask002` | pre-fix `ef3f782b` (`build/probe-app-release`, SHA-256 `307c1793…`) | 0002 | absent | caption close | **1** | **0775** | absent |
| `candidate2-umask002` | `9b9ef069…` | 0002 | absent | close, close, Quit, close | **0, 0, 0, 0** | **0700** | **0700**, created |
| `candidate2-umask022` | `9b9ef069…` | 0022 | absent | close, close | **0, 0** | 0700 | 0700, created |
| `candidate2-umask002-parent775` | `9b9ef069…` | 0002 | **0775, pre-existing** | caption close | **1** | 0775, not repaired | absent (refused) |

- **Diagnostics.** Every run wrote `op=application outcome=started` and
  `outcome=shutdown`. Stdout was empty.
- **Chooser.** Apart from the closing window itself, no BetterPullReview or
  Profiles window was seen after any close.
- **XDG data home.** Under umask 0002 the harness's own `xdg_data_home` is
  0775, as a login under that umask would make `~/.local/share`. The
  application's folders inside it are 0700.
- **Timing.** Each fresh-install caption close exited 39–47 ms after the close
  was posted.
- **First run.** An earlier run on the pre-review candidate image `4a76aeba…`
  gave the same exit codes and modes in all three courses. The review
  correction below touches only how the created root is validated.

**Is the refusal visible to the person?** With the pre-existing 0775 parent,
the image was relaunched with the AT-SPI bridge on and its accessible tree was
walked 3–8 s after the window appeared. There are 470 nodes.

- **Not showing at Home.** The agent panel exists in the tree (`AI agent`,
  `AI agent status`, …) but is **not showing** at Home.
- **No refusal text.** No accessible name contains "Disposable workspace
  recovery" or "Not started". `application_agent_crash_recovery`'s in-process
  course still proves the panel's status carries the message.
- **Diagnostics.** The log reads `outcome=shutdown` with no failure detail.
- **Result.** At Home, the only signal a person gets is the exit code of 1.
  The logs are `native/treedump2-parent775.json`, and `refusal_scan` in
  `native/course-candidate2-umask002-parent775.json`.

## Independent review

An `rfc-reviewer-critical` reviewed the candidate for this named risk: a link,
a substitution, a concurrent creator or a pre-existing shared folder must not
yield a writable data root or neighbourhood parent, must not modify a
directory the call did not create, and must not lose the shared-parent
refusal. It found **no blocking finding**.

- **Finding 1.** A created root was still narrowed through a path-based,
  link-following `chmod`. With a non-sticky shared ancestor, a swapped-in link
  could redirect that `chmod`; the reviewer demonstrated the primitive. This
  was not a regression.
- **Finding 2.** A created root was validated more weakly than a pre-existing
  one.
- **Finding 3.** The header overstated what is checked on a concurrently
  created ancestor.

All three were adopted before commit. A created root now relies only on the
descriptor `fchmod` and is validated like any other, and the header wording was
corrected. The reviewer's narrow recheck found all three resolved and no
regression. The same `validate_private_storage_root` already guards the next
step in `load()`.

Two limitations remain:

- Other-user-owned adversarial layouts could not be built from this account,
  so they were reasoned about, not reproduced.
- No Windows or macOS compile was possible. The Windows branch is textually
  the previous behaviour.

## Against the card's acceptance

| Criterion | Outcome | Evidence |
|---|---|---|
| On the Linux station under umask 0002, a fresh data root is created owner-only, and closing the last window exits 0 with the neighbourhood created | **pass** at `1c6eaf21` | `candidate2-umask002`: parent and root 0700, neighbourhood 0700, four exits of 0. Negative control on the pre-fix image: exit 1, parent 0775. `application_profile_startup_public_path` asserts the same through the composition. |
| With a pre-existing 775 parent, the refusal is visible to the person, and the exit code follows the recorded decision | **pending the owner** | Refusal kept and nothing repaired (automated and native). The exit code is still 1, unchanged, because no decision is recorded. The refusal is not showing at Home; it is presented only in the agent panel. |
| `application_profile_management_public_path` passes under the stock `/tmp` on Linux, and still passes on macOS | **Linux: pass; macOS: not run** | Passes with root-owned 1777 `/tmp` and `TMPDIR` unset, where it failed before. The macOS run belongs to main. |

## Questions for the owner

RFC-BPR-0013, RFC-BPR-0033 and RFC-BPR-0024 do not say what a refused agent
workspace recovery means for the exit code or where it must be shown.

1. **Exit code.** Take a data folder whose parent already existed as
   group-writable, for example `~/.local/share/BetterPullReview` left 0775 by an
   earlier install under umask 0002. BetterPullReview refuses to create agent
   workspaces there. Should closing the last window then still exit with code
   **1**, which is today's behaviour because the refusal counts as a shutdown
   cleanup failure? Or should an otherwise orderly quit exit **0**, with the
   refusal reported only in the application?
2. **Visibility.** That refusal is shown today only as the agent panel's "Not
   started: Disposable workspace recovery needs repair or has reached its
   limit.", and the panel is not showing at Home. Is that enough? Or must the
   refusal also be visible at Home, for example in the window's status bar, the
   way RFC-BPR-0033 reports a failed profile operation?

Neither answer changes the fix for fresh installs above. An existing install
that already has a 0775 parent keeps the refusal and exits 1 until these are
answered, and nothing repairs that folder's mode.

## Limitations

- **Environment.** WSLg runs Weston, not a GNOME or KDE session. Umask 0002 was
  applied by the harness, not by a real `pam_umask` desktop login.
- **Chooser watch.** It samples about every 20 ms.
- **Not run here.** Windows and macOS were not compiled or run.
- **Logs.** Everything is under
  `/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/`: `focused-0817.*`,
  `clang-proxy-0817.log`, `bpr0817-final-build.log`, `bpr0817-worker/`,
  `native/`, `native.summary` and `tools/`. None is committed.
- **Boundaries.** No GitHub or GHES request, credential read or live agent was
  used.
