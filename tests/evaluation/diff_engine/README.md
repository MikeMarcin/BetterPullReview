# Diff-engine evaluation harness

> **Retired by [[BPR-1485]].** Linked xdiff (`l-myers-indent`) is the product's
> primary line matcher, and the first-party matcher this harness measured every
> candidate against is deleted, with the hook it used. Configuring with
> `BPR_DIFF_ENGINE_EVALUATION=ON` now stops with a message; rerun the harness
> from commit `cd4860e30`, the last with the baseline engine. The sources and
> scripts stay as the record of the measurement.

The opt-in harness of [RFC-BPR-0082](../../../docs/rfcs/RFC-BPR-0082-diff-engine-evaluation.md).
It compares candidate algorithms for parts of the diff engine against the
engine itself, through the core's evaluation seam
([`diff_compute_components.hpp`](../../../src/core/include/bpr/core/diff_compute_components.hpp),
[[BPR-1465]]). Every candidate result passes the same first-party validation.
The report and its raw results are in
[`docs/verification/RFC-BPR-0082.md`](../../../docs/verification/RFC-BPR-0082.md).

Nothing here is part of a shipping configuration. `BPR_DIFF_ENGINE_EVALUATION`
is off by default; a configuration with it off acquires and links nothing from
this directory.

## What it builds

- **Pinned candidate sources**, each verified by SHA-256, size and licence
  digest and re-extracted under `build/<tree>/_bpr/diff-evaluation`: Git 2.39.5
  and 2.56.0 (the process shape runs `git` binaries built from them; the linked
  shape compiles 2.56.0's `xdiff/` with the first-party shim in `xdiff_shim/`),
  dtl 1.21, rapidfuzz-cpp 3.3.4 and the standard-library diff-match-patch port
  (leutloff, `ba3bbe3`). Nothing enters `third_party/` or
  `packaging/dependencies.json`.
- **Candidates** (`candidates/`): each wraps one algorithm behind the seam.
  `candidate_descriptions()` lists them: `p-<git>-<algorithm>[-indent]`,
  `l-<algorithm>[-indent]`, `c-onp`, `c-histogram`, `c-slide`, `c-rapidfuzz`,
  `c-dmp` and `c-onp-intraline`.
- **The hook** (`candidates/hook.cpp`): in this build only, the core is compiled
  with `BPR_DIFF_ENGINE_EVALUATION` and every executable links the hook. The
  ordinary entry points then use the candidate named by
  `BPR_DIFF_EVALUATION_CANDIDATE`, or by the first line of the build tree's
  `diff-evaluation-candidate.txt` when a test runner clears the environment.
  An unknown name stops the process.
- **The harness** (`harness/`): corpus timing, phase timing, per-pair intraline
  timing, stops, process overhead, determinism and peak memory. The process
  shape is launched, stopped, killed and reaped through the Git adapter's
  shared supervision (`process_support`).
- **Inventories**: `tool_diff_compute_inventory` and `tool_diff_host_inventory`
  are generated copies of the two suites that stop at their first failed
  expectation. They count failures instead, so a run lists all of them.

## Running it

```sh
cmake --preset dev -B build/eval -DCMAKE_BUILD_TYPE=Release \
  -DBPR_DIFF_ENGINE_EVALUATION=ON
ninja -C build/eval bench_better_pull_review_diff_engine_evaluation \
  tool_diff_compute_inventory tool_diff_host_inventory tool_diff_fuzz_replay
python3 tests/evaluation/diff_engine/scripts/prepare_corpus.py --repo "$PWD" \
  --stage "$PWD/build/eval/_bpr/diff-evaluation" --git /usr/bin/git \
  --output "$PWD/build/eval/diff-evaluation-corpus"
python3 tests/evaluation/diff_engine/scripts/run_measurements.py --build build/eval \
  --corpus build/eval/diff-evaluation-corpus --results build/eval/diff-evaluation-results/<station>
python3 tests/evaluation/diff_engine/scripts/run_suites.py --build build/eval --repo "$PWD" \
  --out build/eval/diff-evaluation-results/<station>/suites.jsonl --candidates <names>
python3 tests/evaluation/diff_engine/scripts/run_inventories.py --build build/eval \
  --out build/eval/diff-evaluation-results/<station>/inventories.jsonl --candidates <names>
python3 tests/evaluation/diff_engine/scripts/make_report.py \
  --results build/eval/diff-evaluation-results/<station> \
  --corpus build/eval/diff-evaluation-corpus --station <station> \
  --adjudication docs/verification/RFC-BPR-0082/suite-adjudication.json \
  --raw docs/verification/RFC-BPR-0082/<station>-raw.json \
  --markdown docs/verification/RFC-BPR-0082/<station>-results.md
```

Every path given to Git is absolute; the harness makes the corpus and output
paths absolute itself. Run the measurements on a station that is as idle as
possible: the harness records the one-minute load average beside every sample.

`scripts/record_oracle.py` recorded the GitHub oracle in
`tests/fixtures/github/diff-oracle-v1/` against `MikeMarcin/BPR-Test`. It is
live and is not part of an ordinary run.

The process shape's launch is POSIX here (`posix_spawn` in the hook,
`QProcess` with a new session in the harness). On Windows the hook needs a
`CreateProcess` path and the harness the Git adapter's job object; that port
belongs to [[BPR-1469]].
