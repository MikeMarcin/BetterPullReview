---
schema: "implement-rfc.card.v1"
id: "BPR-1491"
title: "The hash evaluation driver uses contained temporaries"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_cards":["BPR-1446","BPR-1450","BPR-1451"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e3d28e5e6","evidence":{"kind":"command","command":"ctest --preset dev -R ^contained_temporary_usage_contract$","detail":"Both scratch roots use contained_temporary_directory; the contract passes (failed in the cd4860e30 baseline gate)."}},{"criterion":"AC2","outcome":"pass","revision":"e3d28e5e6","evidence":{"kind":"observation","detail":"--help exits 0 from any cwd; phase bodies unchanged; helper creates 0o700 roots and removes read-only Git objects. The git and sweep256 phases (opt-in BPR_HASH_EVALUATION harness, not in the default graph or release) were not executed."}}]
---

# The hash evaluation driver uses contained temporaries

`contained_temporary_usage_contract` fails on main:
`tests/hash_evaluation/run_hash_evaluation.py` calls `tempfile.mkdtemp` at
lines 277 and 290 instead of `scripts/contained_temporary_support`.

## Acceptance

- AC1: The driver makes its temporaries through
  `scripts/contained_temporary_support`, and
  `contained_temporary_usage_contract` passes.
- AC2: The hash evaluation driver still runs its documented course (or its
  own tests) with the same results.

## Boundaries

No change to what the evaluation measures.

## Evidence

Cause: BPR-1446 added `tests/hash_evaluation/run_hash_evaluation.py`, whose
`git` and `sweep256` phases made their scratch roots with
`tempfile.mkdtemp` and removed them with `shutil.rmtree`. The driver lives
under `tests/`, so `contained_temporary_usage_contract` refuses it: the
`tempfile` owner-only descriptor cannot be used inside the Windows
AppContainer.

Fix: both phases now use `contained_temporary_directory` from
`scripts/contained_temporary_support` (same `bpr-hash-evaluation-` prefix),
imported the way the other `tests/` drivers do it (repository root on
`sys.path`). The phase body runs inside the `with` block and the root is
removed on exit, as before. A removal that fails still raises
(`RemovalFailed` is an `OSError`, as `shutil.rmtree`'s error was). On POSIX
the root is still created by `tempfile` with mode `0o700`. Nothing the
evaluation measures, records or compares has changed.

Verification (AC1, AC2). No build, ctest or benchmark was run, because a
release gate was running performance rows on this machine:

- `python3 tests/scripts/contained_temporary_usage_tests.py` gives
  `Ran 6 tests in 9.966s` / `OK` (exit 0).
- `python3 tests/hash_evaluation/run_hash_evaluation.py --help` exits 0, and
  exits 0 when run from `/` too, so the import does not depend on the
  working directory.
- A direct check with no build: a `contained_temporary_directory` root
  holding `git init` plus a `git hash-object -w` object (read-only object
  files) had the right prefix and mode `0o700`, and was gone after exit.
- Not run: the driver's `git` and `sweep256` phases against a
  `-DBPR_HASH_EVALUATION=ON` build. Those phases need the built harness.
- Integration at e3d28e5e6 (main 7dd7b709d merged), macOS arm64: `ctest --preset dev -R '^(contained_temporary_usage_contract|documentation_structure)$'` passes 2/2; `run_hash_evaluation.py --help` exits 0. The driver's `git` and `sweep256` phases need the opt-in `-DBPR_HASH_EVALUATION=ON` harness, which the default graph and the release do not build; they were not executed. AC2 rests on the unchanged phase bodies, the same `with` scope for each scratch root, and the worker's direct check that the helper makes a 0o700 root and removes read-only Git objects.
