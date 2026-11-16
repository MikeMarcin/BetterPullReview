---
schema: "implement-rfc.card.v1"
id: "BPR-2200"
title: "On Windows, the Qt tests load the platform's fonts under offscreen"
status: "Done"
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

## What was done (2026-11-15)

- `tests/adapters_qt/windows_fonts_startup.cpp` (`18a70bb96`), linked on
  Windows into every `test_` executable of `tests/adapters_qt` that links Qt
  Widgets, Gui or the Qt adapter. The loop is last in the directory's
  `CMakeLists.txt`, like the macOS native-look startup. Before `main`, it
  points `QT_QPA_FONTDIR` at `%WINDIR%/Fonts` unless that is already set.
  Once the application exists on the offscreen platform, it sets Segoe UI
  9 pt. It does nothing on another platform, and macOS and Linux build
  nothing from it.
- The window course's `--title-row` special case is gone. Its `main` names
  the shared startup.
- `src/adapters/qt/AGENTS.md`: Windows offscreen tests use the real fonts
  from one place, and a layout that holds in only one font is a product bug.

One correction to the premise: ctest itself already set
`QT_QPA_FONTDIR=%WINDIR%/Fonts` for every Qt test on Windows
(`cmake/BprTestIsolation.cmake`, BPR-0412). The fonts were missing only
when a course was run directly over SSH without that environment, as
BPR-2197 and this card ran them. What ctest lacked was the desktop's
font: the application font was offscreen's default, not Segoe UI 9 pt.
The startup now covers both.

## Compose review wording, settled

`with no review in progress Compose review reads "Approve or comment…" beside its pencil`
failed only on the no-font fallback. With the fonts loaded,
`test_compose_review_says_approve_or_comment_with_no_review` passes alone,
and the whole window course gets past it. No product fault.

## The Windows Qt set with the fonts (station, Release, `18a70bb96`)

All 242 ctest entries whose executable is under `tests/adapters_qt` ran
directly with ctest's environment (`ENVIRONMENT` and
`ENVIRONMENT_MODIFICATION`), its pass and fail expressions, and `RUN_SERIAL`
entries run alone. They ran twice: as built (fonts), and with
`QT_QPA_FONTDIR` pointed at an empty folder (no fonts, the earlier SSH
state). Fonts: 16 failed. No fonts: 76 failed. 61 entries pass only with
the fonts.

Failed with the fonts, and settled here or carded:

| Entry | Failure line with the fonts | Without fonts | Outcome |
|---|---|---|---|
| `qt_workbench_window_scale_100/125/200` | `at 1280 the review bar fits the view controls, the position and "Approve…", and the title row names the pull request whole` | stopped earlier, at the Compose review wording | Stale test, corrected (`2c17d36f6`). Segoe UI 9 pt fits the whole "Approve or comment…" at 1280 (bar hint 1234 of 1280), and the product keeps the words because they fit. The 1366 check already accepted either form. The whole course then passes at all three scales. |
| `qt_drawer_surfaces_fit_scale_100/125/200` | `16 surface states are clipped or break the principles` (Overview tour-reader: `"Agent" ends at 310 in a 262 px bar`) | stopped earlier, on the Agent tab | Product bug, carded [[BPR-2201]] |
| `qt_workbench_line_draft` | `a right-click inside a dragged selection names its lines` (caption `Line 4`) | passed | Carded [[BPR-2202]] |
| `qt_diff_workbench_scale_125` | `light: every marked line number reaches the theme's text floor on its tinted cell` (line 11, 4.45) | stopped earlier, on the open-thread bar | Carded [[BPR-2203]] |

These failed on the same line with and without fonts, so the fonts did not
cause them: `qt_agent_finding_cards_restart`, `qt_agent_setup_view`,
`qt_agent_steering_fixture`, `qt_claude_process_wrapper`,
`qt_profile_management_window`, `qt_prompt_macro_chips`,
`qt_scoped_prose_editor`, `qt_source_search_panel_scale_1`. They are left
as found: direct runs outside ctest's sandbox, and work in progress
elsewhere on the lane.

## Verification

- Windows station: the `--title-row` course passes at 100%, 125% and 200%
  through the shared startup alone, with no `QT_QPA_FONTDIR` given. It
  failed on the fallback, so the fonts load. The tour case passes alone,
  and so does the whole window course at all three scales.
- macOS `dev`: `qt_workbench_window_scale_100/125/200` pass at `2c17d36f6`.
  The startup compiles to nothing there.
- Documentation gate: `DOCUMENTATION_OK`.
- Station scratch folder removed.

## Correction (2026-11-15, main baseline)

Seven of the eight entries listed above as failing the same way with and
without fonts also fail on main (`ba6b79d67`), and are carded together as
[[BPR-2205]]. The eighth, `qt_source_search_panel_scale_1` (`painted wash
spans the match text`), passes on main with ctest's environment, where the
fonts load but the application font is the offscreen default. It fails on
main without fonts, and on the lane with Segoe UI 9 pt. It therefore depends
on the font, not on lane code (the lane changes neither the panel nor its
test), and it is a failure this card's Segoe UI 9 pt newly exposes. It is
reported to the coordinator, not yet carded.

Carded and fixed as [[BPR-2208]]: the wash is placed from the drawn text layout, and the test measures its widest run.
