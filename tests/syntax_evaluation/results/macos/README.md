# RFC-BPR-0081 raw results, macOS

Measured on 2026-10-13 on the macOS reference station. The report that reads
these files is [docs/verification/RFC-BPR-0081.md](../../../../docs/verification/RFC-BPR-0081.md).

- **Station:** Apple M4 Pro, 14 cores, 48 GB, macOS 26.6.2.
- **Build:** Apple clang 21, `build/eval-release` (Release, `BUILD_TESTING=ON`,
  `BPR_BUILD_SYNTAX_EVALUATION=ON`).
- **Corpus:** materialized by `tools/materialize_corpus.py`.
- **Labels:** revision `309375c5a`.
- **Load:** other agents' builds ran throughout; `run-*.json` records the load
  average.

| File | What it holds |
|---|---|
| `summary.json` | `score_evaluation.py` over the timing run: accuracy per pin, engine, source and category; hypotheses; timing, heap, stop latency, stop outcomes and determinism per size class |
| `summary-accuracy-with-strict.json` | The same scorer over the one-round accuracy run, including the diagnostic strict variants `v0_23_4_strict` and `master_strict` |
| `summary-l-limit-raised.json` | The L-class diagnostic run with the span limit raised to 4,000,000 |
| `metrics-<run>-<pin>.jsonl.gz` | The harness's raw records: one per file, engine and round (`total_ns`, `refine_ns`, `grammar_ns`, `peak_heap`, span count, span digest), one per stop trial, and a summary per corpus-class process with its peak resident bytes |
| `failures-*.jsonl.gz` | Every label disagreement: pin, engine, file, offset, category, detail |
| `run-*.json` | The exact harness commands, exit codes, elapsed times and load averages |
| `size.json` | `tools/measure_size.py`: bytes added per macOS slice, and the compile time of each tree-sitter translation unit |
| `link-option-off.json`, `link-option-on.json`, `configure-option-off.log.gz` | `tools/check_link_dependencies.py` and the configure log with the option off and on |
| `ac07-base-530d92eaf.tsv.gz`, `ac07-evaluation-branch.tsv.gz` | `bench_syntax_span_dump` over the corpus at the RFC base and on the evaluation branch: path, status, span count, digest |
| `large-review-benchmark.json` | `syntax_first_file` and `syntax_navigation` from five interleaved `--prepared-open` rounds per binary |
| `sanitizer.json` | The ASan/UBSan corpus run: exit codes and its agreement with the Release spans |
| `fuzz.json` | The libFuzzer campaigns: binaries, seeds, durations, executions, coverage and findings |
| `build-cost.json` | The clean `dev` build and the extra compile time the candidate would add |

The spans themselves (about 300 MB per pin) are not committed. Rerunning the
harness regenerates them, and the digests in the metrics files check them.
