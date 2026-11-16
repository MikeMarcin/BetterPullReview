---
schema: "implement-rfc.card.v1"
id: "BPR-2207"
title: "architecture_boundary_contract's fixture lacks src/app/CMakeLists.txt"
status: "Done"
rfcs: []
metadata: {"reservation":"docs/issues/_reservations/BPR-2206--BPR-2207--fa4afcace75b.json","found_by":"BPR-2198","since":"3a224ad64","lane":"claude/v0-5-0-prerelease-targets-52265d","commit":"b59d7c61d"}
depends_on: []
---

# BPR-2207: the boundary contract's fixture has no app CMakeLists

## Scope and acceptance

`architecture_boundary_contract` fails (about 300 s, many cases) with:

```
FAIL: test_cli_runner_rejects_raw_errors_and_logging (...)
AssertionError: 'Forbidden raw process error-string exposure surface' not found in
'-- Application, provider, and Qt architecture boundaries verified
CMake Error at cmake/VerifyArchitectureBoundaries.cmake:6368 (file):
  file failed to open for reading (No such file or directory):
    /private/tmp/bpr-net-.../fixture/.../src/app/CMakeLists.txt'
```

`3a224ad64` (BPR-2184) added an unconditional
`file(READ "${BPR_ROOT}/src/app/CMakeLists.txt" ...)` to
`VerifyArchitectureBoundaries.cmake` (the fixture agent is built only when
testing is on), and the contract tests' minimal fixture trees
(`tests/scripts/architecture_boundary_tests.py`) do not contain that file,
so the verifier stops there instead of reaching the hostile surface each
case plants. `architecture_boundaries` on the real tree passes.

Acceptance: the fixture trees carry the file (or the check reads it only
where the tree has one, without weakening it on the real tree), and
`architecture_boundary_contract` passes.

## Cause

The missing app CMakeLists was one of 16 failing cases with one cause.
`3a224ad64` (BPR-2184) made every rule run: `message(FATAL_ERROR …)` is
overridden to record the violation and go on, and the gate prints them all
at the end. A rule that finds a required file missing therefore went on to a
later `file(READ)` of that file (or a `string(SUBSTRING)` on an index it did
not find). That is a CMake script error, which ends `cmake -P` at once, before
the end prints the recorded violations. Each case's expected message was
recorded and never said. The fixture-agent rule added in the same commit read
`src/app/CMakeLists.txt` unconditionally, and the fixture trees have none.

## Fix (`b59d7c61d`)

- `cmake/VerifyArchitectureBoundaries.cmake`: each violation is said
  (`SEND_ERROR`) when it is recorded, so a later script error cannot drop it;
  the end names the count and fails. The fixture-agent rule reads
  `src/app/CMakeLists.txt` only when it exists, and its absence is the rule's
  violation ("must define the fixture agent"). The real tree is checked as
  before.
- `tests/scripts/architecture_boundary_tests.py`: the fixture trees carry an
  app CMakeLists that builds `bpr_fixture_agent` inside `if(BUILD_TESTING)`;
  cases that supply their own app CMakeLists append it. A new case,
  `test_fixture_agent_is_built_only_when_testing`, holds the rule: no file, no
  fixture agent, and the agent outside `if(BUILD_TESTING)` each fail.

## Evidence

- `ctest --preset dev -R '^(architecture_boundary_contract|architecture_boundaries)$'`
  on the lane: both pass (contract 810 s, boundaries 66 s). Before the fix the
  contract failed 16 cases.
