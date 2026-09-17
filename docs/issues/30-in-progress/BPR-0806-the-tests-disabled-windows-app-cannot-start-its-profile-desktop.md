---
schema: "implement-rfc.card.v1"
id: "BPR-0806"
title: "The tests-disabled Windows app cannot start its profile desktop"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-win during task-20260917-win-evidence (BPR-0798, BPR-0730)","found_at":"f2b8e682255fc553a87d4d164c3b27f061300e07","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0806: The tests-disabled Windows app cannot start its profile desktop

## Defect

The `windows-app-release` image of `f2b8e682`, started on an empty data root,
prints `STARTUP_FAILED` and exits 1 within a second, before any window
appears. bpr-win traced it with throwaway diagnostics (all reverted; the
station tree was clean at `f2b8e682` afterwards):

1. **Forward-slash roots are handed to a validator that correctly refuses
   them.** Qt's `QStandardPaths::AppLocalDataLocation` returns
   `C:/Users/<user>/AppData/Local/BetterPullReview/BetterPullReview`.
   `native_application_data_root()` passes it through
   `native_filesystem_path()` (`src/app/src/github_application.cpp:477`),
   which converts the `QString` without native separators. The profile
   startup load then follows this chain:
   `resolve_profile_storage_root` → `validate_private_storage_root` →
   `canonical_windows_root` (`src/adapters/sqlite/src/catalog_database.cpp:2564`,
   which refuses any `L'/'`) → `invalid_path` → registry `unavailable` →
   `github_application_startup_failure::registry` → `STARTUP_FAILED`. The
   RFC-BPR-0024 public-path course has the same shape:
   `tests/end_to_end/profile_management_public_path_tests.cpp:117` roots
   itself at `QFileInfo{...}.canonicalFilePath()` and fails `legacy appearance
   store` on the same refusal. It never reaches its
   `native_empty_desktop_policy()` expectation.
2. **POSIX permission bits are checked on Windows.** With native separators
   applied locally as a trial, startup created
   `state/v1/profile-registry.sqlite3` and `profile-runtime.lock`, then failed
   on the next open. `sqlite_profile_registry_store::ensure_open` refuses an
   existing registry file when
   `symlink_status(path).permissions()` shows group or other bits, and MSVC's
   `std::filesystem` always reports `0666` or `0444`. The result is
   `profile_management_projection.Failure {op=0, reason=registry_unavailable}`
   and `STARTUP_FAILED` again.
3. **Not yet traced.** `validates_profile_runtime_lease` returns `false`
   unconditionally on `_WIN32`. Whether startup or a relaunch depends on it
   has not been established.

On native Windows, then, the RFC-BPR-0024 profile desktop has never started
from a tests-disabled image with the default root. This blocks [[BPR-0798]]
(both halves) and [[BPR-0730]].

## Scope and direction

- Normalize at the boundary where a Qt path becomes a host
  `std::filesystem::path` (native separators on Windows), both in `src/app`
  composition and in the test fixtures that build roots the same way. Keep
  `canonical_windows_root` strict. Do not teach the storage validator to
  accept `/`, and do not weaken its refusal of UNC, device, `\\?\`, reparse,
  relative, reserved-name or overlong paths.
- On Windows, replace POSIX permission-bit checks on private storage objects
  with the adapter's existing Windows private-object validation (owner-only
  security, no reparse point, single link, regular file), rather than
  skipping the check. The privacy the POSIX check expresses must still be
  enforced.
- Trace item 3. If Windows startup, relaunch or a second-process refusal
  needs a runtime lease that is unimplemented rather than defective, stop and
  report it as a design question. Do not stub it to `true`.
- Owned paths: `src/app/src/github_application.cpp` (the path conversion and
  root composition only), the sqlite profile registry and storage-root sources
  under `src/adapters/sqlite/src/`, the affected end-to-end fixtures, and a
  focused Windows contract for the conversion and the permission check.

## Acceptance

- The `windows-app-release` image starts on an empty default data root, shows
  the profile desktop window, and quits cleanly. A second launch on the same
  root reopens it without `STARTUP_FAILED`.
- `application_profile_management_public_path` passes on `windows-dev`,
  including its non-Apple `native_empty_desktop_policy() == quit`
  expectation.
- Focused Windows contract: a forward-slash Qt root is converted before
  storage admission, and `canonical_windows_root` still refuses `/`, UNC and
  `\\?\` inputs. A registry file with a non-private DACL is still refused.
- The macOS and Linux storage courses that cover the changed sources still
  pass, and `architecture_boundaries` passes.

## Evidence

Not run at card creation. The trace above comes from bpr-win's
2026-09-16/17 investigation at `f2b8e682`, which was reverted. The fix is
dispatched to bpr-win. The integrator finishes this card on `main` after
merging the station's commits and collecting its evidence file.
