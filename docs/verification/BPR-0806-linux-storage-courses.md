# BPR-0806 Linux storage courses

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-2`), mandate r2, task `task-20260917-lnx-build`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`.
- **State.** The Linux half of BPR-0806's fourth criterion is recorded below.
  - Six of the seven Linux storage and profile courses pass on the stock build.
  - `architecture_boundaries` passes.
  - `application_profile_multi_window_public_path` aborts on a disengaged
    `std::optional` in the course's own assertion. The failure is identical on
    the pre-change tree and unrelated to BPR-0806's sources.
- **BPR-0806** is integrator-owned and stays In Progress; this station did not
  edit it. The Windows half is
  [the Windows evidence](BPR-0806-windows-profile-storage-startup.md).

## Revision, build and host

- **Revision.** Product source `4883dbd9`: base `20a72bf7`, which includes
  bpr-win's BPR-0806 commits merged in `c0f2fa31` and `53a0a747`, plus
  [BPR-0816](BPR-0816-linux-gcc15-stock-build.md)'s GCC fixes. The run's HEAD was
  `3bb60d90`, whose only further change is documentation.
- **Build.** Stock `dev`: `cmake --preset dev --fresh` (empty
  `CMAKE_CXX_FLAGS`), then a `--clean-first` build at `4883dbd9` under GCC 15.2
  `-Werror`. `ninja -n` reported no pending work before the run.
- **Host.** JaegAW, WSL2, kernel `6.18.33.2-microsoft-standard-WSL2`, Ubuntu
  26.04 LTS, Qt 6.10.3 `gcc_64`. The Secret Service `login` collection read
  `(<false>,)` before the run.

## Courses

The registered names were confirmed with `ctest --preset dev -N`. They are
exactly the seven names dispatched. The course ran serially:

```sh
ctest --preset dev -j1 -V -R '^(sqlite_profile_registry_storage|sqlite_window_review_restoration_storage|sqlite_device_binding_storage|sqlite_profile_storage_root|sqlite_profile_runtime_lease|application_profile_startup_public_path|application_profile_multi_window_public_path)$'
```

It started at 2026-09-17T07:47:37Z, took 11 s and exited with ctest rc 8.
`architecture_boundaries` ran in the BPR-0622 set on the same tree at
07:45:31Z.

| Test | Result | Duration | Marker |
|---|---|---|---|
| `sqlite_device_binding_storage` | Pass | 0.43 s | `device-binding-storage-tests:pass` |
| `sqlite_profile_registry_storage` | Pass | 0.33 s | `profile-registry-storage-tests:pass` |
| `sqlite_profile_storage_root` | Pass | 0.00 s | `profile-storage-root-tests:pass` |
| `sqlite_profile_runtime_lease` | Pass | 0.00 s | `profile-runtime-lease-tests:pass` |
| `sqlite_window_review_restoration_storage` | Pass | 0.16 s | `window-review-restoration-storage-tests:pass` |
| `application_profile_startup_public_path` | Pass | 1.44 s | `profile-startup-public-path:pass` |
| `application_profile_multi_window_public_path` | **Aborted** | 8.94 s | see below |
| `architecture_boundaries` | Pass | 11.09 s | `Application, provider, and Qt architecture boundaries verified` |

## The multi-window abort

```text
/usr/include/c++/15/optional:1165: constexpr const _Tp* std::optional<_Tp>::operator->() const [with _Tp = bpr::core::file_receipt]: Assertion 'this->_M_is_engaged()' failed.
```

- **Pre-existing.** The same test, run through `ctest --test-dir
  build/probe-dev` on a complete `ef3f782b` tree, aborts with the same message
  after 8.99 s (`baseline-0806-multiwindow.ctest.log`). That tree predates the
  BPR-0816 source changes, apart from one unrelated line.
- **Where.** An `rfc-scout` ran the executable on that tree four times with the
  course's environment, twice under `lldb-22`. It aborts every time in `main` at
  `tests/end_to_end/profile_multi_window_public_path_tests.cpp:573`, on
  `windowB->review_editor()->shown_receipt()->Read`. That is the course's own
  `expect`, in the same-review receipt step.
- **Why.** The wait at line 560 does see both receipts engaged. The course then
  selects a conversation line in each window (561–562). Each selection runs
  `diff_workbench::select_conversation_line` →
  `github_application::implementation::adopt_review_selection` →
  `review_draft_editor::present(clear_reason)`, which calls
  `ShownReceipt.reset()` (`review_draft_editor.cpp:770`). Nothing presents a
  receipt again before line 573, which dereferences without checking.
- **What it is.** The course and the product disagree about whether a line
  selection keeps the shown file receipt. The line-573 check and the reset both
  date from 2026-09-08 (`78d4fa5c`, and `fd270e33` "Finish BPR-0704").
  - Nothing here is specific to Linux. libstdc++'s Debug assertions only turn
    the dereference into an abort.
  - On macOS it is most likely undefined behaviour reading stale bytes; that is
    inferred, not observed.
  - It touches neither the path conversion nor the storage sources that
    BPR-0806 changed.
- **Reported, not fixed.** It is outside BPR-0806's Linux half and outside this
  dispatch's authority. Either the course should check after the line
  selections, or the selection should present the receipt again, and BPR-0704's
  owner has to say which is intended.

## Against BPR-0806's fourth criterion (Linux half)

"The macOS and Linux storage courses that cover the changed sources still
pass, and `architecture_boundaries` passes."

- **Linux storage courses: pass.** All five `sqlite_*` storage contracts and
  `application_profile_startup_public_path` pass on the stock build.
- **`architecture_boundaries`: pass.**
- **`application_profile_multi_window_public_path`: fails on Linux,
  independently of BPR-0806.** See above. Whether that blocks the criterion is
  for the integrator on main.
- **Not covered here.** The macOS courses and every Windows criterion.

## Limitations

- **Environment.** WSL2 kernel. Offscreen Qt platform for the public-path
  courses.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/`
  (`run-0806.ctest.log`, `run-0622.ctest.log`, `run-0622-0806.summary`,
  `baseline-0806-multiwindow.ctest.log`). The scout's `lldb` script and output
  are in `scout-mw/` there. None is committed.
- **Boundaries.** No GitHub or GHES request, credential read or live agent was
  used.
