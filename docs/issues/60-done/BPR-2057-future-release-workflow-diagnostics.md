---
schema: "implement-rfc.card.v1"
id: "BPR-2057"
title: "Reduce release dependency waits and expose structured build diagnostics"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"087d5f2baa2285a65076313b40be4318d61cec26","evidence":{"kind":"observation","detail":"Release command harness passes every variant/profile, all six failure boundaries and skip-tests; physical-host procedure retains final native gates."}},{"criterion":"AC2","outcome":"pass","revision":"087d5f2baa2285a65076313b40be4318d61cec26","evidence":{"kind":"observation","detail":"29 runner tests pass, covering actual stdout/stderr child capture, signal/exit/PID, context refusal, collector errors, report results and stale-output attribution."}},{"criterion":"AC3","outcome":"pass","revision":"087d5f2baa2285a65076313b40be4318d61cec26","evidence":{"kind":"observation","detail":"Existing profile/threshold bodies unchanged; 32 release tests, 67 documentation tests, documentation gate and independent review pass; frozen release untouched and no benchmark campaign."}}]
---

# BPR-2057: Future release workflow and diagnostics

## Scope

Owner request, 2026-11-11: leave the current v0.4.2 release alone, improve future
build time on main on a best-effort basis, and instrument builds for diagnostics
that do not require scraping logs. No performance measurement campaign, current
release intervention, acceptance weakening, signing or publication is authorized.

## Acceptance criteria

- AC1: Compile required local graphs before long suites and let independent
  physical hosts advance without a global compilation wait. Keep every required
  final gate and shared Windows/WSL host exclusion.
- AC2: Expose structured phase identity, source/candidate/platform context,
  timestamps, duration, actual exit, child identity and qualification report
  locations while retaining human output and unresolved failures.
- AC3: Document evidence-bound profiles, failure reconciliation and bounded
  timing diagnosis. Verify focused tests and documentation without a benchmark
  campaign or touching the frozen v0.4.2 candidate.

## Result and focused verification

Implementation `087d5f2baa2285a65076313b40be4318d61cec26`:

- The Mac producer compiles Release tests, Debug tests and the selected universal
  distribution variant before long qualification. Its normal/comprehensive,
  Debug-residual and standing Debug timing-omission selections are preserved.
- Future coordination uses a per-physical-host compile prerequisite, keeping
  Windows/WSL serialization and every selected platform's final acceptance gate.
  Valid upper-bound latency passes require no idle-host poll; failures remain
  visible, diagnosed and limited to one explicitly controlled same-binary retry.
- Phase JSONL and its summary expose source/candidate/platform context, cwd,
  command, timestamps, elapsed time, child PID, native exit and private raw
  stdout/stderr paths. Qualification adds structured report paths and unresolved
  row names. Collection failures, signal exits, pending phases and refused stale
  output do not become passing evidence. This is not compiler-unit profiling.
- `python3 tests/scripts/test_suite_runner_tests.py`: 29 tests pass, including
  streamed dual-pipe output, failure/interrupt retention, collection errors,
  native signal status, fresh context/refusal and stale-output attribution.
- `python3 tests/scripts/release_source_assets_tests.py`: 32 tests pass,
  including all variant/profile command orders, all six build failure boundaries,
  scope refusal and skip-tests distribution preservation.
- `python3 tests/scripts/verify_documentation_tests.py`: 67 tests pass.
- `python3 scripts/verify_documentation.py --root .`, `bash -n tools/release.sh`
  and `git diff --check`: pass. Independent review cleared after the pipe-read
  and close-error finding was fixed and tested.

No full product build, native release rehearsal or benchmark campaign was run.
No measured speedup is claimed. Existing frozen v0.4.2 sources, controllers,
outputs, acceptance and publication were not changed. The next release supplies
observed cost evidence; this slice reduces avoidable waits and repeated failure
cycles without reducing the selected evidence obligations.
