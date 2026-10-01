---
schema: "implement-rfc.card.v1"
id: "BPR-1234"
title: "Windows: application_profile_activation_public_path passes two empty arguments when app_bpr is not built, and its script refuses them"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
---

# BPR-1234: Windows: application_profile_activation_public_path passes two empty arguments when app_bpr is not built, and its script refuses them

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`application_profile_activation_public_path` fails on Windows in 0.09 s:

```
usage: profile_activation_contract.py [-h] --application APPLICATION
                                      --desktop-application DESKTOP_APPLICATION
                                      [--tour-application TOUR_APPLICATION]
profile_activation_contract.py: error: unrecognized arguments:
```

The registration (`tests/end_to_end/CMakeLists.txt`, about line 3079) ends with
`$<$<TARGET_EXISTS:app_bpr>:--tour-application>` and
`$<$<TARGET_EXISTS:app_bpr>:$<TARGET_FILE:app_bpr>>`. When `app_bpr` is not a
target, as in this Windows tree, each expands to an empty string. CTest passes
both as empty arguments, and `argparse` rejects them.

## Scope

- Pass the pair only when the target exists, for example as one list
  generator expression with `COMMAND_EXPAND_LISTS`. Nothing in the script needs
  to change.
- Then run the row on Windows. It is pinned `WINDOWS_UNCONTAINED` without a
  Windows measurement ([[BPR-0979]]'s note in
  `scripts/verify_test_network_isolation.py`). Confirm or correct that pin.

## Acceptance

- The row passes on Windows, with and without `app_bpr`.
- macOS and Linux still pass.

## macOS observation, 2026-09-29

The same failure reproduces on macOS arm64 in a dev build configured with
`BPR_BUILD_TOUR_CLI=OFF` (Review Cycles authoring milestone, `main` at
`95a30c5b0`):
`ctest --test-dir build/dev -R "^application_profile_activation_public_path$" --output-on-failure`
fails in 0.1 s with the same `unrecognized arguments:` refusal, and it failed
identically at `8374ea995`. The cause is not Windows-specific, so the macOS
row does not "still pass" without `app_bpr`; the fix above applies to every
platform. (The authoring milestone briefly filed this as a duplicate before
this card reached `main`; it was withdrawn.)

## Fixed at `ab9d3fd7`, verified on Windows (bpr-main-win, 2026-09-29)

The pair now comes from `bpr_activation_tour_arguments`, set only under
`if(TARGET app_bpr)`. `src/app` defines `app_bpr` before `tests` is
configured, so the check is reliable. The script did not change. On
Windows, where `app_bpr` is not built, the row passes: 17.19 s.

Still open: a run on macOS or Linux, where `app_bpr` exists, to show the
pair is still passed. And the row's uncontained pin is still unmeasured on
Windows. The card stays in Backlog until both are recorded.

## Linux, with and without `app_bpr` (bpr-main-linux, 2026-09-29)

Ubuntu 26.04 on WSL2, GCC 15.2, `build/dev` Debug, at `bfc02529a` (which
contains `ab9d3fd7`).

- **Without `app_bpr`** (the dev preset's `BPR_BUILD_TOUR_CLI=OFF`): the
  registered command ends at `--desktop-application …/better_pull_review`,
  with no empty arguments. `application_profile_activation_public_path`
  passes in 3.34 s.
- **With `app_bpr`** (the same tree reconfigured with
  `-DBPR_BUILD_TOUR_CLI=ON`, then built with `app_bpr`): the registration adds
  `--tour-application …/src/app/bpr`. The row passes in 4.00 s, and on POSIX
  the script's `tour_owner_course` runs whenever that argument is present.
  The tree was then reconfigured back to `OFF`.

This is the Linux half of "macOS and Linux still pass", with `app_bpr` both
present and absent. The Windows uncontained pin is not a Linux question and
remains open. The logs are in `build/logs/bpr-station-rounds/` and are not
committed.

## The Windows pin, measured — bpr-main-win, 2026-10-01

Both rows the [[BPR-0979]] pin covers were run contained, through the
sandbox launcher:

- `sqlite_profile_activation` stops at `canonical: Access is denied.` on its
  root, `bpr-act-…` in the container's temporary directory;
- `application_profile_activation_public_path` refuses at its seed's
  registry, the retained ancestor chain.

Both are `dos-volume-name`, and both pass uncontained: the course in 17.21 s,
and the contract in [[BPR-1121]]'s Windows run. The pin's comment in
`scripts/verify_test_network_isolation.py` now records this in place of "not
yet measured contained on Windows". Only the macOS or Linux run with `app_bpr`
is still owed here.
