---
schema: "implement-rfc.card.v1"
id: "BPR-1233"
title: "Windows: live_review_submission_restart_recovery fails at its first kill boundary because the submission store does not open"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
acceptance: [{"criterion":"The row passes on Windows","outcome":"pass","revision":"c52aa0e8","evidence":{"kind":"observation","detail":"bpr-main-win: live_review_submission_restart_recovery passes, 1.08 s, all five kill boundaries recover; recorded on the card."}},{"criterion":"macOS and Linux still pass","outcome":"pass","revision":"69493e235","evidence":{"kind":"command","detail":"Linux (bpr-main-linux, 69493e235): passes, 0.20 s. macOS arm64 (69493e235): passes through ctest, 0.58 s. Recorded on the card."}}]
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

## Fixed at `c52aa0e8`, verified on Windows (bpr-main-win, 2026-09-30)

The cause is [[BPR-1231]]'s. The course made its root, and each boundary's
directory, with `mkdir(mode=0o700)`, and on Windows the submission store
refuses that descriptor. `make_root_private` now narrows them. The contained
temporary root itself is narrowed too, because the unresolved phases write in
it directly.

On Windows, `live_review_submission_restart_recovery` passes, 1.08 s. All five
kill boundaries recover (`prepared`, `witnessed`, `published`, `resolved` and
`published-moved`). Then the unresolved, moved and prepared phases pass.

Still open: a macOS and a Linux run. The card stays in Backlog until then.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

`live_review_submission_restart_recovery` passes, 0.20 s. That is the Linux half of "macOS and Linux still pass".

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## macOS — red-test backlog milestone, 2026-09-30

macOS 26 (Darwin 25.6) arm64, Apple clang, Qt 6.11.2 offscreen, `build/dev` (Debug, `-Werror`) at `69493e235`, which contains the Windows fix.

`live_review_submission_restart_recovery` passes through `ctest`, 0.58 s. With the Linux run above, that is "macOS and Linux still pass".
