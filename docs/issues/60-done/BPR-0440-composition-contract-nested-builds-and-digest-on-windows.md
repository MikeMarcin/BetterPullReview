---
id: BPR-0440
title: The composition contract's digest pin and nested builds on Windows
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# The composition contract's digest pin and nested builds on Windows

`application_composition_contract` fails on the Windows host uncontained, at
`22b991c`, on two distinct things.

**The digest pin did not follow BPR-0421.** The contract pins the content
digest of `src/app/src/github_application.cpp`, and [[BPR-0421]] reordered
two designators in that file from this branch without running the Qt graph,
which is the only graph that checks the pin:

```
CMake Error: github_application.cpp digest drifted
```

[[BPR-0436]] changes the same file again. The pin is re-recorded once, after
both, against the file as it then stands.

**The nested builds do not run here.** Three cases configure or compile
something under the test — `test_language_seal_assertion_rejects_what_does_not_prove_a_seal`
(`seal-control-compiles.cpp`, exit 2), `test_production_mutation_authority_issuers_are_language_sealed`
(`production-baseline.cpp`, exit 2), and
`test_tests_off_release_graph_omits_public_path_fixture`, which builds a
tests-disabled Release tree with Ninja and stops after
`[1/252] Scanning … appearance_contrast.cpp for CXX dependencies`. Exit 2
from a compile step that has not been read is not a cause; each needs its
first diagnostic captured and attributed, and the Release build's stop may
be the same Smart App Control wait [[BPR-0424]] records for `cmTC` probes.

## Required final state

- The `github_application.cpp` digest is re-pinned against the committed
  file and the contract passes that case.
- Each of the three nested-build failures is attributed from its own
  diagnostic; whatever is a test-harness assumption about the compiler
  environment is corrected, and whatever is the host's policy is recorded
  on [[BPR-0424]].
- `application_composition_contract` passes on Windows as registered and is
  unchanged elsewhere.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^application_composition_contract$'
```

## Attributed and repaired — 2026-08-23

The pin is re-recorded once, against `github_application.cpp` as it stands
after [[BPR-0421]] and [[BPR-0436]]: `1ec6e1ff524bbdc9ec4721ab5d88e413aa4fdaa54f89eedce3a429c03f0d8ea9`.
The same drift failed `application_review_round_trip_composition_contract`
in the matrix, which reads the same pin.

The three nested builds, each from its own first diagnostic in the full
`windows-dev` matrix at `b14fc0e`, uncontained as registered:

| case | first diagnostic | cause | repair |
|---|---|---|---|
| `test_language_seal_assertion_rejects_what_does_not_prove_a_seal` (`seal-control-compiles.cpp`, exit 2) | `model.hpp(12): error C2429: language feature 'nested-namespace-definition' requires compiler flag '/std:c++17'` | `compile_snippet` hands `cl` the Clang/GCC line — `-std=c++23 -fsyntax-only` — which cl ignores with a warning, and then compiles as C++14; the control snippet could never compile, so the assertion that it does failed | `compile_snippet` detects `cl` and uses `/std:c++latest /permissive- /EHsc /utf-8 /Zs`; `diagnostic_messages` reads `error C2039: …` as well as `error: …`; the three reason patterns carry MSVC's spellings (`undefined type`, `undeclared identifier`) |
| `test_production_mutation_authority_issuers_are_language_sealed` (`production-baseline.cpp`, exit 2) | the same `C2429` | the same | the same |
| `test_tests_off_release_graph_omits_public_path_fixture` (Ninja Release, tests off, stopped at `main.cpp.obj`) | `main.cpp(5419): error C2039: 'supervisor_image_for': is not a member of 'bpr::app'` | not the host's policy: a product build break. The attended designated-live approval branch of a tests-off `main` calls `designated_live_provenance::supervisor_image_for`, which `designated_live_macos_provenance.hpp` declares only under `__APPLE__` ([[BPR-0352]]); no tests-off build had been compiled on another platform since | on a platform without the capsule the branch refuses with `STARTUP_FAILED`, which is what an attended approval with nothing to verify against should do; the macOS path is untouched |

The Release build's stop was therefore not [[BPR-0424]]'s wait — it was the
compiler, read from its own line — and nothing is recorded there.

### Evidence

| claim | command | result |
|---|---|---|
| the contract passes on Windows as registered | `ctest --preset windows-dev -R '^application_composition_contract$'` | Passed, 5 of 5 cases in 290 s, uncontained as registered (`cards-run-13.log`, 2026-08-23; the tests-off Release build inside it cleared once the fresh image's policy verdict landed) |
| the round-trip contract reads the re-pinned digest | `ctest --preset windows-dev -R '^application_review_round_trip_composition_contract$'` | Passed, 15.4 s, with the object manifest its Windows inspection mode requires (`cards-run-30.log`, 2026-08-23) |
| macOS and Linux are unchanged | the Clang/GCC line is the previous one; the `#if !defined(__APPLE__)` refusal is new code on Linux, where the branch did not compile before | the Linux lane's next tests-off build is the check |

Implemented by the integrator.
