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
