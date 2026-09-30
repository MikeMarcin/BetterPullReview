---
schema: "implement-rfc.card.v1"
id: "BPR-1240"
title: "The scoped writing agent fixture compiles as C++98 because its target declares no standard"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"recorded by the BPR-1211 implementer on 2026-09-29; filed by the main stabilization milestone","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1211"]}
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
