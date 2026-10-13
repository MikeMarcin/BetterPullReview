---
schema: "implement-rfc.card.v1"
id: "BPR-1474"
title: "Fix the confirmed C++ identifier gap classes in the heuristic"
status: "Ready"
rfcs: ["RFC-BPR-0081"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","follow_up_of":"RFC-BPR-0081","report":"docs/verification/RFC-BPR-0081.md","contract":"RFC-BPR-0072"}
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
