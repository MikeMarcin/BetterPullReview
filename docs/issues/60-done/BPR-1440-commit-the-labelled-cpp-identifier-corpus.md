---
schema: "implement-rfc.card.v1"
id: "BPR-1440"
title: "Commit the labelled C++ identifier corpus and gap suite"
status: "Done"
rfcs: ["RFC-BPR-0081"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"309375c5a","evidence":{"kind":"artifact","detail":"tests/syntax_evaluation/corpus/manifest.json names all seven classes; first-party pinned to 530d92eaf; third-party archives named by the build's SHA-256 pins; 5,270 materialized files, byte-identical across two runs; no third-party source committed.","path":"docs/issues/60-done/BPR-1440-commit-the-labelled-cpp-identifier-corpus.md"}},{"criterion":"AC2","outcome":"pass","revision":"309375c5a","evidence":{"kind":"artifact","detail":"Labels committed at 309375c5a before any candidate corpus run; clang 23.1.1 labels with contract overrides; 1,994-item blind hand review, 98.4% agreement, adjudication in labels/hand-review.jsonl.","path":"docs/issues/60-done/BPR-1440-commit-the-labelled-cpp-identifier-corpus.md"}},{"criterion":"AC3","outcome":"pass","revision":"309375c5a","evidence":{"kind":"artifact","detail":"Eleven gap fixtures (H1-H7, namespace macros, split extern C guards, conflict markers, C++26 reflection) with 68 hand labels; deterministic seeded generators for fragments, malformed, pathological and composer inputs.","path":"docs/issues/60-done/BPR-1440-commit-the-labelled-cpp-identifier-corpus.md"}},{"criterion":"AC4","outcome":"pass","revision":"309375c5a","evidence":{"kind":"command","detail":"scripts/verify_documentation.py --root . prints DOCUMENTATION_OK; git diff 530d92eaf -- src/adapters/syntax is empty.","path":"docs/issues/60-done/BPR-1440-commit-the-labelled-cpp-identifier-corpus.md"}}]
---

# BPR-1440: Commit the labelled C++ identifier corpus and gap suite

Part of [RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), milestone `evaluation-ground`. The labels are fixed before any
candidate runs, so this card lands before
[BPR-1442](../10-backlog/BPR-1442-measure-tree-sitter-cpp-on-macos.md) produces
a candidate result.

## Acceptance

- **AC1:** A committed corpus manifest names all seven corpus classes of the RFC.
  First-party files are pinned to a commit, and the third-party archives are
  named by SHA-256 from the build's own pins. No third-party source is committed.
- **AC2:** Labels for the RFC's categories are committed in a revision that
  holds no candidate result. Clang's AST labels the first-party corpus offline,
  and a label counts only when its declaration is in the same blob; the accepted
  conservative rule overrides clang where they differ. A seeded stratified sample
  of up to 50 identifiers per category per source labels the third-party,
  gap-suite and fragment classes by inspection.
- **AC3:** The gap suite has small committed fixtures for H1 to H8, namespace
  macros, `extern "C"` guards split by `#ifdef`, merge conflict markers and
  C++26 reflection syntax. The generators for fragments, malformed and
  pathological inputs and composer replay are deterministic and committed.
- **AC4:** The documentation gate passes and the shipped classifier is unchanged.

## Evidence

Labels revision `309375c5a` (2026-10-13 13:55:30 -07:00). The first candidate
run over the corpus started after it; before it, candidates ran only through
the contract rows, whose expectations predate this RFC.

- AC1: [`corpus/manifest.json`](../../../tests/syntax_evaluation/corpus/manifest.json)
  names the seven classes. First-party files come from `530d92eaf` by `git
  archive`; abseil, RE2, glaze and quill are verified by the SHA-256 and size
  the build pins, KSyntaxHighlighting by its source-manifest digest.
  Materialized: 5,270 files (contract 1,101; first-party 999 including four
  L-size concatenations; third-party 1,788; gap 11; fragments 420; malformed
  and pathological 45; composer 906). Two materializations were byte-identical.
  No third-party source is committed.
- AC2: libclang 23.1.1 labels the contract, first-party and third-party
  classes; the contract's conservative rules (R2-R8) override clang. Coverage:
  contract 1,091 full / 10 partial, first-party 950 / 45, third-party
  1,080 / 707 / 1 unlabelled (`flag_benchmark.cc`, recursion limit). Two full
  labelling runs gave identical files. Twelve independent labellers reviewed a
  stratified sample of 1,994 items (up to 50 per category per source, blind to
  the machine labels) and agreed on 1,963 (98.4%); adjudication corrected rule F
  for fragments (18 items) and overrode one qualifier. See
  [`corpus/README.md`](../../../tests/syntax_evaluation/corpus/README.md) and
  `labels/hand-review.jsonl`.
- AC3: eleven gap fixtures in `tests/fixtures/syntax/evaluation/gap/` with 68
  hand labels (`labels/gap.jsonl`, `tools/label_gap.py`). H8 is measured on the
  pathological inputs `brace-flood-8388600.cpp`, `brace-flood-8388608.cpp` and
  `enum-names-8mib.cpp`. Fragment, malformed, pathological and composer inputs
  are generated by `tools/materialize_corpus.py` from recorded seeds.
- AC4: the documentation gate passes; `src/adapters/syntax` is unchanged.
