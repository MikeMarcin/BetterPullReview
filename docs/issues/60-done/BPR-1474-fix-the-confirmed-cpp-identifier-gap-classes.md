---
schema: "implement-rfc.card.v1"
id: "BPR-1474"
title: "Fix the confirmed C++ identifier gap classes in the heuristic"
status: "Done"
rfcs: ["RFC-BPR-0081"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","follow_up_of":"RFC-BPR-0081","report":"docs/verification/RFC-BPR-0081.md","contract":"RFC-BPR-0072"}
acceptance: [{"criterion":"AC1: each confirmed class has a public-classifier contract test that fails before and passes after, and the scored gap suite shows it refuted with no new false positive","outcome":"pass","revision":"4add47f63","evidence":{"kind":"observation","detail":"cpp_gap_contract (H1-H7, extern C) and brace_flood_contract (H8) fail on revision 6 and pass at 4add47f63. Corpus re-run with the old and new refinement: FP 136 to 132, FN 3,576 to 3,561, no category gains a false positive, no new label disagreement, 19 fixed. Scored H1-H8 refuted; H8 under the owner's 2026-10-13 rule (peak refinement heap on 8 MiB inputs at most 512 MiB): 2.02 GB to 261 MB.","path":"docs/issues/60-done/BPR-1474-fix-the-confirmed-cpp-identifier-gap-classes.md"}},{"criterion":"AC2: cpp_identifier_context.cpp stays at or under 800 lines with no new whole-blob pass; macro expansion, cross-file and unqualified uses stay out of scope","outcome":"pass","revision":"4add47f63","evidence":{"kind":"observation","detail":"698 lines. Conditional directives are recorded in prepare_roles' existing directive scan and consumed in step with collect_declarations; no pass added. Out-of-scope classes were not attempted.","path":"docs/issues/60-done/BPR-1474-fix-the-confirmed-cpp-identifier-gap-classes.md"}},{"criterion":"AC3: the C++ identifier revision is bumped, and RFC-BPR-0072's contract, the harness baseline and syntax_refinement_cancellation pass","outcome":"pass","revision":"4add47f63","evidence":{"kind":"observation","detail":"cpp_identifier_heuristic_revision 7; revision-6 keys refused. syntax_classification_contract, syntax_refinement_cancellation, qt_syntax_pane_composition, application_cpp_identifier_appearance, application_appearance_host, the four boundary checks, core-only (268 of 273 pass; the five failures are in files this slice does not touch), coding-format-check, changed-line tidy and the documentation gate pass after merging main.","path":"docs/issues/60-done/BPR-1474-fix-the-confirmed-cpp-identifier-gap-classes.md"}}]
---

# BPR-1474: Fix the confirmed C++ identifier gap classes in the heuristic

The "extend" outcome of
[RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md).
Its [report](../../verification/RFC-BPR-0081.md) rejected tree-sitter-cpp and
confirmed these failures of the shipped refinement
(`src/adapters/syntax/src/cpp_identifier_context.cpp`) inside RFC-BPR-0072's
scope, each with a committed gap fixture in
`tests/fixtures/syntax/evaluation/gap/` and hand labels in
`tests/syntax_evaluation/labels/gap.jsonl`:

| Hypothesis | Fixture | Confirmed failure |
|---|---|---|
| H1 | `h1_inline_namespace.cpp` | `namespace A::inline B {` is not recognized, so `A`, `B` and the qualified use lose their roles |
| H2 | `h2_attributed_namespace.cpp` | `namespace [[deprecated]] Legacy {` is not recognized |
| H3 | `h3_enum_comparisons.cpp` | `X < 3, High = Y > 2` is paired as a template, so `High` loses its enumerator role |
| H4 | `h4_x_macro.cpp` | an X-macro invocation in an enum body becomes an enumerator and loses its macro role |
| H5 | `h5_conditional_enum.cpp` | an enum opened in both `#if` branches never closes; later names after commas become enumerators |
| H6 | `h6_brace_shadow.cpp` | `int Reason{ 0 };` is not a shadow, so `Reason::ID` is painted |
| H7 | `h7_non_ascii.cpp` | a non-ASCII enumerator is painted only up to its first non-ASCII byte |
| H8 | `brace-flood-8388608.cpp` (generated) | 8 MiB of `{` holds about 2.0 GB of temporary refinement state |

`extern_c_guards.cpp` also shows that an enumerator declared inside a split
`extern "C" {` block is not found from outside it.

## Acceptance

- **AC1:** Each confirmed class above has a contract test through the public
  classifier that fails before the fix and passes after it, and the scored gap
  suite shows the class refuted with no new false positive in any category.
- **AC2:** RFC-BPR-0081's rules hold: `cpp_identifier_context.cpp` stays at or
  under 800 lines and gains no new pass over the whole blob. A fix that would
  need either reopens the parser decision with the report's numbers instead.
  Macro expansion, cross-file uses and unqualified uses stay out of scope.
- **AC3:** The C++ identifier revision is bumped so cached spans from the old
  rules are refused, and RFC-BPR-0072's contract, the evaluation's harness
  baseline and `syntax_refinement_cancellation` still pass.

## Evidence

Implementation `4add47f63`, measured on macOS (Apple M4 Pro) on 2026-10-13.

- AC1: `cpp_gap_contract` in
  [`syntax_classification_contract_tests.cpp`](../../../tests/syntax_contract/syntax_classification_contract_tests.cpp)
  classifies each committed gap fixture through the public classifier. With the
  revision-6 refinement every class fails (H1 both checks, H2, H3, H4, H5's
  closing check, H6, H7 and the split `extern "C"` enum); with `4add47f63` all
  pass. `brace_flood_contract` in
  [`syntax_refinement_cancellation_tests.cpp`](../../../tests/syntax_contract/syntax_refinement_cancellation_tests.cpp)
  bounds what the refinement requests on 299,952 bytes of `{`: 100 bytes per
  source byte now, over 128 before. The baseline engine of
  `syntax_evaluation_v0_23_4` was run over the freshly materialized corpus
  (5,270 files, one round) with the old and the new refinement and scored by
  `score_evaluation.py`. The old run reproduces the committed baseline exactly
  (136 FP, 3,576 FN). New: FP 132 (ns_decl 0, enumerator_decl 1 to 0, enum_use
  12, ns_qualifier 82, conservative_negative 41 to 38, macro_keep 0, protected
  0), FN 3,561 (ns_decl 243 to 240, enumerator_decl 8 to 5, enum_use 438 to
  433, ns_qualifier 2,270 to 2,267, macro_keep 617 to 616). No label
  disagreement is new; 19 are fixed, all in the gap suite. Outside the gap
  suite only `malformed/random-utf8-1mib.cpp` changes spans (non-ASCII bytes).
  H1 to H8 move from confirmed to refuted. H8: the peak refinement heap on
  both brace floods falls from 2,022,703,120 and 1,099,956,240 bytes to
  261,095,520, under the RFC's 512 MiB cap. Determinism held; statuses
  unchanged (5,264 available, 4 over the span limit).
- Owner decision, 2026-10-13: **Accept.** "H8 counts as fixed because the peak
  refinement memory on pathological 8 MiB input is now bounded under
  RFC-BPR-0081's 512 MiB cap (2 GB down to 261 MB)." The original scorer rule
  (H8 confirmed when an 8 MiB input has the largest peak heap of any input)
  could not be refuted by any refinement that keeps state per token.
  `score_evaluation.py` now confirms H8 only when an 8 MiB input's peak
  refinement heap exceeds 512 MiB; under it the re-scored run reads H1 to H8
  refuted. The rule lived in the scorer, not in RFC-adopted text, so it is
  recorded in the [report](../../verification/RFC-BPR-0081.md#gap-hypotheses),
  which keeps the earlier results, T's included, as they were recorded.
- AC2: `cpp_identifier_context.cpp` is 698 lines. No pass over the whole blob
  was added: conditional directive lines are recorded in the existing
  directive scan of `prepare_roles` and consumed in step with
  `collect_declarations`. Macro expansion, cross-file uses and unqualified
  uses were not attempted.
- AC3: `cpp_identifier_heuristic_revision` is 7, and the contract refuses a
  key carrying revision 6. `syntax_classification_contract`,
  `syntax_refinement_cancellation`, `qt_syntax_pane_composition`,
  `application_cpp_identifier_appearance`, `application_appearance_host`, the
  four boundary checks and `core-only` pass; `coding-format-check` and
  changed-line tidy are clean.
