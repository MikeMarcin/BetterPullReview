---
schema: "implement-rfc.card.v1"
id: "BPR-1228"
title: "Restore the application composition contracts at main"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"reported red at baseline by the BPR-1211 implementer; diagnosed at main 22d184166 by the Review Cycles intake session on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1228--BPR-1228--e74f102785c8.json"}
---

# BPR-1228: Restore the application composition contracts at main

## Observation

At `main` 22d184166 on macOS arm64,
`ctest --test-dir build/dev -R "application_review_round_trip_composition_contract|^application_composition_contract$" --output-on-failure`
fails both tests. There are three separate causes, and only the first is a
reseal.

1. **Pinned composition digest.** `tests/end_to_end/application_review_round_trip_composition_contract.cmake:481`
   reports the full LF-normalized `src/app/src/github_application.cpp` digest
   drifted: expected `fa135765…265e6f`, found `bb53bee5…e78e17`. The pin was
   last resealed at aa8826e5a. Five later commits change the file without a
   reseal: 27dbeef51 and 637cd41df (tour owner channel and live-draft routing,
   RFC-BPR-0045), eca094604 (agent tour-writing tickets), bf7a96367 (BPR-1214)
   and 9ad03a828 (BPR-1211). The pin's convention is a reviewed reseal: a
   comment block records what each pinned-scope commit changes and that the
   production mutation branch and grants are unchanged. That review has not
   been done, so this is not a mechanical hash update.
2. **Ambiguous testing grant.** `composition_contract_tests.py`
   `test_whole_review_authority_is_distinct_and_test_factory_is_private`
   compiles `make_review_submission_testing_grant({}, {}, {})` in the testing
   graph. a6513264c added a second overload taking `const staged_review_plan&`,
   so the braced call is ambiguous and the testing declaration no longer
   compiles. The probe needs a typed third argument (or the overloads a
   distinct name); decide which keeps the test's intent that the factory exists
   only in testing graphs.
3. **Release graph names the public-path fixture.**
   `test_tests_off_release_graph_omits_public_path_fixture` asserts
   `github_public_path_fixture.cpp` is absent from the tests-off `build.ninja`.
   It is not compiled, but `src/adapters/logging/CMakeLists.txt` globs every
   `src/*.cpp`/`*.hpp` into `CMAKE_CONFIGURE_DEPENDS` for the diagnostic build
   identity (ac64e5ad0/3381a02c5), so the file appears in the "re-run CMake"
   rule. Either the test should inspect compile edges only (it already checks
   `compile_commands.json`) or the identity glob should exclude testing-only
   sources; that is a design choice about what the diagnostic build identity
   binds.

## Outcome

Both contract tests pass at the resulting `main` head, with each cause fixed at
its owner: a reviewed reseal comment for the five commits, a probe or overload
change that keeps the testing-only factory contract, and a decision on the
release-graph assertion versus the diagnostic identity glob.

## Direct acceptance

- The two named ctest entries pass on macOS at the candidate revision.
- The reseal comment names each pinned-scope commit and what it changes.
- `python3 scripts/verify_documentation.py --root .` passes.
