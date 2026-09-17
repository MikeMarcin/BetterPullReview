# BPR-0806 Windows profile storage startup evidence

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-storage`, run `run-20260916-stations`, dispatched by
`bpr-main-mac`. Baseline `f2b8e682255fc553a87d4d164c3b27f061300e07`; the
producing revision of everything below is
`d0964fdd4a653f7ab7ea27757eeaedce2378a0e8`, which sits on `32a983e6` ([[BPR-0805]],
`/bigobj`) and `f2b8e682` ([[BPR-0804]]). Toolchain: Visual Studio 18 Community,
MSVC 14.51.36231, Qt 6.10.3 `msvc2022_64`, Git for Windows perl appended to
`PATH`. Smart App Control off (both values `0`).

## The defect as found

At `f2b8e682` the `windows-app-release` image printed `STARTUP_FAILED` and
exited 1 about one second after launch on an empty default data root, before
any window. The trace (throwaway diagnostics, reverted before any commit) is on
the card: Qt's `C:/Users/...` spelling reached `canonical_windows_root`, which
refuses `/`; with the separators converted, the profile registry then refused
its own existing file on POSIX group and other permission bits, which MSVC's
`std::filesystem` always reports. `application_profile_management_public_path`
failed in 0.30 s at `FAILED: legacy appearance store` for the first reason.

## What changed

- **The path conversion happens where a path leaves Qt.** The new
  `bpr/app/native_filesystem_path.hpp` converts separators with
  `QDir::toNativeSeparators` on `_WIN32`. It replaces the composition's and
  `main.cpp`'s private copies, and the composition's and `main`'s temporary
  roots use it. So do the end-to-end fixtures that built roots from
  `QFileInfo{}.canonicalFilePath()`. `canonical_windows_root` is unchanged.
- **Existing store files are admitted on the platform's own privacy rules.**
  `detail::admit_private_store_file` and `detail::restrict_private_store_file`
  replace the permission-bit check in the profile registry, window restoration
  and device binding stores.
  - POSIX keeps the previous check.
  - On Windows the helpers work from the retained `state/v1` handles and call
    `open_private_object(regular_file, WRITE_DAC, protect_inherited)`. That
    requires one link, no reparse point and a matching handle path, and it
    requires owner-only security. The single inherited grant on a file sqlite
    created is made explicit, and anything wider is refused without repair.
- **The Windows lease stub is not on the startup path.**
  `validates_profile_runtime_lease` returns `false` on `_WIN32`, but its only
  callers are the POSIX activation server (`profile_activation.cpp`), and
  `make_profile_activation_server` is unavailable on Windows. Startup, relaunch
  and second-process refusal use the real lease: `profile-runtime.lock`, opened
  with share mode 0 and a strict DACL. The stub means only that Windows has no
  single-instance activation handoff.
- **Fixture roots that were private only on POSIX.** They called
  `std::filesystem::permissions(owner_all)`, which does nothing on Windows. They
  now call `make_storage_root_owner_private`. This covers the sqlite store
  courses for the device binding store, the profile registry and window
  restoration, and the Windows-registered profile public paths.

## Native tests-disabled image

`cmake --preset windows-app-release` and `cmake --build --preset
windows-app-release` from a `vcvars64` shell produce
`build/windows-app-release/src/app/better_pull_review.exe`, SHA-256
`f249fad47843e2a750af33664c4890296fbe1e58863bc5b7458e6b111f552ca0`. The image
was launched directly on the interactive desktop with the Qt runtime directory
prepended to its own `PATH`. It ran on an empty default root,
`%LOCALAPPDATA%\BetterPullReview\BetterPullReview`. The owner's earlier root was
moved aside for these runs, to be restored before the station stops. The window was closed with UI
Automation `WindowPattern.Close`, which is the caption close, never a process
kill.

| Step | Result |
|---|---|
| First launch, empty root | `BetterPullReview — Default profile` window 1.3 s after launch |
| Close the only window | Exit code 0, 55 ms after the close. No other window appeared (the only other visible top-level window was the `PseudoConsoleWindow` that comes with redirected stdout). |
| Relaunch on the same root | Window after 0.8 s, no `STARTUP_FAILED`. Close gives exit code 0 after 51 ms. |
| Window identity (`profile_registry_windows`, `profile_registry_meta.last_window_id`) | Launch 1 holds window 1, and its row is gone after the close. The relaunch holds window 2 (`last_window_id` 1 then 2). |
| Second process while the first runs | stdout `PROFILE_ALREADY_OPEN`, exit 1 after about 1 s. The first keeps its window and exits 0 when closed. |
| DACLs (`icacls`) | `profile-runtime.lock`, `profile-registry.sqlite3`, `window-restoration.sqlite3` and `device-bindings.sqlite3` show `JAEGAW\mikem:(F)`, protected. `state\v1` shows `JAEGAW\mikem:(OI)(CI)(F)`. |

## Focused contract

`application_windows_profile_storage_startup_contract` is registered on `WIN32`
only. The executable run directly passes in 0.08 s. It checks the following:

- A Qt forward-slash root becomes the native spelling.
- `validate_private_storage_root` still refuses `/`, a UNC spelling
  (`\\localhost\C$\...`), a `\\?\` spelling and a relative root.
- `resolve_profile_storage_root` refuses the unconverted root and resolves the
  converted one.
- A fresh registry is created, and its file is admitted on the second and third
  opens.
- A second hard link is refused, and the file is admitted again once the link
  is removed.
- A file whose DACL grants Everyone read is refused, and still refused on a
  second attempt, so the refusal does not repair it.

Two local trials, both reverted, show the contract fails without the fix:

| Trial | Result |
|---|---|
| The header returns the Qt spelling unconverted | `FAILED: a Qt forward-slash root becomes the native Windows spelling` |
| The registry store restores the POSIX permission-bit check | `FAILED: a second open admits the registry file it created` |

## Courses on `windows-dev`

The `windows-dev` Debug targets were built from a `vcvars64` shell. The
executables were run directly with `QT_QPA_PLATFORM=offscreen`, outside the
AppContainer. The reason is in the ctest section below.

| Course | Result at `d0964fdd` |
|---|---|
| `application_windows_profile_storage_startup_contract` | pass, 0.08 s |
| `application_profile_startup_public_path` | pass, 8.84 s |
| `sqlite_profile_registry_storage` | pass, 1.49 s |
| `sqlite_profile_storage_root` | pass, 0.06 s |
| `sqlite_window_review_restoration_storage` | pass, 1.78 s |
| `sqlite_device_binding_storage` | pass, 0.78 s |
| `sqlite_profile_runtime_lease` | pass, 0.04 s |
| `application_profile_management_public_path` | **fail**, 5.26 s: `FAILED: each concurrent profile retains its own startup and shutdown diagnostics` |
| `application_profile_multi_window_public_path` | **fail**, 30.45 s: MSVC `optional(429)` assertion, `operator->() called on empty optional`. Not investigated. |
| `application_device_binding_restart` | **fail**, 47.61 s: `FAILED: ordinary Public preview selection becomes active`. Not investigated. |

**The management course and the missing Windows log sink.** The management
course fails only at its diagnostics-log check. That check needs
`<root>/logs/diagnostics.log`, and the Windows file sink does not exist:
`quill_log.cpp` opens the log only under `#if !defined(_WIN32)`, as recorded by
[[BPR-0701]]. A local trial, reverted, skipped only that loop on Windows. The
whole course then passed in 22.4 s, including the non-Apple
`native_empty_desktop_policy()() == quit` expectation and the B1 quit-policy
course. The integrator is asked whether to gate that check or to implement the
sink first. Until a ruling, this criterion is **not met**.

**The ctest registrations.** Under ctest the contained registrations of these
Windows profile and sqlite store courses die at launch inside the AppContainer.
They exit with code 3 and no output, or `make_storage_root_owner_private`
refuses the temporary root. This is consistent with [[BPR-0362]]'s lowbox
limits. The new contract behaves the same way. Only the `WINDOWS_UNCONTAINED`
registrations pass through ctest: `sqlite_device_binding_storage` and
`sqlite_profile_runtime_lease`. Moving the others would mean pinning them in the
network-isolation manifest. That manifest gate is already red at this head for
tests this change does not touch, so the choice is left to the integrator.

**`architecture_boundaries`.** It fails identically at `f2b8e682` (run against
a `git archive` of that revision) and at `d0964fdd`: `Provider data crossed the
core/Qt boundary in src/adapters/qt/src/workbench_window.cpp: https?://`. That
failure is pre-existing. Because the script stops at its first failure, it does
not check the files this change touched.

## Remaining

The card's own criteria stand as follows:

- The release image starts, quits and relaunches: **met**.
- The focused contract: **met**.
- The courses that cover the changed sources pass on this host when run directly
  (macOS and Linux not run here): **met**, except that
  `application_profile_management_public_path` is **not met** because of the
  Windows diagnostics sink.
- `architecture_boundaries`: **not met**, pre-existing and unrelated.
