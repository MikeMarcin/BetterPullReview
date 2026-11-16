---
schema: "implement-rfc.card.v1"
id: "BPR-2207"
title: "architecture_boundary_contract's fixture lacks src/app/CMakeLists.txt"
status: "Ready"
rfcs: []
metadata: {"reservation":"docs/issues/_reservations/BPR-2206--BPR-2207--fa4afcace75b.json","found_by":"BPR-2198","since":"3a224ad64"}
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
