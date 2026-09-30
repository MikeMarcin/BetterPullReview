---
schema: "implement-rfc.card.v1"
id: "BPR-1240"
title: "The scoped writing agent fixture compiles as C++98 because its target declares no standard"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"recorded by the BPR-1211 implementer on 2026-09-29; filed by the main stabilization milestone","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1211"]}
acceptance: [{"criterion":"scoped_writing_agent.cpp compiles as C++23 and the dev build completes","outcome":"pass","revision":"790bc57cf","evidence":{"kind":"command","detail":"compile_commands.json shows -std=c++23; cmake --build --preset dev -- -k 0 exit 0 on macOS arm64 (Apple clang)."}},{"criterion":"application_review_tour_writing passes on macOS","outcome":"pass","revision":"790bc57cf","evidence":{"kind":"command","detail":"Full ctest --test-dir build/dev run: application_review_tour_writing Passed 27.38 s."}},{"criterion":"Documentation gate","outcome":"pass","revision":"790bc57cf","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1240: The scoped writing agent fixture compiles as C++98 because its target declares no standard

## Observation

At `main` e9e65eab2 on macOS arm64 (dev preset, `/usr/bin/c++`, Apple clang),
`cmake --build --preset dev` stops at
`tests/end_to_end/scoped_writing_agent.cpp`: the compile line carries
`-std=c++98`, and the file's trailing return types, braced initializers and raw
strings do not compile. `tests/end_to_end/CMakeLists.txt` adds
`test_scoped_writing_agent` with `bpr_add_executable` and no
`target_compile_features`, so the language level is whatever the configured
compiler defaults to. [[BPR-1211]] recorded the same failure; a fresh configure
happened to pick C++14 and passed.

Without the executable, `application_review_tour_writing` cannot start its
scripted agent, and the full dev build is red.

## Outcome

The target declares the repository's language level, like every other
executable in that file, so it compiles the same way on every configure.

## Direct acceptance

- The compile command for `scoped_writing_agent.cpp` carries `-std=c++23` in
  the configured `build/dev`, and `cmake --build --preset dev` completes.
- `application_review_tour_writing` passes on macOS.
- `python3 scripts/verify_documentation.py --root .` passes.

## Resolution (macOS arm64, 2026-09-29)

`790bc57cf` adds `target_compile_features(test_scoped_writing_agent PRIVATE cxx_std_23)`.
After it, `build/dev/compile_commands.json` compiles `scoped_writing_agent.cpp`
with `-std=c++23`, `cmake --build --preset dev -- -k 0` completes with no
failed step, and `application_review_tour_writing` passes in the full dev run
(27.4 s). Windows and Linux compile the file under their own presets; the
declaration makes the level explicit there too, and was not run on those
stations by this card.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

The full dev build compiles `test_scoped_writing_agent`, and `application_review_tour_writing` passes. The same full build first needed two GCC-only test fixes, in `754502ff7`: `-Wdangling-pointer` in `agent_launch_tests.cpp` and `-Wshadow=local` in `gh_cli_transport_tests.cpp`.

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## Windows — bpr-main-win, 2026-09-30

Windows 11 Home 10.0.26200, MSVC 19.51, clang-format 22.1.8, Qt 6.11.2, offscreen, `build/windows-dev` Debug with warnings as errors, at `69493e23`.

`test_scoped_writing_agent` builds as C++23 under MSVC. `application_review_tour_writing` exited 1 contained, with no line, and passed uncontained. `0f8b9ebe` makes the row `WINDOWS_UNCONTAINED`, pinned with that measurement. Then it passes, 22.79 s.
The details are in [main stabilization](../../verification/main-stabilization-2026-09-30.md).
