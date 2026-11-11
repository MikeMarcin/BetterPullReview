---
schema: "implement-rfc.card.v1"
id: "BPR-2058"
title: "Prioritize previously failed repair qualification tests"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057"]
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"853923079535bb783c520267d745df9a5dee5db0","evidence":{"kind":"observation","detail":"Prior failure closure and exact-file selection verified; multiple/no-failure, excluded/renamed, preset and dependency checks pass. No circular pivot."}},{"criterion":"AC2","outcome":"pass","revision":"853923079535bb783c520267d745df9a5dee5db0","evidence":{"kind":"observation","detail":"Native four-job CTest setup/body/cleanup and full remainder execute after repeated early failure; aggregate coverage, missing report failure and raw batch receipts verified."}},{"criterion":"AC3","outcome":"pass","revision":"853923079535bb783c520267d745df9a5dee5db0","evidence":{"kind":"observation","detail":"32 runner tests, 33 release-source tests, documentation, Bash syntax, diff checks and independent final review pass. No full build, benchmark or frozen release change."}}]
---

# BPR-2058: Failure-first repair qualification

## Scope

The owner's final direction, 2026-11-11, drops circular rotation as needless
complexity. Preserve the runner's existing collect-all behavior, prioritize
previously failed tests, then run the remaining tests with normal CTest scheduling.
Prior results are ordering hints only, never reused passing evidence. Keep current
selected coverage and fixture/dependency groups. Apply only to future main work;
the active v0.4.2 release and its frozen procedure remain unchanged.

## Acceptance criteria

- AC1: Prior structured same-preset failures run first with their full fixture and
  dependency groups, followed by all remaining selected rows using native scheduling.
- AC2: Collect all results, including remaining rows after a repeated early failure.
  Preserve complete coverage, raw per-batch reports, failed/missing/skipped rows,
  cleanup and locks. Refuse excluded/unknown prior failures rather than hiding them.
- AC3: Focused real CTest controls, documentation and independent review pass;
  the active release remains unchanged and no benchmark campaign is run.

## Result and focused verification

Implementation `853923079535bb783c520267d745df9a5dee5db0` follows the final
failure-first direction. No circular-pivot implementation was made.

- `--failures-first` reads a same-preset structured timing report as an ordering
  hint, retains its digest and prioritizes prior failing fixture/dependency
  components. Excluded/renamed failures and split components are refused.
- Both native CTest batches retain collect-all execution and ordinary concurrency,
  locks and cleanup. All current selected rows remain required. An early repeated
  failure does not suppress the remainder; raw per-batch results and the aggregate
  retain every failure, missing row and actual nonzero exit.
- Exact row-name files avoid Windows command-line size limits. Release/Debug
  entrypoint flags are independently bound to their respective native presets.
  No old result gains pass credit and no threshold or profile membership changes.
- `python3 tests/scripts/test_suite_runner_tests.py`: 32 tests pass. The new
  native four-job fixture proves prior-failure setup/body/cleanup run first,
  all remaining rows run even after a repeated failure, and aggregate coverage
  is exact. Controls cover multiple/no failures, dependency closure, exclusions,
  wrong preset, duplicates and missing first-batch result collection.
- `python3 tests/scripts/release_source_assets_tests.py`: 33 tests pass,
  including independent Release/Debug ordering-hint forwarding.
- Documentation structure, Bash syntax and diff checks pass. Independent review
  cleared the final implementation, including exact-name files and aggregate
  JUnit metadata.

No production builds, performance campaign or current v0.4.2 release changes
were made. Earlier failure feedback is expected; no measured speedup is claimed.
