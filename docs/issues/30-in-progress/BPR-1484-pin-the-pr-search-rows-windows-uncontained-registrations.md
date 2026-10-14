---
schema: "implement-rfc.card.v1"
id: "BPR-1484"
title: "Pin the PR Search rows' Windows-uncontained registrations"
status: "In Progress"
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

- Cause: [[BPR-1423]]'s 7d56dc1e5 (and its WIP 0220258f4) registered the six
  rows `WINDOWS_UNCONTAINED` in `tests/end_to_end/CMakeLists.txt` with no
  pins. `pin_registration_failures` ([[BPR-1343]]) refuses that on every
  platform.
- AC1: 358098090 pins all six rows `dos-volume-name` beside a comment naming
  [[BPR-1423]]. Every course (`source_pr_find_public_path_tests.cpp`,
  `source_pr_search_scope_path_tests.cpp`,
  `source_pr_search_performance_tests.cpp`) builds its data root with
  `e2e::private_data_root` first. `application_overview_double_click` starts
  the same way, and [[BPR-1348]] measured it contained on Windows: it fails
  "private composed course root" before any course step. So these rows are
  pinned by that shape, and the comment says they are not yet measured
  contained on Windows. `application_source_pr_search_retry` is registered
  only where `UNIX` is true, so Windows never configures it, and the comment
  says so. The sibling native Find rows (`application_source_find*`) keep
  their `unmeasured (registered on macOS)` pins; this card does not touch
  them.
- AC2: macOS, `build/dev`, with the gh CLI transport test executable built
  (`test_better_pull_review_gh_cli_git_smart_transport_tests`). Without the
  pins (HEAD 8bbe0b1a2's script on the same build),
  `test_network_isolation_manifest` fails with exactly the six
  `TEST_NETWORK_ISOLATION_FAILED:application_source_pr_*` lines. With them
  it passes. After merging main (977118d9c, which brings in BPR-1478) and
  reconfiguring, `ctest --preset dev -R
  '^test_network_isolation_(manifest|structure_contract)$'
  --output-on-failure` passes 2 of 2, and `python3
  scripts/verify_documentation.py --root .` prints `DOCUMENTATION_OK`.
