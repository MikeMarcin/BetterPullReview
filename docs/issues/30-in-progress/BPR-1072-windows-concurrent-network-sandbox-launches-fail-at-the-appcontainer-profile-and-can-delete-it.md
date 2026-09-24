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
- AC4 `test_network_isolation_manifest`, `test_network_isolation_external`,
  `test_network_isolation_child` and `test_network_isolation_loopback` pass on
  Windows, and the four rows of the first sighting,
  `qt_diff_axis_scale_{100,125,200}` and `qt_github_conversation_panel`, pass
  at `-j3` three times out of three.

## Evidence

Pending.
