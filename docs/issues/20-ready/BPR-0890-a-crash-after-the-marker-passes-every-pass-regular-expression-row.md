---
schema: "implement-rfc.card.v1"
id: "BPR-0890"
title: "A crash after the marker is printed passes every row that has a PASS_REGULAR_EXPRESSION"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable src/app and gates partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0890: A crash after the marker is printed passes every row that has a PASS_REGULAR_EXPRESSION

## Defect

CTest ignores exit status when a row has a pass expression —
`cmake/BprTestIsolation.cmake:741-748` records this deliberately. **117 rows
carry one**: 79 in `tests/end_to_end/CMakeLists.txt` and 38 in
`tests/adapters_qt/CMakeLists.txt`.

`report_verdict` prints `PUBLIC_PATH_OK` **before** `QApplication::exec()`
returns, and before `desktop`, `QApplication` and the static destructors run
(`src/app/main.cpp:8735-8741`). So an abort anywhere in that tail is invisible
to CTest: the marker is already on stdout and the row is green.

That tail is not hypothetical. `github_application_liveness_tests.cpp:84-92`
documents exactly this class of shutdown fault for [[BPR-0215]], [[BPR-0217]]
and [[BPR-0827]].

[[BPR-0552]] chose the pass-expression trade-off knowingly, but did not close
this gap. It is worth closing now because this dispatch has just spent a great
deal of effort on shutdown behaviour — [[BPR-0875]]'s thirteen-row Windows
cleanup cluster is precisely a family of shutdown faults, and a green row is
currently no evidence that shutdown succeeded.

## Scope

- Pair the marker with the child's exit status.
  `scripts/run_network_isolated_test.py:117-136` already has the child's code
  and could print `TEST_RUNNER_CHILD_EXIT:<n>`, which a second
  `FAIL_REGULAR_EXPRESSION` entry can catch. That is the macOS launcher; the
  Linux `execvp` launcher
  (`tests/network_isolation/linux_network_sandbox.cpp:126`) and the Windows
  launcher need the same, or the gap just moves platform.
- Whatever shape is chosen, it has to compose with [[BPR-0875]]'s work on the
  non-append `FAIL_REGULAR_EXPRESSION` property, and with the existing
  `bpr_expects_failed_run` rows that are supposed to fail.
- Then check whether any currently green row is hiding a non-zero exit. That
  is the interesting part, and it should be done before anything is
  celebrated as fixed.

## Acceptance

- A row whose application prints its marker and then aborts fails, shown by a
  deliberate abort in the shutdown tail.
- The existing expected-failure rows still behave.
- The card records how many previously green rows were hiding a non-zero exit,
  on each platform.

## The fail-closed verdict is not pinned on ordinary rows either

From the same review, and the same machinery, so it belongs here rather than
in its own card.

`cmake/BprTestIsolation.cmake:770-773` sets
`FAIL_REGULAR_EXPRESSION "PUBLIC_PATH_FAILED"` with a **non-append**
`set_property`. Any later row-level
`set_tests_properties(... FAIL_REGULAR_EXPRESSION ...)` silently replaces it.

The gate does not catch that. `scripts/verify_test_network_isolation.py:641-659`
checks the property only for the fourteen `EXIT_VERDICT_TESTS`, and
`configured_failures` (`:903-1003`) never asserts its **presence** for a
`bpr_network_isolated` row that lacks the `bpr_expects_failed_run` label.

Today no row overrides it — grep confirms — so nothing is broken. But the
comment at `BprTestIsolation.cmake:753-756` claims "a row added later inherits
it and the guarantee cannot drift", and nothing enforces that claim. The
reviewer verified the gap directly: adding `FAIL_REGULAR_EXPRESSION "x"` to one
row and running `ctest --show-only=json-v1` still passes
`test_network_isolation_manifest`.

**Deliberately not taken as a quick fix in dsp-005.** The check itself is a few
lines in `configured_failures`, but it has to get the live lane, the
`bpr_expects_failed_run` rows and any legitimate exception right, and an
assertion that is wrong in the strict direction turns the manifest gate red for
every platform at once. It wants the same pass as this card's exit-status work,
which touches the same property on the same rows.

Additional acceptance: a row that replaces the inherited
`FAIL_REGULAR_EXPRESSION` fails `test_network_isolation_manifest`, shown by
adding one deliberately.
