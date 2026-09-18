---
schema: "implement-rfc.card.v1"
id: "BPR-0890"
title: "A crash after the marker is printed passes every row that has a PASS_REGULAR_EXPRESSION"
status: "In Progress"
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

## Findings, dsp-006 (macOS)

### The measurement, first

Every one of the **208** rows this build configures with a
`PASS_REGULAR_EXPRESSION` was run with the macOS launcher printing the child's
exit status, and every one of them was measured — 64 in the full-suite sweep
(`build/r2-logs/dsp6-0890-sweep.log`, 382 rows, 1584 s) and the remaining 144
in a follow-up run of exactly the rows the first sweep had not marked
(`build/r2-logs/dsp6-0890-rerun.log`, 424 s). No pass-expression row was left
unmeasured.

**One green row was hiding a non-zero exit, and it was hiding it by design:**

- `rfc0017_release_absence_negative_control` — green, child exit **1**. It runs
  the shipped-artifact scan against the tests-enabled binary, where the scan
  *must* fail; the row pins the refusal text and the scanner exits 1. Nothing
  anywhere said so. It now says so at the registration, with
  `EXPECTS_NONZERO_EXIT`.

One further green row exits non-zero and was already declared:
`application_automated_run_budget` (exit 1, `bpr_expects_failed_run`).

Four rows exited non-zero and were **already red**, so they hid nothing:
`application_large_review_cache_bytes`, `application_large_review_loaded_close`
and `application_large_review_resources` ([[BPR-0893]]'s known load-sensitive
footprint rows, and this sweep ran six-wide), and
`application_overview_since_review_contract`. No row died of a signal.

So on macOS the shutdown tail is clean today: **zero green rows hiding an
undeclared fault**. That is a measured zero rather than an assumed one, and it
is what makes the guard below a regression net rather than a bug fix.

### The mechanism

`scripts/run_network_isolated_test.py` prints `TEST_RUNNER_CHILD_EXIT:<n>`
after every run — the evidence, in every log — and
`TEST_RUNNER_CHILD_EXIT_FAILED:<n>` only when that code is non-zero, which is
what `bpr_add_network_isolated_test` now matches as a second
`FAIL_REGULAR_EXPRESSION` entry. A literal rather than a negated digit class,
because CTest's regular expressions have no lookahead. A negative value is
death by that signal.

Both entries are written with `APPEND`, because `set_property(... PROPERTY
...)` replaces the whole property and a second non-append write would have
silently deleted the first — the hazard this card folds in, met from the inside.

Two exemptions, each written at the row rather than inferred:
`EXPECTS_FAILED_RUN` (unchanged: the row whose subject is a failed run, which
inherits neither entry) and the narrower `EXPECTS_NONZERO_EXIT` (the row whose
child exits non-zero as the answer it asserts, which keeps `PUBLIC_PATH_FAILED`
and drops only the exit entry). The two keywords refuse to be combined.

Configured result: 379 rows carry both entries, 1 carries the run verdict
alone, 2 carry neither — `application_automated_run_budget` and the enumerated
macOS-unsandboxed row `gh_cli_git_smart_transport`, which is registered outside
the seam and is now held to the stronger rule instead (no pass expression at
all, so its exit status is the only thing that decides it).

### Evidence

- **A row that prints its marker and then aborts now fails.** A row registered
  through the real seam, whose child printed `PUBLIC_PATH_OK` and then raised
  `SIGABRT`: `***Failed  Error regular expression found in output.
  Regex=[TEST_RUNNER_CHILD_EXIT_FAILED]`, child exit -6
  (`build/r2-logs/dsp6-0890-proof.log`). The same fixture under the property
  set this card starts from **passes**
  (`build/r2-logs/dsp6-0890-ctest-semantics.log`), and a clean exit still
  passes, so the difference is the guard and not the fixture.
- **The gate now catches a replaced property.** A deliberate row-level
  `set_tests_properties(application_public_path PROPERTIES
  FAIL_REGULAR_EXPRESSION "NEVER_PRINTED_SENTINEL")` turns
  `test_network_isolation_manifest` red with
  `TEST_NETWORK_ISOLATION_FAILED:application_public_path must carry the
  inherited fail-closed verdict ['PUBLIC_PATH_FAILED',
  'TEST_RUNNER_CHILD_EXIT_FAILED']; found ['NEVER_PRINTED_SENTINEL']`. The
  reviewer's original demonstration — add an override, gate still green — no
  longer reproduces.
- The expected-failure rows still behave: `application_automated_run_budget`
  and `rfc0017_release_absence_negative_control` both pass, alongside the gate
  rows and a sample of ordinary rows
  (`build/r2-logs/dsp6-0890-final-verify.log`, 12/12).

### Not closed here

The acceptance line "on each platform" is answered for macOS only. The Linux
launcher `execvp`s, so it has no exit status to state and must become a
supervising parent before it can; the Windows launcher already waits on the
child and only needs to print, but its `WINDOWS_UNCONTAINED` rows run with no
launcher at all and stay unguarded — which is exactly [[BPR-0875]]'s cluster.
Both platforms already carry the property, so the launcher is the only thing
left to change on each.
