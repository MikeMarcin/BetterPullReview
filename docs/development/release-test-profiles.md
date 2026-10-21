# Normal and comprehensive release qualification

The owner's incremental-release amendment to `release-30min-portfolio-r1`
permits a fast baseline plus focused patch regressions. Major releases and broad,
unclassified, Qt, toolchain, ABI or platform changes request comprehensive native
qualification. The existing runner performs selection; there is no new workflow
engine and no cross-request verdict reuse.

A normal PASS means **only the selected checks passed**. It does not qualify an
omitted heavy claim. In particular, BPR-1653's strict Ubuntu RSS failure remains a
release blocker; the exception for published v0.3.1 is not extended. Signing,
notarization, native trust/installation, legal/source approval, M8/M9, vault,
crash/privacy and vulnerability obligations retain their separate boundaries.

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
`--test-profile comprehensive`; missing scope escalates to comprehensive.
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
| `resource-budget` | Strict RSS/cache, 32 epochs, large payload, scale and performance workloads | Core/diff/GitHub hosts and ordinary storage behavior; no resource PASS implied |
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
