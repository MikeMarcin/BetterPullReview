---
schema: "implement-rfc.card.v1"
id: "BPR-1231"
title: "Windows: application_private_review_native, _file and _retry cannot start the application (no Qt PATH), then refuse startup at the registry"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
---

# BPR-1231: Windows: application_private_review_native, _file and _retry cannot start the application (no Qt PATH), then refuse startup at the registry

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`application_private_review_native`, `application_private_review_file` and
`application_private_review_retry` fail on Windows: `Required regular
expression not found. Regex=[PRIVATE_REVIEW_PUBLIC_PATH_OK …]`.

Run directly, `tests/end_to_end/private_review_public_path.py` fails at its
`seed` phase with return code 3221225781 (`0xC0000135`,
STATUS_DLL_NOT_FOUND). The row gives the application no Qt `bin` directory on
`PATH`, as the other Windows application rows do. With Qt's `bin` prepended,
the seed runs and then refuses:

```
STARTUP_FAILED_CAUSE: startup_failure=registry
STARTUP_PROFILE_REFUSAL:registry_unavailable
STARTUP_FAILED
```

The three rows are also among the ones `test_network_isolation_manifest`
reports as not pinned ([[BPR-1237]]).

## Scope

- Give the rows the Windows `PATH` and fixture environment the other
  application public-path rows get.
- Find why the registry is unavailable. Likely, not measured: the course's
  data root lacks the owner-private descriptor the stores require (see the
  Windows private-root helpers of [[BPR-0876]] and [[BPR-1049]]).

## Acceptance

- The three rows pass on Windows, and print `PRIVATE_REVIEW_PUBLIC_PATH_OK`.
- macOS and Linux still pass.

## Fixed at `e175008d`, verified on Windows (bpr-main-win, 2026-09-30)

The Qt `PATH` part of the observation above came from a direct run. Under
`ctest` the rows already get Qt's `bin` on `PATH`. The failure was the
registry refusal alone.

`private_review_public_path.py` made its root with `tempfile` and
`mkdir(mode=0o700)`. On Windows that writes CPython's descriptor (SYSTEM,
Administrators, OWNER RIGHTS), which the profile registry refuses. The course
now uses `contained_temporary_directory` and `make_root_private`, as the other
Python courses do. POSIX is unchanged.

On Windows, `application_private_review_native`, `_file` and `_retry` pass:
23.89, 24.20 and 22.13 s.

Still open: a macOS and a Linux run. The three rows also remain on
[[BPR-1237]]'s list of unpinned rows. The card stays in Backlog until then.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

`application_private_review_native`, `_file` and `_retry` pass: 4.27, 4.50 and 4.01 s. They pass again after `4519a44f8`, which changes `private_review_store.cpp`. That is the Linux half of "macOS and Linux still pass".

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## macOS — red-test backlog milestone, 2026-09-30

macOS 26 (Darwin 25.6) arm64, Apple clang, Qt 6.11.2 offscreen, `build/dev` (Debug, `-Werror`) at `69493e235`, which contains the Windows fix.

`application_private_review_native`, `_file` and `_retry` pass through `ctest`: 28.30, 25.76 and 25.09 s (run three at a time). With the Linux run above, that is "macOS and Linux still pass". The three rows remain on [[BPR-1237]]'s unpinned list, which that card owns.
