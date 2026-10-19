# C++ identifier parser evaluation (RFC-BPR-0081)

[RFC-BPR-0081](../../docs/rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md)
compares the shipped C++ identifier refinement
(`src/core/src/cpp_identifier_context.cpp`) with candidate T, the same
refinement fed by tree-sitter-cpp. Nothing here ships. The directory exists in a
build only with `BPR_BUILD_SYNTAX_EVALUATION=ON`, which needs `BPR_BUILD_QT=ON`
and `BUILD_TESTING=ON`; with the option off no target acquires or links
tree-sitter. The report and its results are in
[docs/verification/RFC-BPR-0081.md](../../docs/verification/RFC-BPR-0081.md).

| Path | What it is |
|---|---|
| `candidate/cpp_identifier_context_tree_sitter.cpp` | Candidate T. It defines `refine_cpp_identifiers` with the shipped signature |
| `harness/syntax_evaluation_harness.cpp` | The harness. It holds both refinements behind the unchanged classifier |
| `corpus/` | The corpus manifest and its description |
| `labels/` | The committed labels, fixed before any candidate result |
| `tools/` | Corpus materializer, labellers, runner, scorer, size probe and link check |

`cmake/BprTreeSitterProvider.cmake` acquires the tree-sitter v0.27.1 runtime
and two tree-sitter-cpp pins, the v0.23.4 tag and `master` at
`c009222808634c1014f82438d4883753516a2c24`. It uses the RE2 provider's pattern:
SHA-256, size and licence digest, re-extracted under `build/_bpr`, offline mode
honoured, source substitution refused.

## Targets, for each pin (`v0_23_4`, `master`)

- `bench_syntax_evaluation_<pin>`: the harness, with both refinements.
- `bench_syntax_evaluation_<pin>_strict`: the same harness with the
  candidate's diagnostic "strict" error policy: no facts from inside
  tree-sitter ERROR nodes. The evaluated T is "lenient".
- `bpr_syntax_candidate_<pin>`: the candidate's object files. A twin links them
  ahead of the syntax adapter, so the adapter's own classifier calls the
  candidate. The adapter archive's refinement is then never loaded.
- Twins of the RFC's contract rows, registered as `<row>@ts-v0.23.4` and
  `<row>@ts-master`: `syntax_classification_contract`,
  `syntax_refinement_cancellation`, `qt_syntax_pane_composition`,
  `application_cpp_identifier_appearance` and
  `application_large_review_benchmark`. In a `BPR_BUILD_FUZZERS` graph there is
  also `fuzz_syntax_<pin>`, with `fuzz_syntax_corpus@…`.
  `application_appearance_host` does not link the syntax adapter, so it has no
  twin.

`bench_syntax_span_dump` classifies the corpus with the shipped classifier
alone. AC07 compares its output at the RFC base with its output after the
evaluation.

The `syntax-evaluation` target builds them all.

## Running it

```sh
cmake --preset dev -B build/eval-release -DCMAKE_BUILD_TYPE=Release -DBPR_BUILD_SYNTAX_EVALUATION=ON
ninja -C build/eval-release syntax-evaluation
python3 tests/syntax_evaluation/tools/materialize_corpus.py --repo-root . \
  --cpm-cache build/.cpm-cache --out "$SCRATCH/corpus"
python3 tests/syntax_evaluation/tools/run_evaluation.py --build build/eval-release \
  --corpus "$SCRATCH/corpus" --out "$SCRATCH/results"
python3 tests/syntax_evaluation/tools/score_evaluation.py --repo-root . \
  --corpus "$SCRATCH/corpus" --results "$SCRATCH/results" --out "$SCRATCH/summary.json"
python3 tests/syntax_evaluation/tools/measure_size.py --repo-root . --build build/eval-release --out "$SCRATCH/size.json"
python3 tests/syntax_evaluation/tools/check_link_dependencies.py --build build/eval-release
ctest --test-dir build/eval-release -R '@ts-'
```

The harness takes `--classes` (a corpus class or size class),
`--maximum-spans` (the product's 262,144 by default; the L diagnostic used
4,000,000), `--rounds`, `--stop-trials` and `--engines`.
`tools/render_tables.py` prints the report's tables from a summary.

`$SCRATCH` is a directory outside the checkout. The corpus includes extracted
third-party sources, which are never committed.
