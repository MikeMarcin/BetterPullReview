---
schema: "implement-rfc.card.v1"
id: "BPR-1233"
title: "Windows: live_review_submission_restart_recovery fails at its first kill boundary because the submission store does not open"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
---

# BPR-1233: Windows: live_review_submission_restart_recovery fails at its first kill boundary because the submission store does not open

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`live_review_submission_restart_recovery` fails on Windows in 0.13 s:

```
RuntimeError: --kill-prepared exited before checkpoint (1):
FAILED: submission store opens
```

`tests/scripts/review_submission_restart_recovery_tests.py` runs the provider
executable at `--kill-prepared`, and it fails its first expectation,
`submission store opens` (`tests/github_contract/live_review_submission_provider_tests.cpp:380`).
The row is already pinned `WINDOWS_UNCONTAINED` (`dos-volume-name`), so this is
not the AppContainer. The store sources changed on 2026-09-26 (`a6513264`,
`d9ce7daf`).

## Scope

- Find why the store refuses to open, uncontained. Likely, not measured: the
  course root is made by the Python driver without the owner-private
  descriptor the stores require (see `copy_private_root` and
  `make_root_private` in `tests/end_to_end/bpr_narrow_owner_private.py`,
  [[BPR-0876]]).

## Acceptance

- The row passes on Windows.
- macOS and Linux still pass.
