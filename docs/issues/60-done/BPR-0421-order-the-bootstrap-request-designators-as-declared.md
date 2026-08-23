---
id: BPR-0421
title: The core-only graph stopped compiling under MSVC on a misplaced designator
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# The core-only graph stopped compiling under MSVC on a misplaced designator

The first fresh `core-only` build of the Windows lane, from the `windows-lane`
worktree at `3cc9393`, stopped at one error:

```
tests\application\designated_review_authority_bootstrap_tests.cpp(193): error C7560:
'BuildConfiguration': designators must appear in member declaration order of class
'bpr::application::detail::designated_review_bootstrap_request'
```

`ninja -k 0` over the whole graph found no second one: 363 of 364 edges built.

[[BPR-0352]] added `SupervisorSha256` and `InventorySha256` to
`designated_review_bootstrap_request` on 2026-08-18 and declared them *after*
`BuildConfiguration` (`src/application/src/review_mutation_execution_authority.hpp:83-108`),
but both places that initialize the aggregate list them *before* it:
the test fixture at `designated_review_authority_bootstrap_tests.cpp:190-194`
and the production composition at `src/app/src/github_application.cpp:1817-1823`.
C++20 requires designators in declaration order; MSVC enforces it, and this is
the same class [[BPR-0338]] already repaired twice in the Qt graph.

The production site is in the Qt graph and so was not reached by this build,
but it is the same defect and would have stopped `windows-dev` the same way.

## Required final state

- Both initializers list their designators in the declared order.
- No initializer expression changes, and the reorder is observationally
  inert under C++20's left-to-right evaluation: the moved entries are a string
  literal and a copy of `aConfig.BuildConfiguration`; the only `std::move`
  (`.Replay`) keeps its position.
- `core-only` builds to completion under MSVC 14.51 and
  `application_designated_review_authority_bootstrap` passes contained.
- Both files are `clang-format` clean.

## Not in scope

Preventing the class from recurring. The clang/GCC targets carry `-Werror`
(`cmake/Warnings.cmake:15`), under which clang's default-on
`-Wreorder-init-list` should already have refused this on macOS; why the
2026-08-18 macOS build accepted it is not measurable from this host and is
recorded as unmeasured rather than guessed.

## Verification

```sh
cmake --build --preset core-only
ctest --preset core-only --output-on-failure -R '^application_designated_review_authority_bootstrap$'
clang-format --dry-run --Werror -style=file \
  tests/application/designated_review_authority_bootstrap_tests.cpp \
  src/app/src/github_application.cpp
```

run with the MSVC x64 environment imported.

## Result

Both initializers now list `BuildConfiguration` third, where the struct
declares it. Nothing else moved.

```
cmake --build --preset core-only                                       364/364 edges, 0 failed
ctest --preset core-only -R '^application_designated_review_authority_bootstrap$'
                                                                        1/1 Passed (contained)
clang-format --dry-run --Werror -style=file <both files>               clean
git diff --check                                                       clean
```

The test ran under the AppContainer launcher, which is the only way
`core-only` registers it. That is the first time this test has executed on
Windows: before this card the object it lives in did not compile.
