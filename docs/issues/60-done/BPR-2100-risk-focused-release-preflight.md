---
schema: "implement-rfc.card.v1"
id: "BPR-2100"
title: "Discover native release regressions before long qualification"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057", "BPR-2058"]
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3218eadd0e4dc97c20cf7980cadabfab5b9cbbb5","evidence":{"kind":"observation","detail":"Nine mandatory original/representative courses, exact native mode/cache, missing/skipped/narrowed refusal, additive inclusion, collect-all and fresh-final synthetic CTest controls pass."}},{"criterion":"AC2","outcome":"pass","revision":"3218eadd0e4dc97c20cf7980cadabfab5b9cbbb5","evidence":{"kind":"observation","detail":"Real Bash entrypoint harness proves both preflight modes precede long suites and collect both failures; every variant/profile, build failure and skip-tests boundary preserved. Linux-first and physical-host exclusion procedure updated."}},{"criterion":"AC3","outcome":"pass","revision":"3218eadd0e4dc97c20cf7980cadabfab5b9cbbb5","evidence":{"kind":"observation","detail":"35 runner,34 release-source,67 documentation and2 allocation tests pass; docs/board/Bash/diff pass and independent final critical review clear. No full build, benchmark or frozen release intervention; no native PASS reuse enabled."}}]
---

# Risk-focused native release preflight

## Scope

Owner direction, 2026-11-11: improve future releases on main, best effort without
a lengthy benchmark campaign. Leave the current frozen v0.4.2 release alone.
Discover cross-platform failures earlier with short Release and Debug checks,
including the Linux Git cache ownership regression. Prefer independent Linux
where available, otherwise discover Linux failures before extended Windows work.
Investigate evidence reuse without assuming unchanged validity.

## Acceptance

- AC1: An explicit bounded risk preflight runs the original Git cache ownership
  and profile reset courses plus representative core/host/composed/storage and
  containment checks. Missing required rows and narrowed selections refuse.
  Preserve native scheduling, reports, failures and Debug policy.
- AC2: Future Mac release entrypoint completes Release and Debug preflights before
  long suites and stops downstream work after failure. Windows/Linux instructions
  schedule early Linux discovery and preserve physical-host exclusion.
- AC3: Final qualification remains input-bound and fresh; no cross-run preflight
  or old-source pass reuse is enabled. Focused runner/entrypoint and documentation
  tests pass without full product builds or current-release interference.

## Completed implementation and verification

Source `3218eadd0e4dc97c20cf7980cadabfab5b9cbbb5` adds `risk-preflight` to the
existing runner. Its nine mandatory rows include the full original Git cache
ownership and profile-reset courses, retained SQLite state, the native isolation
manifest, a bounded composed Overview path, inline drawer, core diff and host
controls. The full workbench omnibus stays in final qualification rather than
being repeated as an early mixed Debug timing gate. Missing, skipped or narrowed
baseline checks cannot pass; additional named checks are additive only. Actual
Release/Debug manifest modes and their generated caches must agree.

The future Mac entrypoint runs both short modes serially after compilation,
retains both results and starts neither long suite if one fails. Existing final
normal/comprehensive, Debug residual/timing omissions, heavy obligations,
BPR-2057 phase diagnostics and BPR-2058 failure-first behavior remain intact.
Selected final rows run fresh; no cross-run native evidence import was added.
The procedure prefers an independently verified Ubuntu 26.04 host if available;
otherwise it places WSL discovery before extended Windows work under the shared
physical-host lease. No independent Linux producer was present in the available
environment catalog, and none was provisioned or dispatched. Broader evidence
reuse remains unready because its transitive native/configuration/input closure
has not been verified.

Focused verification:

- `python3 tests/scripts/test_suite_runner_tests.py`: 35 tests pass, including
  real miniature Release and Debug CTest graphs, collect-all failure/skip
  controls, additive inclusion, cache-mode mismatch and fresh final execution.
- `python3 tests/scripts/release_source_assets_tests.py`: 34 tests pass,
  including all variant/profile orders, both preflight failure boundaries,
  final lane-specific ordering hints and skip-tests preservation.
- `python3 tests/scripts/verify_documentation_tests.py`: 67 tests pass.
- `python3 tests/scripts/allocate_issue_id_tests.py`: 2 tests pass.
- Documentation gate, scoped board validation, Bash syntax and `git diff --check`
  pass. Independent critical review cleared the final source and operational
  timing-only continuation without changing its acceptance boundaries.

No full product build, native qualification or timing campaign was run, so no
measured speedup or cross-platform runtime acceptance is claimed. The current
frozen v0.4.2 source, controllers, outputs and running jobs were not changed.
