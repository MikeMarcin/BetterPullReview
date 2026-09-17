# BPR-0806 Windows profile storage startup evidence

**Station and task.** Station `bpr-win` (JaegAW, `windows-native`, branch
`windows-lane`) ran task `task-20260917-win-storage` in run
`run-20260916-stations`, dispatched by `bpr-main-mac`.

**Revisions.** The baseline is `f2b8e682255fc553a87d4d164c3b27f061300e07`.
The change consists of two commits:

- `d0964fdd4a653f7ab7ea27757eeaedce2378a0e8`, the storage fix and contract;
- `d587b5f1`, the course's diagnostics claim and the Windows registrations.

Both sit on `32a983e6` ([[BPR-0805]], `/bigobj`) and `f2b8e682`
([[BPR-0804]]). Every result below was produced at `d587b5f1` unless a row names
another revision.

**Toolchain and host.** Visual Studio 18 Community, MSVC 14.51.36231, Qt 6.10.3
`msvc2022_64`, with Git for Windows perl appended to `PATH`. Smart App Control
is off; both registry values read `0`.

## The defect as found

At `f2b8e682` the `windows-app-release` image failed on an empty default data
root. About one second after launch, before any window, it printed
`STARTUP_FAILED` and exited 1. The card records the trace, taken with throwaway
diagnostics that were reverted before any commit:

1. Qt's `C:/Users/...` spelling reached `canonical_windows_root`, which refuses
   `/`.
2. With the separators converted, the profile registry refused its own existing
   file. It checked POSIX group and other permission bits, which MSVC's
   `std::filesystem` always reports as set.

`application_profile_management_public_path` failed in 0.30 s with
`FAILED: legacy appearance store`, for the first reason.

## What changed

- **Qt-boundary conversion.** The new `bpr/app/native_filesystem_path.hpp`
  applies `QDir::toNativeSeparators` on `_WIN32`. It replaces private copies in
  the composition and in `main.cpp`, and the temporary roots in both use it.
  The end-to-end fixtures that built roots from `QFileInfo{}.canonicalFilePath()`
  use it too. `canonical_windows_root` is unchanged.
- **Store-file admission.** `detail::admit_private_store_file` and
  `detail::restrict_private_store_file` replace the permission-bit check in the
  profile registry, window restoration and device binding stores.
  - POSIX keeps the previous check.
  - Windows works from the retained `state/v1` handles and calls
    `open_private_object(regular_file, WRITE_DAC, protect_inherited)`. This
    requires one link, no reparse point, a matching handle path, an owner-only
    owner, and a single private grant. The grant a new sqlite file inherits is
    made explicit. Anything wider is refused and not repaired.
- **Runtime lease trace.** `validates_profile_runtime_lease` returns `false` on
  `_WIN32`, but only the POSIX activation server calls it
  (`profile_activation.cpp`). `make_profile_activation_server` is unavailable
  on Windows. Startup, relaunch and second-process refusal use the real lease:
  `profile-runtime.lock` with share mode 0 and a strict DACL. The stub means
  only that Windows has no single-instance activation handoff.
- **Private fixture roots.** Fixture roots previously relied on
  `std::filesystem::permissions(owner_all)`, which does nothing on Windows. They
  now call `make_storage_root_owner_private`. This applies to the sqlite store
  courses (device binding, profile registry, window restoration) and to the
  Windows-registered profile public paths.
- **Diagnostics claim per platform** (integrator ruling).
  `application_profile_management_public_path` reads the logging adapter's
  typed `diagnostic_log_status` from a probe `quill_log`:
  - Where the file sink is `available`, the per-profile startup and shutdown
    assertion is unchanged.
  - On Windows the probe must report `unavailable`, and no concurrent profile
    may have a `logs/diagnostics.log`.

  Windows has no file sink. [[BPR-0701]] made no Windows claim, and
  [[BPR-0603]] holds Windows file support in the owner-deferred RFC-BPR-0027
  phase.
- **Registrations** (integrator ruling). Five rows are now
  `WINDOWS_UNCONTAINED` and pinned in `scripts/verify_test_network_isolation.py`
  with the measured cause `dos-volume-name`:
  - `application_windows_profile_storage_startup_contract`
  - `application_profile_management_public_path`
  - `application_profile_startup_public_path`
  - `sqlite_profile_registry_storage`
  - `sqlite_window_review_restoration_storage`

  `sqlite_device_binding_storage` was already pinned.

## Native tests-disabled image

**Build.** `cmake --preset windows-app-release` then `cmake --build --preset
windows-app-release`, from a `vcvars64` shell. The output is
`build/windows-app-release/src/app/better_pull_review.exe`, with SHA-256
`46619a531e8ffd51d6cd25b3c3431f7d4e15e43cc728b3986d781bc9919eca51` at
`d587b5f1`. The image compiles in the attended-review source digest, so a
tracked test or script change relinks it. The `d0964fdd` image hashed
`f249fad47843e2a750af33664c4890296fbe1e58863bc5b7458e6b111f552ca0` and gave the
same results.

**Launch conditions.**

- Launched directly on the interactive desktop.
- The Qt runtime directory was prepended to the image's own `PATH`.
- Empty default root: `%LOCALAPPDATA%\BetterPullReview\BetterPullReview`. The
  owner's earlier root was moved aside for these runs and is restored before
  the station stops.
- The window was closed with UI Automation `WindowPattern.Close`, the caption
  close, never a process kill.

| Step | Result |
|---|---|
| First launch, empty root | `BetterPullReview — Default profile` window 0.8 s after launch |
| Close the only window | Exit 0, 55 ms after the close. No chooser. The only other visible top-level window was the `PseudoConsoleWindow` that redirected stdout brings. |
| Relaunch on the same root | Window after 0.7 s, no `STARTUP_FAILED`. Close gives exit 0 after 69 ms. |
| Window identity (`profile_registry_windows`, `profile_registry_meta.last_window_id`) | Launch 1 holds window 1; its row is gone after the close. The relaunch holds window 2 (`last_window_id` 1 then 2). |
| Second process while the first runs | stdout `PROFILE_ALREADY_OPEN`, exit 1. The first keeps its one window and exits 0 when closed. |
| DACLs (`icacls`, at `d0964fdd`) | `profile-runtime.lock`, `profile-registry.sqlite3`, `window-restoration.sqlite3` and `device-bindings.sqlite3` show `JAEGAW\mikem:(F)`, protected. `state\v1` shows `JAEGAW\mikem:(OI)(CI)(F)`. |

## Focused contract

`application_windows_profile_storage_startup_contract` is registered on `WIN32`
only. It checks:

- A Qt forward-slash root becomes the native spelling.
- `validate_private_storage_root` still refuses `/`, a UNC spelling
  (`\\localhost\C$\...`), a `\\?\` spelling and a relative root.
- `resolve_profile_storage_root` refuses the unconverted root and resolves the
  converted one.
- A new registry is created, and its file is admitted on the second and third
  opens.
- A second hard link is refused; the file is admitted again once the link is
  removed.
- A file whose DACL grants Everyone read is refused, and is still refused on a
  second attempt.

Two local trials at `d0964fdd`, both reverted, show that it fails without the
fix:

| Trial | Result |
|---|---|
| The header returns the Qt spelling unconverted | `FAILED: a Qt forward-slash root becomes the native Windows spelling` |
| The registry store restores the POSIX permission-bit check | `FAILED: a second open admits the registry file it created` |

## Courses through ctest on `windows-dev`

**Command.** From a `vcvars64` shell with Git's `usr/bin` appended:

```bat
ctest --preset windows-dev -R "^(application_windows_profile_storage_startup_contract|application_profile_management_public_path|application_profile_startup_public_path|sqlite_profile_registry_storage|sqlite_window_review_restoration_storage)$" --output-on-failure
```

| Course | Result at `d587b5f1` |
|---|---|
| `sqlite_profile_registry_storage` | Passed, 1.48 s |
| `sqlite_window_review_restoration_storage` | Passed, 1.61 s |
| `application_profile_startup_public_path` | Passed, 9.91 s |
| `application_profile_management_public_path` | Passed, 13.85 s, including `native_empty_desktop_policy()() == quit` and the Windows no-sink claim |
| `application_windows_profile_storage_startup_contract` | Passed, 0.07 s |

Run directly at `d0964fdd`, `sqlite_profile_storage_root`,
`sqlite_device_binding_storage` and `sqlite_profile_runtime_lease` passed in
0.06 s, 0.78 s and 0.04 s. Through ctest, `sqlite_device_binding_storage` passed
in 0.78 s and `sqlite_profile_runtime_lease` in 0.02 s.

**Measured contained cause.** At `d0964fdd` the five rows were still contained.
A temporary terminate hook, reverted, printed the exception each contained row
died on:

- The registry, restoration and contract fixtures all failed with
  `filesystem_error: canonical: Access is denied` on
  `C:\Users\<user>\AppData\Local\Packages\bprautomatedtest\AC\Temp\...`, before
  any course step.
- The two public paths printed `FAILED: private desktop test root under the
  canonical storage policy`, because `make_storage_root_owner_private` refused
  the same package temp root.

This is [[BPR-0362]]'s `dos-volume-name`. All five pass uncontained.

**Network-isolation gate.** `ctest --preset windows-dev -R
"^test_network_isolation_manifest$"` fails with the same 25 sorted
`TEST_NETWORK_ISOLATION_FAILED` lines before the change (at `1f29eaa4`) and after
it (at `d587b5f1`). The delta is zero, so the five pins introduce no finding and
none of them appears in either list. BPR-0806 does not pin or change the
pre-existing findings; the integrator is carding them on `main`. They are:

- eight unpinned uncontained rows, each with three lines: *not in the pinned
  Windows set*, *does not start with* the sandbox launcher, and *lacks the
  `bpr_network_isolated` label*:
  - `application_agent_chat_anchored_continuation_contract`
  - `application_agent_finding_dif_context_contract`
  - `application_epoch_ordinary_update_recreated`
  - `application_epoch_portable_occurrence_two_catalogues`
  - `git_sparse_cache_contract`
  - `profile_window_restoration_fixture_contract`
  - `sqlite_profile_activation`
  - `sqlite_profile_runtime_lease`
- `raw add_test ownership is ['cmake/BprTestIsolation.cmake',
  'tests/github_contract/CMakeLists.txt']`.

`python tests/scripts/verify_test_network_isolation_tests.py` passes: 46 tests,
9 skipped.

## Not introduced, and outside BPR-0806

| Row | Result at `d587b5f1` | Before the storage fix | Status |
|---|---|---|---|
| `architecture_boundaries` | `Provider data crossed the core/Qt boundary in src/adapters/qt/src/workbench_window.cpp: https?://` | The same finding at `f2b8e682`, from `cmake -DBPR_ROOT=<git archive f2b8e682> -P cmake/VerifyArchitectureBoundaries.cmake` | Not introduced. The script stops at its first finding. |
| `application_profile_multi_window_public_path` | Through ctest, still contained: `FAILED: private desktop test root under the canonical storage policy`. Run directly at `d0964fdd` (`build\windows-dev\tests\end_to_end\Debug\profile_multi_window_public_path.exe`, `QT_QPA_PLATFORM=offscreen`): after 30.45 s, `optional(429) : Assertion failed: operator->() called on empty optional` | With the conversion but POSIX-only fixture roots: `startup failure: 5`, `FAILED: profile owner admitted` in 0.09 s | Integrator's card. The storage fix moves it from startup refusal to a later empty-optional assertion. |
| `application_device_binding_restart` | Through ctest (already uncontained): `FAILED: ordinary Public preview selection becomes active`. Run directly at `d0964fdd`: the same line after 47.61 s | With the conversion but POSIX-only fixture roots: `FAILED: application startup succeeds` in 0.09 s | Integrator's card. The storage fix moves it past application startup to Public preview selection. |

## Criteria

| Acceptance | Outcome |
|---|---|
| The tests-disabled image starts on an empty default root, shows the profile desktop window, quits cleanly, and a second launch reopens it without `STARTUP_FAILED` | Met, at `d587b5f1` |
| `application_profile_management_public_path` passes on `windows-dev`, including `native_empty_desktop_policy() == quit` | Met through ctest, at `d587b5f1` |
| The focused Windows contract (conversion; strict `/`, UNC and `\\?\` refusals; non-private registry file refused) | Met, at `d587b5f1` |
| The macOS and Linux storage courses that cover the changed sources still pass, and `architecture_boundaries` passes | Windows storage courses pass. macOS and Linux were not run on this station. `architecture_boundaries` fails at the base too (not introduced). |
