---
schema: "implement-rfc.card.v1"
id: "BPR-1072"
title: "Windows: concurrent network-sandbox launches fail at the AppContainer profile, and can delete it, because every launch calls CreateAppContainerProfile and only ERROR_ALREADY_EXISTS counts as an existing profile"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, mac batch 3 check (qt_diff_axis_scale_* at -j3)","found_at":"90ef83f0","measured_at":"b5cb3b43","related_cards":["BPR-0347","BPR-0872","BPR-0890"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1072--BPR-1074--bd67ade43931.json"}
---

# BPR-1072: Windows: concurrent network-sandbox launches fail at the AppContainer profile, and can delete it, because every launch calls CreateAppContainerProfile and only ERROR_ALREADY_EXISTS counts as an existing profile

## Defect

Every contained Windows row starts through
`better_pull_review_network_sandbox.exe`
(`tests/network_isolation/windows_network_sandbox.cpp`). Before it launches
the row, the launcher calls `ensure_container_sid()`, which calls
`CreateAppContainerProfile` for the one fixed container, `BprAutomatedTest`.
It treats `S_OK` as created and `HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)` as
already there. Anything else ends the launch with exit 125 and
`BPR_NETWORK_ISOLATION_UNAVAILABLE:profile:hr=…`.

The profile already exists by the time any row runs: configure time creates it
through the launcher's `--bpr-appcontainer-setup` query. So every launch asks
the system to create a profile that exists, and under `ctest -j` several
launches ask at once.

## Seen (bpr-main-win, 2026-09-23, `90ef83f0`)

`ctest --preset dev -j3 -R '^(qt_diff_axis_scale_(100|125|200)|qt_github_conversation_panel)$'`
failed one row in 2 of 4 runs, each time with
`BPR_NETWORK_ISOLATION_UNAVAILABLE:profile:hr=0x8000ffff` (`E_UNEXPECTED`),
once after 0.02 s and once after 0.71 s. The same row passed 3 of 3 alone.

## Measured (bpr-main-win, 2026-09-24, `b5cb3b43`)

A station helper (not committed) started K launchers at the same moment, each
running `cmd.exe /d /c exit 0` in the container, for R rounds. Logs are
station records under `build/logs/bpr-1072-main-win/`, not committed.

| K × R | Exit 0 | Exit 125 | Lines |
| --- | --- | --- | --- |
| 1 × 3 | 3 | 0 | none |
| 4 × 25 | 13 | 87 | 32 `launch:win32=2`, 29 `profile:hr=0x800703fa`, 14 `profile:hr=0x8007000a`, 12 `profile:hr=0x8000ffff` |

`0x800703fa` is `ERROR_KEY_DELETED`, `0x8007000a` is `ERROR_BAD_ENVIRONMENT`,
and `launch:win32=2` is `CreateProcessW` failing with `ERROR_FILE_NOT_FOUND`
after the profile step had passed. After the 4 × 25 run the profile was gone:
`GetAppContainerFolderPath` for the container's SID returned `0x80070002`, and
its mapping under
`HKCU\Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppContainer\Mappings`
was absent. One more single launch created it again.

So `CreateAppContainerProfile` on a profile that exists is not a read. Calls
that overlap undo each other's work, and they can remove the profile under a
launch that is already past its own profile step. `E_UNEXPECTED` is one of
four ways that shows.

## Scope

- Stop calling `CreateAppContainerProfile` for a profile that exists. Check
  that the profile exists without changing it, and create it only when it is
  missing. Serialize creation across launchers, so two launchers that both
  find it missing do not race.
- Keep the boundary's security: the launch still enters the zero-capability
  `BprAutomatedTest` container, and any outcome other than "exists" or
  "created" still refuses the launch with exit 125 and runs nothing.
- Add a Windows row that starts several launchers at once for several rounds
  and requires every child to start inside the container. It must fail
  against the current `ensure_container_sid`.

## Acceptance

- AC1 The failure is measured and its cause named, with the codes and what
  happens to the profile.
- AC2 Concurrent launches no longer fail at the profile. A Windows row starts
  several launchers at once for several rounds, and every child reports that
  it runs inside the AppContainer. The row fails against the current
  `ensure_container_sid` (negative control).
- AC3 The launcher still fails closed. A profile check or creation that
  fails for any reason other than an absent profile refuses the launch with
  exit 125 and a named stage, and runs nothing. This is shown on at least one
  induced genuine failure.
- AC4 `test_network_isolation_manifest`, `test_network_isolation_external`
  and `test_network_isolation_loopback` pass on Windows, and the four rows of
  the first sighting, `qt_diff_axis_scale_{100,125,200}` and
  `qt_github_conversation_panel`, pass at `-j3` three times out of three.

*Amended while working (bpr-main-win, 2026-09-24):* AC4 first named
`test_network_isolation_child` too. That row fails on Windows before this
card and for another reason. A contained Python cannot start a child
(`PermissionError: [WinError 5]`), and [[BPR-0872]] left the row contained on
purpose. Its Windows expectation is [[BPR-0903]]'s, in Backlog. It failed the
same way before and after this change, so it is out of this card's
acceptance.

## Evidence

### Fixed in the launcher (bpr-main-win, 2026-09-24)

`ensure_container_sid()` no longer asks the system to create a profile that
exists. Under one session-wide named mutex,
`Local\BprAutomatedTestAppContainerProfile`, every launcher does three things:

1. It derives the container SID from the name.
2. It reads whether the profile exists with `GetAppContainerFolderPath`. That
   call answers only for a registered profile, and it reports
   `ERROR_FILE_NOT_FOUND` for one that is absent.
3. It creates the profile only when it is absent.

"Exists", "created" and, for a launcher built before the lock,
`ERROR_ALREADY_EXISTS` pass. Everything else refuses the launch with exit 125
and a named stage:

- the lock cannot be made, or is not taken within 30 s: `profile-lock`;
- the existence check gives any other answer: `profile-check`;
- creation fails: `profile`.

The lock is held only for the check and at most one creation, not across
`CreateProcessW`. The container is the same zero-capability
`BprAutomatedTest`, entered exactly as before. `GetAppContainerFolderPath`
returns memory freed with `CoTaskMemFree`, so the launcher, and the
configure-time `--bpr-appcontainer-setup` query that compiles the same source,
now also link `ole32`. A scratch CMake project that calls
`bpr_windows_appcontainer_sid` on the new source configures and prints the
same SID, `S-1-15-2-2501375856-…-2136715157`.

**The row (AC2)** is `test_network_isolation_concurrent_launch`, Windows only.
It starts four launchers at the same moment for eight rounds. Each launcher
runs `network_isolation_counterexamples.py contained`, which prints
`NETWORK_ISOLATION_CONTAINED:appcontainer` only if its token is an
AppContainer token. The row passes only on
`NETWORK_ISOLATION_CONCURRENT_LAUNCHES_CONTAINED:32`. Otherwise it names each
failed launch, its exit code and its refusal line.

The row runs the launcher, so it cannot itself be contained. Registered
contained, every launch stopped at its first step with
`BPR_NETWORK_ISOLATION_UNAVAILABLE:firewall-service:win32=5`: the Service
Control Manager refuses a lowbox token. So the row is `WINDOWS_UNCONTAINED`,
pinned in `scripts/verify_test_network_isolation.py` under a new measured
cause, `service-manager-denied`. The structure contract's set of causes names
it too.

| Launcher | The row | The station's stress helper |
| --- | --- | --- |
| Before (`ensure_container_sid` as on main at `c920c776`) | **fails**: `NETWORK_ISOLATION_CONCURRENT_LAUNCH_FAILED:18/32`, with 8 × `launch:win32=2`, 4 × `profile:hr=0x800703fa`, 3 × `profile:hr=0x8000ffff`, 2 × `profile:hr=0x8007000a`, 1 × `launch:win32=31` | 4 × 25: 87 of 100 refused |
| After | passes, 32 of 32 contained, 0.93–0.98 s | 4 × 25: 100 of 100; 8 × 25: 200 of 200 |

**Fails closed (AC3).** A station helper (not committed) squatted the lock two
ways and ran one launch with the `contained` child each time:

| Induced failure | Launcher | Child |
| --- | --- | --- |
| The lock's mutex exists with a DACL that denies everyone | exit 125 at once, `BPR_NETWORK_ISOLATION_UNAVAILABLE:profile-lock:win32=5` | did not run |
| Another process owns the lock | exit 125 after 30.0 s, `BPR_NETWORK_ISOLATION_UNAVAILABLE:profile-lock:win32=258` | did not run |
| Lock released (control) | exit 0 | `NETWORK_ISOLATION_CONTAINED:appcontainer` |

**AC4**, `build/dev`, MSVC 19.51 Debug:

- serial: `test_network_isolation_structure_contract`,
  `test_network_isolation_manifest`, `test_network_isolation_external`,
  `test_network_isolation_loopback` and
  `test_network_isolation_concurrent_launch` pass, 5 of 5;
- `-j3`, `qt_diff_axis_scale_{100,125,200}` and
  `qt_github_conversation_panel`: 4 of 4, three times out of three.

`clang-format` reports the launcher clean. `clang-tidy` reports nothing in
the new lines. It does report three older findings in the file, all
`readability-identifier-naming`: `no_status` (`3c8f25c0`) and `wmain`'s
`argc` and `argv` (`7ecd8e5f`). Logs are station records under
`build/logs/bpr-1072-main-win/` and are not committed.
