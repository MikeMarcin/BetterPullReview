---
schema: "implement-rfc.card.v1"
id: "BPR-1484"
title: "Pin the PR Search rows' Windows-uncontained registrations"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1484--BPR-1484--9823868ffbb1.json","origin":"test_network_isolation_manifest fails on main at e145ccd35, owner request 2026-10-13","related_rfcs":["RFC-BPR-0077"],"related_cards":["BPR-1423","BPR-1343","BPR-1348"]}
---

# Pin the PR Search rows' Windows-uncontained registrations

`test_network_isolation_manifest` fails on main (seen at e145ccd35):

```text
TEST_NETWORK_ISOLATION_FAILED:application_source_pr_find is registered WINDOWS_UNCONTAINED without an entry in WINDOWS_UNCONTAINED_TESTS; add one in the same commit, "unmeasured (registered on <platform>)" until Windows measures it
```

and the same for `application_source_pr_search_scope`,
`application_source_pr_search_performance`,
`application_source_pr_find_scale_1_25`,
`application_source_pr_find_scale_2_0` and
`application_source_pr_search_retry`.

[[BPR-1423]] registered the six rows in `tests/end_to_end/CMakeLists.txt`
with `WINDOWS_UNCONTAINED` (7d56dc1e5, and its WIP 0220258f4) but added no
pins to `scripts/verify_test_network_isolation.py`. [[BPR-1343]]'s check
refuses a `WINDOWS_UNCONTAINED` registration without one, on every platform.

## Acceptance

- AC1: `WINDOWS_UNCONTAINED_TESTS` pins all six rows, beside a comment that
  names [[BPR-1423]] and says why each row is uncontained on Windows and
  whether that was measured.
- AC2: `ctest --preset dev -R test_network_isolation_manifest
  --output-on-failure` passes on macOS, and the documentation gate passes.

## Boundaries

No test, registration or product source changes. Windows has not measured
the rows contained; this card does not claim it has.

## Evidence
