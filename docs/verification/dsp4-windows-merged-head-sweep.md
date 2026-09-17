# dsp-004 Windows sweep at the merged head

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-merged-head-runs`. The head is
`2b2e5549d81c4f9ba92cd93d542b5e6064b68c2e`. Evidence only; no product, card or
manifest behaviour changed for it.

**Host.** Windows 11 Home 10.0.26200 (the app reports "Windows 11 Version
25H2"); Visual Studio 18 Community; Qt 6.10.3 `msvc2022_64`; Git for Windows
2.55.0.windows.3; display scale 150%; screen 2560x1440 logical.

**Tool versions, with how they resolve.**

- `clang-format` 22.1.8, from `PATH` at `C:\Program Files\LLVM\bin\clang-format.exe`.
  It is the only `clang-format` on `PATH`; no older copy shadows it.
- `python3` 3.14.7, resolving to
  `C:\Users\mikem\AppData\Local\Microsoft\WindowsApps\python3.exe`. CMake's
  `Python3::Interpreter` is a **different** interpreter,
  `C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64/python.exe`, which
  reports 3.14.6, and that is what every registered Python row runs.

**Build.** `cmake --preset windows-dev` and a full Debug build: **warning-clean,
no errors**. That includes [[BPR-0862]]'s two non-whitespace moves
(`server_card.hpp` in `workbench_window.hpp`, `<QPushButton>` in
`review_submission_composition_public_path.cpp`); MSVC accepts both include
blocks as self-contained.

## The sweep

`ctest --preset windows-dev --output-on-failure --timeout 900`, serial, one
process. **362 rows, 309 passed, 53 failed, 3379.95 s.** Every failure is below
with its first failing line, grouped by measured cause.

### A. Contained rows that must launch a child process (11 rows)

An AppContainer here cannot launch `git.exe` or another interpreter, and git
cannot read a working directory it is handed: measured directly,
`git rev-parse --show-toplevel` in the container exits 128 with
`fatal: Unable to read current working directory: Permission denied`.

| Row | First failing line |
| --- | --- |
| `issue_id_allocation_contract` | `PermissionError: [WinError 5] Access is denied` |
| `github_recorded_fixture_contract` | `PermissionError: [WinError 5] Access is denied` |
| `large_review_fixture_contract` | `PermissionError: [WinError 5] Access is denied: 'C:\Program Files\Git\cmd\git.exe'` |
| `attended_review_verdict_contract` | `PermissionError: [WinError 5] Access is denied` |
| `project_source_dump_contract` | `PermissionError: [WinError 5] Access is denied` |
| `dif_assets_contract` | `dif_assets.AssetError: generated directory is absent: generated` |
| `rfc0017_public_preview_decision_contract` | `FileNotFoundError: ...\AC\Temp\...\repository\docs\verification\reviews\...` (42 errors) |
| `test_network_isolation_child` | `PermissionError: [WinError 5] Access is denied` |
| `rfc0016_relocated_operator_contract` | `CalledProcessError: ['git', '-C', '...AC\Temp\...', 'init', '--quiet']` exit 128 |
| `rfc0016_production_identity_security` | `CalledProcessError: ['git', 'init', '--quiet']` exit 128 |
| `application_agent_finding_protocol` | `Access is denied.` then `FAILED: the canary fixture exits cleanly` |

### B. Two rows that need board content the repository does not carry (2 rows)

`documentation_structure` and `documentation_issue_graph_contract` both end at

```text
ModuleNotFoundError: No module named 'board_model'
```

`scripts/verify_documentation.py:38` inserts
`.agents/skills/implement-rfc/scripts` and imports `board_model` from it. On
this station `.agents/skills/implement-rfc` is a **symlink out of the
repository** into `C:\Users\mikem\.mask\store\v1\<digest>\payload\implement-rfc`,
and `.agents/` is untracked and git-ignored. The store's payload grants no
AppContainer entry, so a contained row is denied it: measured, `listdir` of
that scripts directory inside the container fails `[WinError 5]`.

Granting the container read on that one store payload was measured as the
cause: the import then succeeds and the row advances to a **second** contained
obstacle, `project_config.ProjectConfigError: not a Git consumer repository:
C:\code\git\bpr-windows`, which is the `git rev-parse` of group A. The grant was
reverted, so the station stays representative.

Both rows pass **uncontained** (`DOCUMENTATION_OK` directly). So on Windows
these rows depend on skill content outside the repository plus a git child, and
neither is reachable from inside the container.

### C. Application startup refuses with `registry_unavailable` (5 rows)

| Row | First failing line |
| --- | --- |
| `application_large_review_benchmark` | `large-review-benchmark-failed: application startup succeeds` |
| `application_unavailable_diff_public_path` | same |
| `application_approximate_diff_public_path` | same |
| `application_review_ledger_composition_contract` | `review-ledger seed process failed (1)`, `STARTUP_FAILED_CAUSE: startup_failure=registry` |
| `application_defect_composition_contract` | `STARTUP_FAILED`, `startup_failure=registry` |

Measured with a temporary print at `github_application.cpp` ~12623, reverted:
the underlying value is `profile_startup_failure` **2 = `registry_unavailable`**.
`github_application_startup_failure::registry` is the catch-all arm of that
mapping, so the reported cause hides which profile-startup failure fired; a
named arm would have saved this measurement.

It is **not** a containment effect: the benchmark fails identically contained
and uncontained, and the two composition rows are registered
`WINDOWS_UNCONTAINED`.

**[[BPR-0856]] on Windows, measured.** The benchmark's Windows branch keeps the
bare `QTemporaryDir` as the data root, and that is the root the registry
refuses. With the `#if !defined(_WIN32)` removed, so Windows also gets the
owner-private parent and the `data` child, the row **composes**
(`UNAVAILABLE_DIFF_COMPOSED_OK`) uncontained. Contained, that same arrangement
stops earlier at `large-review-benchmark-failed: private data root parent`,
which is the narrowing an AppContainer cannot perform. So Windows needs a third
arrangement, not the bare `QTemporaryDir`: the row is affected, not unaffected.

### D. Rows whose only failing fact is the shutdown cleanup (12 rows)

Every one ends `PUBLIC_PATH_FAILED` with
`PUBLIC_PATH_VERDICT:... cleanup_failed=1`. **Eleven of the twelve carry
`presentation=1`**, so everything the row measures passed and only the cleanup
refused; the twelfth is `application_uiforetw_selected_diff_contract`
(`presentation=0`, below).

`application_uiforetw_selected_diff_contract`,
`application_workbench_composition_contract`,
`application_review_round_trip_composition_contract`,
`application_overview_since_review_contract`,
`application_review_behaviour_contract`, `application_saved_server_contract`,
`application_appearance_theme_customization_contract`,
`application_epoch_persistence_github-com`,
`application_epoch_persistence_ghes-3_18`,
`application_epoch_maintenance_public_path`,
`application_appearance_gesture_contract`,
`application_appearance_store_fallback_contract`.

`ShutdownCleanupFailed` has three sources
(`github_application.cpp` ~11762-11812): the durable window layout's write, the
window-layout erase, and `std::filesystem::remove_all(McpSnapshotRoot)`. Which
one fires is not printed, and a named one would have saved a measurement here
too. `application_native_notification_attended_contract` ends the same way,
`NATIVE_NOTIFICATION_REFUSED:shutdown_cleanup_failed`, with every other marker
(`NATIVE_NOTIFICATION_DISPLAY_BOUNDARY_OK`, `..._QT_ACTIVE_WINDOW_OK`,
`..._TRAY_SIGNAL_OK`, `..._ACTIVATION_OK`, `..._EXACT_LANDING_SELECTION_OK`) OK.

### E. [[BPR-0854]] item 1's five contained courses (4 rows red, 1 passes)

| Row | First failing line |
| --- | --- |
| `application_device_binding_relaunch` | `os.chmod(root, 0o700)` -> `PermissionError: [WinError 5] Access is denied: '...\AC\Temp\bpr-0644-relaunch-612768de307ca452\application'` |
| `application_profile_owner_admission_public_path` | `FAILED: seed registry`, then the seed child `CalledProcessError` |
| `application_profile_window_restart_public_path` | `seed failed (3)` |
| `application_profile_window_layout_public_path` | `seed failed (3)` |
| `application_profile_window_review_restoration_public_path` | `auth-seed failed (3)` |

`application_device_binding_launch_renewal` also fails, at
`FAILED: private desktop test root under the canonical storage policy`.

**The helper itself is sound on Windows.** Measured inside the container: a
root created the helper's way (`os.mkdir` with no mode under
`tempfile.gettempdir()`) is created, a child directory inside it is created, a
file in it is written, and `icacls` shows the container's own SID inherited
`(I)(OI)(CI)(CR)(F)`. So the contained driver **can** use the directory it just
created, which is the question the card asked.

What fails is around it: `os.chmod(<dir>, 0o700)` is denied inside the
container, and the seeding **executables** exit 3, which is the abort this
station has measured before for `std::filesystem::canonical` under the
container ([[BPR-0833]], [[BPR-0845]], [[BPR-0846]]'s courses).

**[[BPR-0868]]'s leak, seen once.** After the sweep exactly one course
directory is left behind:
`C:\Users\mikem\AppData\Local\Packages\bprautomatedtest\AC\Temp\bpr-0644-relaunch-612768de307ca452`,
from the row that died at `os.chmod`. Nothing reported it, which is the
swallowed-removal behaviour the card predicts. No leftovers remain for the
other four prefixes.

### F. Carded already

- `qt_composed_github_scale_100`, `_125`, `_200`: `FAILED: the code is in a
  fixed-pitch font at 1.6 times the row's size` — [[BPR-0869]].
- `qt_syntax_pane_composition`: `FAILED: the line that opened the block comment
  has scrolled off the top, so nothing on screen says the marker line is a
  comment` — [[BPR-0870]].

### G. The remaining eight, each its own cause

| Row | First failing line |
| --- | --- |
| `rfc0017_release_absence_negative_control` | `rfc0017-test-inventory:fail:string present in release binary: --network-denied` |
| `sqlite_epoch_catalog_behavior_contract` | `FAIL: a copy of the previous file migrates to v6` (a copied store file again, as in [[BPR-0853]]) |
| `github_cli_process_runner` | `FAILED: opt-in preserves only exact bounded stdout and completed exit code` |
| `live_review_submission_restart_recovery` | `RuntimeError: --kill-prepared exited before checkpoint (1)`, `FAILED: submission store opens` |
| `epoch_kill_recovery` | `RuntimeError: fetch: recovery refs do not contain one exact base/head pair` |
| `epoch_transaction_kill_recovery` | `RuntimeError: receiving: recovery refs do not contain one exact base/head pair` |
| `rfc0016_production_course_security` | `AssertionError: Lists differ: ['outcomes\authorization_pending.json', ...] != ['outcomes/authorization_pending.json', ...]` — a backslash/forward-slash expectation |
| `application_review_submission_composition_public_path` | the row fails in 0.06 s with no output before the missing `review-submission-composition` marker |

The four device-authorization rows fail on a marker the run never printed:
`application_device_authorization_registered_course` and `..._refresh_course`
want `DEVICE_FLOW_STAGE:authorized`, `..._panel_course` and `..._panel_absent`
want `DEVICE_FLOW_ENROLLMENTS:1`. Their runs print the appearance and
notification markers instead, and no `DEVICE_FLOW_CLEANUP_FAILED:true` appears
anywhere in the sweep.

## The rows this task named

| Row | Result |
| --- | --- |
| `sqlite_anonymous_epoch_authority` ([[BPR-0857]]) | Pass, 0.91 s |
| `sqlite_epoch_catalog_storage_contract` ([[BPR-0857]]) | Pass, 1.22 s |
| `application_unavailable_diff_public_path` ([[BPR-0858]]) | Fail, group C |
| `application_approximate_diff_public_path` ([[BPR-0858]]) | Fail, group C |
| `application_native_notification_attended_contract` ([[BPR-0861]]) | Fail, cleanup only |
| `application_workbench_composition_contract` ([[BPR-0860]]) | Fail, cleanup only — **the drawer-width wire is fixed**: `WORKBENCH_DRAWER_WIDTH_WIRE_OK`, every other `WORKBENCH_*` marker OK, `presentation=1` |
| `application_uiforetw_selected_diff_contract` ([[BPR-0859]]) | Fail: `UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED`, `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`, `presentation=0`, `cleanup_failed=1`. No `UIFORETW_NAVIGATION_ROW_UNEXPECTED` was printed, so the platform-aware strip row held |
| `application_large_review_benchmark` ([[BPR-0856]]) | Fail, group C; the Windows branch is affected |
| `msvc_secure_crt_contract` ([[BPR-0854]] item 2) | Pass |
| `contained_temporary_usage_contract` ([[BPR-0854]] item 1) | Pass |
| `application_windows_title_row` ([[BPR-0653]]) | Pass, 2.35 s |
| `qt_workbench_window_scale_100` / `_125` / `_200` | Pass individually (105.0 s, 102.8 s, 94.3 s) |
| `test_network_isolation_manifest` | Pass, 6.40 s |

`splitter_widgets` under the real Windows style was not reported: BPR-0860's
three named stall terms never fired, because the row reached its end and failed
only on the cleanup.

### A timeout margin worth raising

In one sequential batch `qt_workbench_window_scale_100` **hit its 120 s
TIMEOUT** at 120.03 s, and `qt_workbench_window_scale_200` failed once at
`FAILED: sized publication completes its native measurement before scroll
evidence`; both then passed alone at 105 s and 94 s. These rows took about 83 s
before this run's merges, so the 120 s limit now has roughly 15% of headroom on
this station.

## [[BPR-0653]]'s zoom row on the real platform

The repaired row takes the **double-click path** on real Windows, not the
sub-floor one, and passes: `strip=2560x30`, spacer `379,4 692x22`,
`window=2560`, `floor=902`, screen 2560x1440, platform `windows`.

Offscreen on this station it also takes the double-click path at every scale
(1.0: `strip=796`; 1.25: `strip=637`; 2.0: `strip=398` with the spacer at
`x=331` width 47, so the point lands inside). With `layout()->activate()` in
place the sub-floor branch does not fire here at any scale, so on this station
it is unexercised rather than wrong.
