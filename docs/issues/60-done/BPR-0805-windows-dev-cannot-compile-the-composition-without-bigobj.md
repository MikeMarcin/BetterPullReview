---
schema: "implement-rfc.card.v1"
id: "BPR-0805"
title: "windows-dev cannot compile the application composition without /bigobj"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-win during task-20260917-win-evidence (BPR-0798 automated half)","found_at":"f2b8e682255fc553a87d4d164c3b27f061300e07","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"windows-dev Debug compiles and links better_pull_review_composition and test_profile_management_public_path","outcome":"pass","revision":"32a983e6","evidence":{"kind":"observation","detail":"bpr-win native Windows build of windows-dev Debug targets with /bigobj; C1128 gone; course executed and passes at d587b5f1"}},{"criterion":"windows-app-release still compiles","outcome":"pass","revision":"32a983e6","evidence":{"kind":"observation","detail":"bpr-win windows-app-release build; image sha256 46619a53... used for BPR-0806/0730/0798 evidence"}},{"criterion":"macOS dev configure unchanged (MSVC-gated)","outcome":"pass","revision":"c0f2fa31","evidence":{"kind":"command","detail":"cmake --preset dev -B build/r2-dev exit 0 at merge; 15 storage/profile courses built and passed; build/r2-logs/merge1-configure.log, merge1-ctest.log"}}]
---

# BPR-0805: windows-dev cannot compile the application composition without /bigobj

## Defect

The Windows station's `windows-dev` Debug build of `f2b8e682` stops at:

    src/app/src/github_application.cpp(1,1): error C1128: number of sections exceeded object file format limit: compile with /bigobj  [better_pull_review_composition.vcxproj]

No target in the repository passes `/bigobj`. The composition translation unit
has outgrown MSVC's default COFF section limit in Debug, so no tests-enabled
Windows graph can build the application or any course that links
`better_pull_review_composition`. That includes
`application_profile_management_public_path`, which [[BPR-0798]] needs.

## Scope

- Pass `/bigobj` to MSVC for `better_pull_review_composition`, and only for
  MSVC. Leave other compilers and targets alone.
- Owned path: `src/app/CMakeLists.txt`.

## Acceptance

- `windows-dev` Debug compiles and links `better_pull_review_composition` and
  `test_profile_management_public_path` on the Windows station.
- `windows-app-release` still compiles.
- The macOS dev configure is unchanged, because the option is MSVC-gated.

## Evidence

Not run at card creation. bpr-win holds the fix as a `windows-lane` commit
(local `76f6e3a4` on `f2b8e682`), and both Windows trees built green with it
at 21:18-21:20 local on 2026-09-16. The integrator finishes this card on `main`
after the station pushes the commit and the merge is checked.

## Evidence, 2026-09-17 (task-20260917-win-storage)

The fix is station commit `32a983e6`, merged on main in `c0f2fa31`. It adds
`/bigobj` to `better_pull_review_composition` under `if(MSVC)` in
`src/app/CMakeLists.txt`.

| Criterion | Check | Result | Revision |
|---|---|---|---|
| `windows-dev` Debug compiles and links the composition and `test_profile_management_public_path` | bpr-win `cmake --build` of the windows-dev Debug targets `app_better_pull_review` and `test_profile_management_public_path`; C1128 gone; the course then ran (it passes at `d587b5f1` under [[BPR-0806]]) | pass | `32a983e6` |
| `windows-app-release` still compiles | bpr-win `cmake --build --preset windows-app-release`; image built and used for BPR-0806/0730/0798 | pass | `32a983e6`, `d587b5f1`, `e93d9215` |
| macOS dev configure unchanged | `cmake --preset dev -B build/r2-dev` at `c0f2fa31` exits 0, and 15 storage/profile courses built and passed there (`build/r2-logs/merge1-*.log`). The option is inside `if(MSVC)`. | pass | `c0f2fa31` |
