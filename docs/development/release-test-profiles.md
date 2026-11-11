# Normal and comprehensive release qualification

The owner's incremental-release amendment to `release-30min-portfolio-r1`
permits a fast baseline plus focused patch regressions. Major releases and broad,
unclassified, Qt, toolchain, ABI or platform changes request comprehensive native
qualification. The existing runner performs selection; there is no new workflow
engine and no cross-request verdict reuse.

A normal PASS means **only the selected checks passed**. It does not qualify an
omitted heavy claim. BPR-1653 preserves the original strict Ubuntu RSS failure
and a separately owner-authorized 240 MiB RSS criterion, with independent liveheap
216 MiB and complete current-source course acceptance. Normal alone supplies no
resource evidence; the exception for published v0.3.1 is not extended. Signing,
notarization, native trust/installation, legal/source approval, M8/M9, vault,
crash/privacy and vulnerability obligations retain their separate boundaries.

## Standing Debug performance policy

On 2026-10-26 the owner made Debug performance timings permanently nongating:
"From now and until the end of time I won't care about debug timings, we dont
even have to run those in debug." This supersedes earlier requirements to accept
Debug timing budgets, including comprehensive-release descriptions. It does not
expire with a release and does not require another waiver when a Debug timing
assertion recurs.

Use Release for performance acceptance. Dedicated performance courses may be
omitted in Debug. The current explicit omission set is:

- `application_large_review_history`
- `application_large_review_launch`
- `application_large_review_responsiveness`
- `application_large_review_active_work`
- `application_large_review_pathological`
- `application_large_review_prepared_open`
- `core_diff_compute_benchmark`
- `application_source_pr_search_performance`

The existing runner supports that bounded selection without changing test bodies:

```sh
python3 scripts/run_test_suite.py --preset dev --profile comprehensive --change-scope ownership --tests-regex '^(?!(application_large_review_(history|launch|responsiveness|active_work|pathological|prepared_open)|core_diff_compute_benchmark|application_source_pr_search_performance)$).*'
```

Use `windows-dev` on Windows. `--tests-regex` filters the validated manifest with
Python's regular expression syntax; the runner constructs CTest's positive row
selection itself. Tooling preflight still runs. Preserve `selection.json`, its
exact configured/selected/omitted names and actual results. This is comprehensive
functional selection with declared Debug performance omissions, not an all-graph
Debug PASS. New rows remain selected unless explicitly classified and omitted;
there is no duration or directory heuristic. A performance course's functional
preconditions do not receive Debug coverage credit when the course is omitted;
its complete Release course and separate Debug functional controls retain their
own identities.

Mixed functional courses can contain timing-sensitive assertions. Keep their
actual failures and any unreached checks, classify the exact timing predicate,
and continue without a performance repair, rerun or approval request. A short
watchdog that cuts off slow Debug functional work may be increased for one
bounded semantic completion course, preserving the original deadline failure and
the exact command/environment change. Actual hangs and functional failures are
not timing acceptance. Optimized Release timings, memory/resource acceptance,
compilation, artifacts/source/legal, signing, notarization, security and native
trust remain separate obligations.

## Early native risk preflight

Before long qualification, run the explicit `risk-preflight` profile on the
candidate's tests-enabled Release **and** Debug graphs on each selected native
platform. It is early regression discovery, not normal or comprehensive
qualification. Existing tooling prerequisites still run fresh. No test body,
watchdog, Release performance limit or native containment rule changes.

The nine mandatory rows in `tests/release_test_profiles.json` cover:

- `git_cache_contract`: the original full cache course, including persisted
  artifact lifetime/foreign-replacement ownership (BPR-2095), not a new smoke
  assertion that can miss the regression.
- `application_profile_data_reset_public_path`: the composed reset/reopen,
  kept-store and removed-file lifetime course (BPR-2099).
- `sqlite_epoch_catalog_behavior_contract`: ordinary retained-state storage.
- `test_network_isolation_manifest`: configured native containment declarations.
- `qt_workbench_overview`: five bounded composed Overview/navigation cases,
  rather than repeating the full workbench omnibus and its mixed 100 ms drawer
  assertion before the long suites.
- `qt_composed_inline_drawer_scale_100`, `core_diff_compute`,
  `application_diff_host` and `application_github_host`: representative
  composed interaction, deterministic diff and host lifecycle checks.

This small row set omits dedicated performance campaigns, real-time expiry and
large-review resource courses. It retains original per-course watchdogs; it is
not a promised wall-time budget. New risk-specific cheap public paths can be
added with `--include-tests-regex`, without removing the mandatory baseline.
Do not add dedicated Debug timing courses. Missing baseline rows, unsupported or
mismatched build configurations, `--tests-regex`, prior-failure ordering and
scope/heavy-group classification are refused. Every required row must execute;
a skipped check does not produce a passing preflight. Keep actual mixed Debug
functional/timing failures and apply the standing Debug policy above, without
turning a timing disposition into a functional pass.

After the physical-host compilation prerequisites, use fresh output directories:

```sh
python3 scripts/run_test_suite.py --preset qualification-release --profile risk-preflight --jobs 4 --output-dir /absolute/new-preflight-release
python3 scripts/run_test_suite.py --preset dev --profile risk-preflight --jobs 4 --output-dir /absolute/new-preflight-debug
```

On Windows use `windows-qualification-release` and `windows-dev` with the
approved native Python. `tools/release.sh` runs both preflights automatically
before either long Mac qualification lane. Raw manifest, selection, JUnit,
timing and phase-ledger results remain separate from the final suites. The Mac
entrypoint collects both mode results even after a Release failure, then stops
before long suites if either failed. A failure blocks unstarted downstream work;
retain all failures in that attempt, reconcile the candidate and fix functional
failures before redispatch. Do not replace an in-flight release's frozen controller.

If a mixed Debug row ever fails only a performance predicate, retain the failed
row and its unreached checks, identify the exact predicate and apply the standing
nongating policy without requesting another timing waiver. Continue final Release
and Debug functional qualification using the direct runner commands in
[Select the route](#select-the-route), with the same candidate, native graphs,
scopes, focused regressions, heavy obligations and existing request ledger.
Those runs execute fresh; the preflight report is not a substituted receipt.
Do not repeatedly invoke the failed producer just to turn the timing row green.
Packaging reentry retains the existing prepare-only/skip-tests policy and its
explicit limitation acceptance requirement for publication; a timing disposition
neither authorizes skipping final functional qualification nor supplies a package,
signing or publication approval.

Final qualification still uses the selected normal/comprehensive and Debug
policy below, including focused patch regressions and relevant heavy groups.
Every selected final row runs fresh. A preflight result supplies no replacement
for one of those rows, no omitted-heavy claim and no cross-source acceptance.
Rows omitted by the final profile remain separately reported early checks, not
comprehensive Debug coverage. Prior full-suite failures still use the independent
`--failures-first` mechanism on the final suite.

## Select the route

Use the tests-enabled Release preset (`qualification-release` on macOS/Linux,
`windows-qualification-release` on Windows). Build it normally, then run:

```sh
python3 scripts/run_test_suite.py --preset qualification-release --profile normal --change-scope incremental --include-tests-regex '^(release_update_.*|native_https_.*|qt_release_maintenance_scale_.*|application_release_maintenance_public_path)$'
```

This keeps the normal baseline and adds the named patch regressions. It never
turns a regression expression into a replacement for the baseline. The current
incremental scope covers updater/HTTP/UI work; changed runner/scheduler and
fixture work also gets its focused controls and native scheduling proof.

Run the native Debug diagnostic lane using `--profile debug-residual` and the
same declared scope/regressions. It always selects the five workbench/drawer,
core-diff and diff/GitHub-host commands. Their unconditional expectations,
6,000-line Debug workloads, Qt assertions and checked MSVC STL remain distinct
from Release's larger workloads, four 100 ms assertions and allocation sweeps.
Five Debug commands do not cover every Qt DLL/CRT/STL diagnostic path.

A declared `device`, `diagnostics`, `ui`, `storage`, `providers`, `tooling` or
`process` scope adds its named heavy owners. Additional `--heavy-group ID`
arguments request more obligations. An `incremental` scope requires explicit
patch regressions. A `routine` scope is only for known metadata/version/docs
changes or unchanged production inputs with separate focused qualification proof.
These are operator declarations, not an inferred complete input closure. Do not
label a broad or unknown change routine. Missing scope is refused; `unknown`,
`broad`, `qt`, `toolchain`, `platform` and `ownership` escalate to comprehensive.

```sh
python3 scripts/run_test_suite.py --preset qualification-release --profile heavy --heavy-group device-lifecycle
python3 scripts/run_test_suite.py --preset qualification-release --profile comprehensive
python3 scripts/run_test_suite.py --preset dev --profile comprehensive
```

A major release uses comprehensive Release and Debug on the affected native
platforms. The generic runner defaults to comprehensive for existing callers.
`tools/release.sh` explicitly defaults its qualification to normal and accepts
`--test-scope`, `--test-regressions`, `--test-heavy-group` and
`--test-profile comprehensive`; missing scope refuses before builds or credential lookup and prints exact normal/comprehensive invocations.
Windows/Linux operators use the same runner before the existing package paths.
An unsigned mechanical rehearsal can use already recorded qualification and the
existing skip-tests/prepare-only producer path; it supplies no publication approval.

## Prioritize repaired failures without dropping coverage

For a repaired candidate, pass the previous same-preset `timings.json` to the
existing runner:

```sh
python3 scripts/run_test_suite.py --preset qualification-release --profile comprehensive --failures-first /absolute/previous/qualification-release/timings.json
```

The runner keeps its collect-all behavior. After the ordinary fresh tooling
preflight, it runs the prior failing tests and their entire fixture/dependency
components, then all remaining selected tests with normal CTest scheduling.
It continues the remainder even if an early failure repeats, retaining all current
failures for the next repair. There is no circular rotation, first-failure stop,
new concurrency policy or previous-source PASS reuse. The report is only an
ordering hint; new or previously unrun rows stay in the normal remainder.

`selection.json` records the prior report path/digest, failed names and both
batches. Both batches retain native locks, serial declarations and fixture setup/
cleanup. A connected component cannot cross batches. Unknown/renamed or currently
excluded prior failures are refused: reconcile the change or explicitly include
the current obligation, rather than silently hiding it behind a narrower profile.
A report for another preset is refused. An empty prior failure set uses the normal
single invocation.

Each batch writes separate raw CTest/JUnit files, linked by `batches.json` and
`timings.json`. Exact row-name files select the batches without expanding large
native inventories past Windows command-line limits. The aggregate JUnit/timing inventory still requires every selected
row exactly once (apart from the existing, separately identified same-invocation
tooling preflight credit). Missing/malformed results never become passing proof.
A missing batch report does not prevent collecting the independent remainder;
the aggregate remains failed/incomplete. Logs are not parsed for scheduling.

The Mac release entrypoint forwards `--test-failures-first /absolute/release/timings.json`
and `--debug-failures-first /absolute/debug/timings.json` independently. Keep the
same explicit normal/comprehensive scope selection and Debug policy. Windows and
Linux use the runner directly. Apply this to future repair work only; do not
replace a running release's frozen scripts or controllers.

## Coverage and heavy obligations

[The exact row inventory](../../tests/release_test_profiles.json) is authoritative.
Unknown/new rows run fresh in normal; they cannot disappear through a directory
or time-threshold heuristic. Every run retains the complete configured manifest,
`selection.json`, declared change scope, focused additions, selected rows, omitted
rows and their claims/triggers. Missing/duplicate/unexpected results fail the
selected qualification. Tooling bootstrap still executes in every invocation.
A later heavy request executes fresh; a normal PASS supplies no heavy receipt.

| Heavy group | Retained claim | Normal counterpart |
|---|---|---|
| `device-lifecycle` | Real polling/backoff, grant/vault, restart and account-mismatch lifetimes | Core device policy, controlled-clock refresh host, transport contracts and public smoke |
| `diagnostic-age` | Original real 180.1-second expiry and backend maintenance | Diagnostic controls, generation/privacy barriers, ordinary logger and content policy |
| `resource-budget` | Strict RSS/cache, 32 epochs, large payload, scale and performance workloads; separately amended Ubuntu/WSL RSS 240 MiB and heap 216 MiB | Core/diff/GitHub hosts and ordinary storage behavior; no resource PASS implied |
| `gui-matrix` | Additional scale/theme/render/focus/geometry/shortcut occurrences | Full 115-case workbench default at one scale plus representative surfaces |
| `composed-lifecycle` | Long public paths, repeated restarts, restoration and lifecycle matrices | Public/composed smoke and application-host cases |
| `tool-adversarial` | Hostile checker/asset/dependency corpora and generic tests-off image | Current-source/dependency/security checks and seven source/compiler composition methods |
| `process-recovery` | Death, crash, native credential lifecycle, overload and reap courses | Ordinary process, logging and storage contracts |

The eight composition methods are partitioned, not discarded or satisfied by a
producer image: `application_composition_source_contract` runs seven methods;
`application_composition_contract` runs the one generic tests-off application
build method. Comprehensive runs each method once. Actual restricted distribution
configuration, corresponding source, SPDX and installed notices stay fresh.

The device renewal omnibus uses recorded local transport, not live GitHub.
Its 21 lifetimes include eight five-second first polls, two sets of three
five-second retries, two roughly one-second renewal armings and 6.5 seconds of
settling: about 78.5 seconds of deliberate waits plus startup/store/drain work.
The 90.35-second observed course is useful comprehensive timer/lifecycle evidence;
the existing fake-clock refresh host tests prove policy without those waits.
The logger age course is also preserved unchanged in heavy mode at the owner's
explicit request. No production clock or retry semantics were modified.
