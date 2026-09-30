---
schema: "implement-rfc.card.v1"
id: "BPR-1231"
title: "Windows: application_private_review_native, _file and _retry cannot start the application (no Qt PATH), then refuse startup at the registry"
status: "Backlog"
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
