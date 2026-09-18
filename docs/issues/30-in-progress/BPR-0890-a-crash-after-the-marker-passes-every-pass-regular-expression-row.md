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

## Windows: the launcher states the exit, and the uncontained set is measured (bpr-win, 2026-09-17)

The tip is `86b049014577bb02ec6952868215b8c1d31a5589`.

### The marker

`tests/network_isolation/windows_network_sandbox.cpp` prints
`TEST_RUNNER_CHILD_EXIT:<n>` after `GetExitCodeProcess`, and
`TEST_RUNNER_CHILD_EXIT_FAILED:<n>` when the status is not zero, then flushes,
before `CloseHandle`. Unsigned `%lu`: a Windows crash arrives as a status
rather than a small number — `0xC0000005` is 3221225477, and this run met
`0xC0000409` once already — and `%d` would print those negative. No CMake or
gate change; the inherited failure expression is on every row already.

### The measured gap: thirteen rows have no launcher, and it is currently empty

A `WINDOWS_UNCONTAINED` row clears the isolation prefix entirely
(`cmake/BprTestIsolation.cmake` ~702), so it runs with no launcher and
therefore carries no `TEST_RUNNER_CHILD_EXIT` marker at all. That set is
[[BPR-0875]]'s thirteen-row cluster — precisely where a shutdown fault would
hide.

**Measured on 2026-09-17 at `86b04901`: every green one of the thirteen exited
0. The one red row is red on both signals.** So the gap is real and, today,
empty.

| Rows | Result |
| --- | --- |
| `application_workbench_composition_contract`, `application_overview_since_review_contract`, `application_review_behaviour_contract`, `application_saved_server_contract`, `application_appearance_theme_customization_contract`, `application_epoch_persistence_github-com`, `application_epoch_persistence_ghes-3_18`, `application_epoch_maintenance_public_path`, `application_appearance_gesture_contract`, `application_appearance_store_fallback_contract` | green, exit 0 |
| `application_uiforetw_selected_diff_contract` | red on both signals: exit 1 **and** its pass expression does not match. [[BPR-0859]]'s capture fault |
| `application_native_notification_attended_contract`, `application_review_round_trip_composition_contract` | green, exit 0, and **judged by the exit status directly** — see below |

**The last two never needed this card's guard.** They carry a
`FAIL_REGULAR_EXPRESSION` but no `PASS_REGULAR_EXPRESSION`, and it is only a
pass expression that makes CTest ignore a non-zero exit. Their verdict is the
exit status, with the failure expression as an additional veto — the stronger
signal, not a weaker one. They are the part of the uncontained set this card's
defect cannot reach.

### The method, so this is re-runnable rather than re-invented

For each row in the set:

1. Take the row's own command, environment and `PASS_REGULAR_EXPRESSION` from
   `ctest --test-dir build/windows-dev --build-config Debug --show-only=json-v1`.
   The registry, never a card's prose: five of these thirteen names lacked
   their `_contract` suffix in a dispatch note, and a regex built from the prose
   quietly matched 8 of 13.
2. Run that command directly, with that environment, from the row's working
   directory.
3. Compare the exit status against whether the pass expression matched. A row
   that matches while exiting non-zero is the defect this card is about.

The driver used is `uncontained_exits.py`, kept with the station's task
scripts; it is thirty lines and the three steps above are all it does.

### When this measurement goes stale

**When a row is newly pinned `WINDOWS_UNCONTAINED`, re-run it.** That is the
only way the set grows, so it is the only trigger needed, and [[BPR-0872]]'s
rule is what moves a row into the set. A result that is empty today says
nothing about a set that has gained a member since.

### Deliberately not built: a no-container launcher mode

Running these rows under the launcher in a "report exit only" mode was
considered and declined. The question it would answer has been answered
without it, so the machinery would be justified only by the possibility that
the answer changes later — and that is better served by re-running the
measurement above than by a permanent second code path on the isolation seam.

### Two Windows defects found by running the contract, both fixed here

- `tests/scripts/verify_test_network_isolation_tests.py` pinned the cause
  vocabulary at five terms, so the contract failed the moment main landed the
  two this station measured. It now names `child-process-denied` and
  `repository-content-outside-checkout`.
- This card's own new row — the one proving CTest fails a marker-printing child
  that exits non-zero — wrote its generated `CTestTestfile.cmake` with a raw
  Windows interpreter path. CMake reads a backslash in a quoted argument as an
  escape, so the file listed no tests and the assertion counting them failed:
  a broken generator wearing a broken assertion's clothes. The interpreter path
  now uses forward slashes, as the generated script path beside it already did.
  This would fail on any Windows station, not only this one.

Both isolation gates pass at this tip: `test_network_isolation_structure_contract`
3.53 s and `test_network_isolation_manifest` 6.48 s.
