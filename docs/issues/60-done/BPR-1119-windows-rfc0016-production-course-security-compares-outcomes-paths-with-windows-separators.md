---
schema: "implement-rfc.card.v1"
id: "BPR-1119"
title: "Windows: rfc0016_production_course_security compares outcomes/ paths with Windows separators"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win segment 7, the core-only CTest run (94a09016..877e3d60)","related_rfcs":["RFC-BPR-0016"],"related_cards":["BPR-1120"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1119--BPR-1120--11fa21d04f58.json"}
acceptance: [{"criterion":"AC1 rfc0016_production_course_security passes on Windows, run through CTest as registered","outcome":"pass","revision":"19c0699e","evidence":{"kind":"command","detail":"bpr-main-win, build/dev configured at 8a56b80a, Python 3.14.7: ctest --preset dev -j1 -R '^rfc0016_production_course_security$' (contained, as registered) Passed 0.43 s, serial with operator_response_validation_contract, rfc0016_relocated_operator_contract, contained_temporary_usage_contract and contained_temporary_support_contract (5/5). The file as it was fails 1 of 39 (test_published_evidence_is_admitted_by_the_verifier, the lists differ only in backslash against slash); fixed, 39 OK.","path":"docs/issues/60-done/BPR-1119-windows-rfc0016-production-course-security-compares-outcomes-paths-with-windows-separators.md#fixed-in-the-test-bpr-main-win-2026-09-24"}},{"criterion":"AC2 The test still fails when the published file list differs (negative control)","outcome":"pass","revision":"19c0699e","evidence":{"kind":"command","detail":"The fixed test run with outcomes/token_success.json renamed in its expected list in memory: FAILED (failures=1), the diff naming 'outcomes/token_success.json' against 'outcomes/token_success_renamed.json'.","path":"docs/issues/60-done/BPR-1119-windows-rfc0016-production-course-security-compares-outcomes-paths-with-windows-separators.md#fixed-in-the-test-bpr-main-win-2026-09-24"}},{"criterion":"AC3 macOS and Linux are unchanged","outcome":"pass","revision":"19c0699e","evidence":{"kind":"freeform","detail":"On POSIX as_posix() of a relative path equals the str() the test used before, so the compared list is byte-identical there. No macOS or Linux run from this station.","path":"docs/issues/60-done/BPR-1119-windows-rfc0016-production-course-security-compares-outcomes-paths-with-windows-separators.md#fixed-in-the-test-bpr-main-win-2026-09-24"}}]
---

# BPR-1119: Windows: rfc0016_production_course_security compares outcomes/ paths with Windows separators

## Defect

`rfc0016_production_course_security` fails on the Windows station, and only
there. One of the 39 tests in `tests/scripts/rfc0016_production_course_tests.py`
fails: `test_published_evidence_is_admitted_by_the_verifier`. It lists the
published course's files as `str(path.relative_to(course))` and compares
them with names written with `/`. On Windows, `str()` of a relative path uses
`\`, so `outcomes\authorization_pending.json` never equals
`outcomes/authorization_pending.json`.

Measured at `685ae35f` with Python 3.14.7, running the test file directly:
`FAILED (failures=1)` of 39.

```
AssertionError: Lists differ: [... 'outcomes\\authorization_pending.json', ...] != [... 'outcomes/authorization_pending.json', ...]
```

The published names are the expected ones, and only the separator differs.
So the course publishes what it should, and the comparison is what depends
on the platform.

## Scope

- Compare the relative paths in one form on every platform (`as_posix()`).
- No change to the course or to what it publishes.

## Acceptance

- AC1 `rfc0016_production_course_security` passes on Windows, run through
  CTest as registered.
- AC2 The test still fails when the published file list differs (negative
  control).
- AC3 macOS and Linux are unchanged. `as_posix()` of a POSIX relative path is
  the string `str()` gave there before.

## Fixed in the test (bpr-main-win, 2026-09-24)

The change is `19c0699e`: the test lists the published files with
`path.relative_to(course).as_posix()`. Nothing else changed. Run
`bpr-main-win-20260923`, `build/dev` configured at `8a56b80a`, Python 3.14.7.
The logs are station records under `build/logs/bpr-1119-main-win/` and
`build/logs/bpr-1120-main-win/rows.log`, and are not committed.

| Run | Result |
| --- | --- |
| The test file as it was, run directly (`685ae35f`) | `FAILED (failures=1)` of 39: `test_published_evidence_is_admitted_by_the_verifier`, lists differ only in `\` against `/`. |
| The fixed file, run directly | 39 tests, `OK`. |
| `rfc0016_production_course_security` through CTest as registered (contained), serial with the four rows of [[BPR-1120]] | Passed, 0.43 s. |
| Negative control (AC2): the fixed test, with `outcomes/token_success.json` renamed in its expected list in memory | `FAILED (failures=1)`: `- 'outcomes/token_success.json'`, `+ 'outcomes/token_success_renamed.json'`. |

**AC1 met:** the row passes on Windows through CTest. **AC2 met:** a list
that differs still fails the test. **AC3:** on POSIX, `as_posix()` of a
relative path is the string `str()` already gave, so macOS and Linux compare
the same list as before. No run was made there from this station.
