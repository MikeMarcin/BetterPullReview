---
schema: "implement-rfc.card.v1"
id: "BPR-1119"
title: "Windows: rfc0016_production_course_security compares outcomes/ paths with Windows separators"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win segment 7, the core-only CTest run (94a09016..877e3d60)","related_rfcs":["RFC-BPR-0016"],"related_cards":["BPR-1120"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1119--BPR-1120--11fa21d04f58.json"}
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
