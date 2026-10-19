# BPR-1619 handoff

Role: rfc-implementer | Model: gpt-6.1-sol | Effort: xhigh

Assignment `core-boundaries-v1/core-boundaries`, mandate `core-boundaries-r1`,
leaf `/root/core_boundaries`, BetterPullReview main checkout. Complete bounded
milestone; no subagents or additional engineering owners. Parent was read-only.

## Result and candidates

- Baseline: `4bae9df701fdd70e16c0481a1c595e404b4298ce`.
- Plan/reservation: `a69668185`; claim: `30515b8b753e1571f701d0716bf6edd7b131fa75`.
- Production candidate: `381ec842b`.
- Final engineering/evidence candidate: `d17f69f78` (only public input-contract
  comments and verification/binding added; computation unchanged).
- Acceptance/board completion: `2c7dd37ae8d4a870f7231a47f71d99d52ebf0e17`.
- BPR-1619 is Done. Four criteria and all four additive milestone exits pass.
- Core privately owns the xdiff matcher, provider-free contract and factory;
  entire xdiff adapter/target/wiring removed. Xdiff stays its own replaceable
  shared library with unchanged pin/notices/source/relinking contracts.
- Application still owns scheduling, copied-input lifetime and abandonment.
  Core owns the pure identifier refinement and span/status values, with
  application aliases; Qt packaged grammar/template/resources and host caches
  retain their owners. No syntax version or algorithm change.
- Atomics/mutexes with explicit caller-owned/referenced state and lexical paths
  are permitted; filesystem effects and hidden global decision state are not.
  Native path conventions differ; no portable path-identity claim.

## Verification

[Verification](verification.md), [acceptance](acceptance.json) and
[original binding](milestone-binding.json) retain the evidence and limitations.
Final [plan check](final-plan-check.json) says stop, and the scoped
[board check](final-board-check.json) is valid. Helpers check consistency;
engineering acceptance is the implementer's evidence-backed judgment.

- Core-only direct computation/core diff/host: 3/3 pass; actual core symbols,
  undefined xdl_diff resolved by its DSO and no Qt linkage observed.
- Pre-move wrapper versus core wrapper: 86/86 complete output rows identical;
  all 78 GitHub hunk joins agree. Moved production bodies are identical after
  only include/namespace/type qualification and format22.
- Composed public syntax/cancellation, identifier appearance, active-work and
  pathological large review plus actual app dependency audit: six pass.
  Cache-byte course intentionally skips Debug (requires optimized build).
- Complete boundary, dependency, runtime/package/source/security group: 9/9
  pass (352.80 s). Optional parser candidate/harness compile; no remeasurement.
- Sibling's resealed round-trip composition contract passes (4.89 s).
- Format22 on 15 C++ files, tidy22 on both moved production TUs, documentation
  gate/67 contract tests and diff whitespace checks pass. Post-finish gate
  reports DOCUMENTATION_OK.
- The old 2026-10-13 raw intraline baseline differs in 57 rows after later
  BPR-1537 work; the same differences exist with the exact pre-move wrapper.
  That preserved history is not a relocation regression or a new failure waiver.

## Review, writers and command state

No required human/native/independent acceptance remains for this bounded scope;
no attended session, event cursor, protected candidate or review server exists.
All owned configure/build/CTest, corpus/baseline comparison, compiler/tidy/format,
documentation and bookkeeping commands finished. No process/server is handed off.
Consumed task-owned Git source extracts, probe binaries, runtime-header extracts,
corpus and edit scripts were removed. Useful numeric/log evidence, selection and
comparison procedure remain in `/tmp/bpr-core-boundaries` for the coordinator.
Shared build trees were retained. No owner desktop/browser/private-data access,
live GitHub mutation, push, remote work or package publication occurred.

Unrelated writer work was preserved and excluded from explicit owned-path
commits. Its source candidate `530a7b94d` and board completion `120a9ea9c` are
interleaved in main; neither belongs to this milestone. At handoff its
`docs/issues/_checkpoints/BPR-1377/continuation.md` remains dirty. No clean
whole-checkout claim is made. The shared composition reseal belongs to that
writer; its historical comment was preserved. This leaf releases repository
writer ownership after committing this durable bookkeeping-only checkpoint.

## Continuation and uncertainties

Report this mandate complete. No successor scope is assigned. Original RFC0081
Windows/Linux measurements and closeout, RFC0082 platform/closeout and wider
native/package/vault/crash/vulnerability acceptance remain on their existing
cards. No broader RFC closure, release qualification, universal performance or
cross-platform lexical-path equivalence is implied. Source guards remain
textual checks rather than complete effect analysis.

## Production candidate paths

```text
AGENTS.md
CONTRIBUTING.md
cmake/BprDependencies.cmake
cmake/BprXdiffProvider.cmake
cmake/VerifyArchitectureBoundaries.cmake
cmake/VerifyCoreBoundary.cmake
docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md
docs/architecture/diff-pipeline-and-rendering.md
docs/architecture/system-architecture.md
docs/development/build-and-test.md
docs/rfcs/RFC-BPR-0072-cpp-identifier-context.md
docs/rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md
docs/rfcs/RFC-BPR-0082-diff-engine-evaluation.md
docs/verification/RFC-BPR-0072.md
docs/verification/RFC-BPR-0081.md
docs/verification/RFC-BPR-0082.md
packaging/dependencies.json
src/adapters/CMakeLists.txt
src/adapters/syntax/CMakeLists.txt
src/adapters/syntax/src/cpp_identifier_context.cpp
src/adapters/syntax/src/cpp_identifier_context.hpp
src/adapters/syntax/src/packaged_syntax_classifier.cpp
src/adapters/xdiff/CMakeLists.txt
src/adapters/xdiff/include/bpr/adapters/xdiff/xdiff_line_matcher.hpp
src/adapters/xdiff/src/xdiff_line_matcher.cpp
src/app/CMakeLists.txt
src/app/src/cycle_session.cpp
src/app/src/github_application.cpp
src/app/src/review_cycle_desktop.cpp
src/application/include/bpr/application/appearance_runtime_ports.hpp
src/application/include/bpr/application/diff_line_matcher.hpp
src/core/AGENTS.md
src/core/CMakeLists.txt
src/core/include/bpr/core/cpp_identifier_context.hpp
src/core/include/bpr/core/diff_line_matcher.hpp
src/core/include/bpr/core/syntax_classification.hpp
src/core/src/cpp_identifier_context.cpp
src/core/src/xdiff_line_matcher.cpp
tests/CMakeLists.txt
tests/core/CMakeLists.txt
tests/core/core_computation_boundaries_tests.cpp
tests/scripts/architecture_boundary_tests.py
tests/scripts/core_boundary_tests.py
tests/support/product_diff.hpp
tests/syntax_evaluation/CMakeLists.txt
tests/syntax_evaluation/README.md
tests/syntax_evaluation/candidate/cpp_identifier_context_tree_sitter.cpp
tests/syntax_evaluation/harness/syntax_evaluation_harness.cpp
tests/syntax_evaluation/tools/measure_size.py
third_party/git-xdiff/PROVENANCE.md
```
