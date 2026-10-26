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
