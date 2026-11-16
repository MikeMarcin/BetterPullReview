---
schema: "implement-rfc.card.v1"
id: "BPR-2200"
title: "On Windows, the Qt tests load the platform's fonts under offscreen"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-2197's Windows width courses, Windows station over SSH, 2026-11-15","reservation":"docs/issues/_reservations/BPR-2199--BPR-2200--d19a716e3a2b.json","related_cards":["BPR-2197","BPR-2199"]}
---

# BPR-2200: On Windows, the Qt tests load the platform's fonts under offscreen

## Found

ctest runs the Qt courses on Qt's offscreen platform
(`QT_QPA_PLATFORM=offscreen` in `bpr_qt_github_common_environment`). On
Windows that platform finds no fonts unless `QT_QPA_FONTDIR` names them.
Every font resolves to an unnamed fallback (`QFontInfo` family "", point
size -1), about twice as wide as Segoe UI 9 pt: the whole pull request name
measured 1388 pixels against 624. Windows results therefore reflect no real
font. Rows yield at widths no desktop sees, and anything that depends on font
metrics is tested against the fallback.

BPR-2197 fixed this only for the window's `--title-row` courses
(`7038aecdb` and `c2020a55c` on the lane). Before the application starts,
their `main` points `QT_QPA_FONTDIR` at `%WINDIR%/Fonts`, then sets Segoe UI
9 pt. Those courses then passed on the Windows station at 100%, 125% and
200%.

Without the fonts, the full window course over SSH stops early, both at
`8eb302151` (before BPR-2197) and at `22220f4f0`:

    FAILED: with no review in progress Compose review reads "Approve or comment…" beside its pencil

With the fonts loaded and Segoe UI 9 pt, it gets past that check and stops at
the Tour case instead (BPR-2199):

    FAILED: the Tour tab is the route: every stop in view, no list of its own to scroll

## Scope and acceptance

- On Windows, every Qt test under `tests/adapters_qt` that runs on the
  offscreen platform loads the Windows fonts folder and uses Segoe UI 9 pt,
  the way BPR-2197's title-row fix does.
  - `QT_QPA_FONTDIR` comes from `%WINDIR%`, unless it is already set.
  - One place sets it: the shared test environment, or a shared startup
    helper such as `native_look_startup.cpp`. Not each test.
  - The window's `--title-row` special case is folded into it.
- Find out what the Compose review wording check (`with no review in
  progress Compose review reads "Approve or comment…" beside its pencil`)
  shows with the fonts loaded, and settle it. A check that failed only on
  the fallback is recorded as such; a real Windows fault is fixed or carded.
- Every Windows Qt course that this change newly fails is recorded with its
  failure line, then fixed or carded. BPR-2199 is the first.
- macOS and Linux are unchanged.

## Reproduce on the Windows station

1. Export the lane to a scratch folder beside the station checkout.
   - Clone: `git clone --shared --no-checkout C:/code/git/BetterPullReview SCRATCH/src`.
   - Fetch a bundle of the lane into the clone and check it out.
2. Configure:
   `cmake --preset windows-dev -DCPM_SOURCE_CACHE=C:/code/git/BetterPullReview/build/.cpm-cache`
3. Build:
   `cmake --build build/windows-dev --config Release --target test_better_pull_review_qt_workbench_window_tests -- -m:8`
   - In Git Bash, MSBuild switches take a dash; Git Bash rewrites `/m:8`
     into a path.
4. Put two folders on `PATH`:
   - `build/windows-dev/Release`, for `bpr_xdiff.dll`;
   - Qt's `.cpm-cache/bprqt/6.12.0/6.12.0/msvc2022_64/bin`.
5. Run the course.
   - Without fonts, as ctest runs today:
     `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.0 BPR_EXPECTED_QT_SCALE=1.0 better_pull_review_qt_workbench_window_tests.exe`.
   - With fonts: add `QT_QPA_FONTDIR=C:/Windows/Fonts` and set Segoe UI 9 pt.
   - `BPR_TEST_CASE_TRACE=1` names each case as it starts;
     `BPR_WORKBENCH_ONLY_CASE=NAME` runs one.
6. Remove the scratch folder afterwards.

ctest itself cannot start these courses over SSH. Its network sandbox
(`tool_better_pull_review_network_sandbox`) fails to start the test process
with 0xc0000142 (DLL initialization failed), before the test runs. Over SSH,
run the executables directly with the environment ctest would set, or run
ctest from a desktop session.

Qt's `windows` platform is no substitute over SSH either: the session's
screen is about 1028 pixels wide, and Windows clamps larger windows to it.

## Intended verification

The Windows Qt courses on the station with the fonts loaded, each new
failure recorded. macOS and Linux unchanged.
