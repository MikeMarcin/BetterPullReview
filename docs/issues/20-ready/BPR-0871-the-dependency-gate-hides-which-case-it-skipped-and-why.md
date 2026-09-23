---
schema: "implement-rfc.card.v1"
id: "BPR-0871"
title: "dependency_modernization_contract reports a skip without naming the case or the reason"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win"]
metadata: {"found_by":"bpr-linux, in two successive full Linux sweeps","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0871: dependency_modernization_contract reports a skip without naming the case or the reason

## Defect

`dependency_modernization_contract` passes, and one of its twenty-six cases
silently does not run. Its whole output is:

```
....................s....
Ran 26 tests in 84.984s

OK (skipped=1)
```

The `s` and the count are the only evidence a case was skipped. Neither the
case nor the reason appears anywhere in the run, so a reader of a green sweep
cannot tell which guarantee was not checked — and a second case skipping later,
for a worse reason, would look exactly the same.

The skipped case is
`test_syntax_prefix_installs_one_library_per_configuration`, and its reason is
"fewer than two installed KSyntaxHighlighting configurations: not a
multi-configuration tree". That is legitimate on a single-config Ninja tree.
It was read out of the source by the Linux station, **not** out of the run,
which is exactly the gap.

The row is not cheap — 85 s on Linux, 23 s on macOS — so it is not a row people
re-run casually to find out what it did.

This is an observability gap, not a failure. It surfaced only because two
successive full sweeps recorded the row's output rather than just its verdict:
the earlier Linux sweep recorded "Pass, 64.48 s" and nothing more.

## Scope

- Make the run name what it skipped and why: raise the unittest runner's
  verbosity, or print each skip's case and reason after the run. Whichever is
  cheaper, the requirement is that the row's own output answers "which
  guarantee did this not check, and why".
- Check the other Python contract rows for the same shape. A row that can skip
  silently is worth finding before it does.
- Consider whether a skip should be visible in the sweep summary too, since
  the value here is for someone reading a whole sweep rather than one row.

Do not make the skip an error: single-config trees are the normal case and the
guarantee genuinely does not apply there.

## Acceptance

- The row's output names every skipped case and its reason, shown in a real
  run.
- A second skip appearing for a different reason is distinguishable from this
  one in that output.
- The row still passes on macOS, Linux and Windows, and still skips rather
  than fails on a single-configuration tree.

## Linux done at `867ed35f`; macOS and Windows still owed (bpr-main-linux, 2026-09-23)

**Decision: the card stays open for two confirmation runs.** The third
acceptance line names macOS and Windows, and neither station has run the
candidate. Windows matters most: it is the only station where the
per-configuration case actually runs, and this change moved where the case
looks. So the card goes back to Ready with `platforms: ["mac", "win"]`, so
that those two queues offer it. Linux owes nothing more.

### What changed (`867ed35f`)

**The rows name what they skipped.** `tests/scripts/skip_report.py` is a
unittest runner. Before unittest's summary, it prints one line for each
skipped case, with its reason, and one for each expected failure. The
dependency row now ends:

```
.....................s.....
======================================================================
1 case was not checked (CTEST_FULL_OUTPUT keeps this list in CTest's report):
SKIPPED: dependency_modernization_tests.DependencyModernizationTests.test_syntax_prefix_installs_one_library_per_configuration: one installed KSyntaxHighlighting configuration, DEBUG, in /home/mmarcin/BetterPullReview/build/dev/_bpr/ksyntaxhighlighting/6.28.1/prefix/lib/x86_64-linux-gnu/cmake/KF6SyntaxHighlighting: a single-configuration tree has nothing to compare
----------------------------------------------------------------------
Ran 27 tests in 65.680s

OK (skipped=1)
```

Each line names the script rather than `__main__`, so one `grep` over a
sweep's log lists every unchecked case and the row it came from.

Scope offered a cheaper option: raise unittest's verbosity. It was not used,
for two reasons:

- A verbose line can be split by output the case itself writes, so the case
  and its reason end up on different lines.
- CTest keeps only the first KiB of a passing row's output in its JUnit
  report. This was measured on CMake 4.4.2 in a scratch project. A passing
  test's 5 KB of output was cut to its first KiB. The same output was kept
  whole once it contained `CTEST_FULL_OUTPUT`, as
  `CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE` documents. A long verbose row
  would lose its later skips.

So the report prints `CTEST_FULL_OUTPUT` whenever it has something to name.
`rfc0016_device_live_runner_security`'s 2.4 KB of output is kept whole in
JUnit because of it.

**The case's own reason was wrong on Linux.** This is a new finding. The case
globbed `_bpr/ksyntaxhighlighting/*/prefix/lib/cmake/KF6SyntaxHighlighting/`.
A Linux install puts the exports under `prefix/lib/x86_64-linux-gnu/cmake/`,
which is where `KF6SyntaxHighlighting_DIR` in `build/dev/CMakeCache.txt`
points. So on Linux the case found no export at all. It then reported fewer
than two configurations, which was true, and "not a multi-configuration tree",
which was only true by luck. On a real two-configuration Linux tree (Ninja
Multi-Config, `Debug;Release`, configured under `/tmp`), `7d03de48` skipped
with the same message: "fewer than two installed KSyntaxHighlighting
configurations: not a multi-configuration tree". That is the "second case
skipping later, for a worse reason" the defect warned about, and its message
was identical.

The case now reads the directory `find_package` used from the tree's cache.
Each outcome has its own message:

| What the case finds | Outcome |
|---|---|
| No `KF6SyntaxHighlighting_DIR` in the cache (core-only), or a `-NOTFOUND` value | Skip: "this tree does not build KSyntaxHighlighting: its cache has no KF6SyntaxHighlighting_DIR" (or names the value) |
| One export | Skip: "one installed KSyntaxHighlighting configuration, DEBUG, in `<dir>`: a single-configuration tree has nothing to compare" |
| Two or more exports | Runs. Fails if two configurations name the same library |
| No export in the cached directory | Fails: "no per-configuration export in `<dir>`, the directory the tree found KSyntaxHighlighting in" |

**The other Python rows (Scope 2).** Sixteen unittest scripts under `tests/`
can skip a case, counting the new contract. All sixteen now use the runner.
The new row `skip_report_contract` keeps it that way. It fails when a script
under `tests/` contains a skip or expected failure and calls `unittest.main`
without `testRunner=SkipReportingTestRunner`. It has already caught one:
[[BPR-1027]] added `cfg_complete_link_tests.py` after the survey and before
the commit. The scan reads each script's own text, so it does not see a skip
raised in an imported helper; none exists in `tests/` today.

| Row, or script | Cases skipped on this station |
|---|---|
| `rfc0016_device_live_runner_security` | 12: "the attended live runner refuses every platform but macOS" |
| `dependency_modernization_contract` | 1: the case above |
| `coding_standard_format_gate_contract` | 1: `clang-format` on `PATH` is 21.1.8 ([[BPR-0865]]) |
| `runtime_audit_contract` | 1: "requires macOS libproc" |
| `test_network_isolation_structure_contract` | 1: the Mach-O symbol probe uses macOS's `/usr/bin/nm` |
| `issue_id_allocation_contract`, `cfg_complete_link_contract`, `contained_temporary_support_contract`, `dif_assets_contract`, `package_runtime_contract`, `release_source_assets_contract`, `rfc0016_device_capture_security`, `rfc0017_public_preview_decision_contract`, `skip_report_contract` | 0. Each has a skip path that does not fire here |
| `designated_live_macos_capsule_tests.py`, a macOS-only row | 2 when run directly here: "hdiutil is macOS-only" |
| `record_rfc0017_anonymous_contracts_tests.py`, not a registered row | 0 |

So a sweep on this station has 16 unchecked cases in five green rows, and
until now it named none of them. The other unittest scripts under `tests/` have no
skip path. The non-unittest rows were checked too: none has a silent skip.
`coding_format_check` exits 77 and names why.

**The sweep summary (Scope 3).** This was considered, and the summary is
deliberately unchanged. CTest has no result for a row that passed but skipped
some cases. A skipped row would report 26 checked cases as not run. It would
also put this row in "The following tests did not run" on every Linux and
macOS sweep, and readers would learn to ignore the list that
`coding_format_check` relies on. Instead, `docs/development/build-and-test.md`
("A passing row can leave cases unchecked") tells whoever reports a sweep to
list the `SKIPPED:` lines along with the rows that did not run, using:

```sh
grep -h -e '^SKIPPED: ' -e '^EXPECTED FAILURE: ' build/dev/Testing/Temporary/LastTest.log
```

The same lines are in the JUnit `system-out`.

### Verification (Linux, JaegAW, WSL2 Ubuntu 26.04, GCC 15.2, CMake 4.4.2; rows run under Python 3.13.15)

| Check | Candidate | Result |
|---|---|---|
| **Before:** `ctest --preset dev -R '^dependency_modernization_contract$' -V --output-junit …` | `7d03de48` | Pass, 27 cases, `OK (skipped=1)`. The case and the reason appear nowhere in the output or in JUnit, and JUnit says `skipped="0"` |
| **After, AC1:** the same row, in a 22-row `ctest --preset dev -j8` pass | `867ed35f` | Pass, 65.7 s. The console, `LastTest.log` and the JUnit `system-out` all carry the `SKIPPED:` line quoted above |
| **AC2 and AC3, a single-configuration tree and another reason:** `ctest --preset core-only -R '^(dependency_modernization_contract\|skip_report_contract\|test_network_isolation_manifest\|test_network_isolation_structure_contract)$'` | `867ed35f` | 4/4 pass. The dependency row skips the same case for a different reason, and names it: "this tree does not build KSyntaxHighlighting: its cache has no KF6SyntaxHighlighting_DIR" |
| The case alone, against the `/tmp` Ninja Multi-Config `Debug;Release` tree | `7d03de48` | `skipped 'fewer than two installed KSyntaxHighlighting configurations: not a multi-configuration tree'`, which is false for that tree |
| The same | `867ed35f` | Runs: `... ok`. DEBUG is `libKF6SyntaxHighlighting_debug.a` and RELEASE is `libKF6SyntaxHighlighting.a` |
| **Negative control:** the same tree with its DEBUG export pointed at the RELEASE library | `867ed35f` | `FAILED`, "configurations share an installed library". The export was restored afterwards |
| **Negative control:** a cache that names an empty package directory | `867ed35f` | `FAILED`, "no per-configuration export in /tmp/bpr0871/empty-package …" |
| `skip_report_contract` directly, under Python 3.13.15 and 3.14.4 | `867ed35f` | 9 cases OK on both. They cover a named skip with its reason, two reasons told apart, an expected failure, a class-level skip naming each case, `CTEST_FULL_OUTPUT` present only when something is named, the script name in place of `__main__`, `unittest.main` building the runner from the class, and the scan |
| **Negative control:** the runner with its result class removed | working tree | 6 of 9 fail. Restored |
| **Negative control:** the dependency row's `testRunner=` removed, and a `skipTest` planted in `type_read_binding_tests.py` | working tree | The scan fails and names both, `tests/scripts/dependency_modernization_tests.py:1092` and `tests/scripts/type_read_binding_tests.py:64`. Restored |
| `ctest --preset dev -j8` over 22 rows: every changed row, `skip_report_contract`, `test_network_isolation_manifest`, `test_network_isolation_structure_contract`, `contained_temporary_usage_contract`, `platform_configuration_contract`, `rfc0016_test_inventory`, `rfc0017_test_inventory`, `rfc0017_test_inventory_contract`, `documentation_structure`, `documentation_issue_graph_contract` | `867ed35f` | 22/22 pass. The `grep` above lists 16 cases in five rows |
| `designated_live_macos_capsule_tests.py` and `record_rfc0017_anonymous_contracts_tests.py`, run directly because neither is a Linux row | these paths as committed in `867ed35f` | 49 cases, `OK (skipped=2)`, both named; 166 cases, OK |
| `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py`; `git diff --check` | `867ed35f` with this card's edits | `DOCUMENTATION_OK`; 64 tests OK; clean |

The runs were made at `c50c121e`. That is the same commit before it was
rebased onto `7dfece4f` for the push, and it is not on `main`. The rebase
brought in only issue cards: `git diff --name-only c50c121e 867ed35f` lists
six files, all under `docs/issues/`. So the results hold for `867ed35f`.

The floor interpreter, 3.10, is not installed on this station, so the runner
was not run under it. It uses nothing that 3.10's `unittest` lacks.

No composition-contract pin is touched: the change is confined to
`tests/scripts/`, one registration in `tests/CMakeLists.txt`, and
`docs/development/build-and-test.md`. `build/dev` needed no rebuild.

The logs are in `build/r2-logs/bpr0871-*` on this station and are not
checked in.

### Acceptance, as it stands

- **AC1**, the output names every skipped case and its reason in a real run:
  pass on Linux at `867ed35f`.
- **AC2**, a second skip for a different reason can be told apart: pass on
  Linux at `867ed35f`. `dev` and `core-only` give the same case different,
  named reasons. `skip_report_contract` pins two reasons in one run.
- **AC3**, the row passes on all three platforms and still skips on a
  single-configuration tree: Linux passes, and skips on both `dev` and
  `core-only`. macOS and Windows are owed.

### Still owed

**bpr-main-win.** `windows-dev` uses Visual Studio with `Debug;Release`, so
this is where the case runs. Reconfigure `windows-dev` at `867ed35f` or later
so that `skip_report_contract` is registered, then run:

```powershell
ctest --preset windows-dev -j1 -V -R "^(dependency_modernization_contract|skip_report_contract|coding_standard_format_gate_contract)$"
```

Expect:

- All three rows pass.
- `dependency_modernization_contract` prints **no**
  `SKIPPED: dependency_modernization_tests.DependencyModernizationTests.test_syntax_prefix_installs_one_library_per_configuration`
  line, because the case ran and compared DEBUG with RELEASE. If it prints
  one, record the reason: it would be a regression from this change.
- `coding_standard_format_gate_contract` names its three discovery cases, one
  `SKIPPED:` line each, with the reason "find_program on Windows looks for
  .com and .exe files, and these stubs are batch files".
- `skip_report_contract` passes contained. It is registered without
  `WINDOWS_UNCONTAINED`. If it fails contained, record the first failure.

**Mac integrator.** Run:

```sh
ctest --preset dev -V -R '^(dependency_modernization_contract|skip_report_contract)$'
```

Expect both rows to pass under the sandbox runner. The dependency row should
print `SKIPPED: dependency_modernization_tests.DependencyModernizationTests.test_syntax_prefix_installs_one_library_per_configuration: one installed KSyntaxHighlighting configuration, DEBUG, in <the cache's KF6SyntaxHighlighting_DIR>: a single-configuration tree has nothing to compare`.

If both stations observe this, the card can finish with the records above.
