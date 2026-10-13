---
schema: "implement-rfc.card.v1"
id: "BPR-1441"
title: "Stop the C++ refinement partway in a test and fuzz the C++ classifier"
status: "Done"
rfcs: ["RFC-BPR-0081"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"cad32aa0a","evidence":{"kind":"observation","detail":"syntax_refinement_cancellation passes on macOS: stop requested at the 12,800th of 25,601 refinement allocations, after the grammar pass, gives cancelled with no spans; uncancelled control available with spans; negative control (default stop token in the refinement) exits 1.","path":"docs/issues/60-done/BPR-1441-stop-the-cpp-refinement-partway-and-fuzz-the-classifier.md"}},{"criterion":"AC2","outcome":"pass","revision":"cad32aa0a","evidence":{"kind":"observation","detail":"fuzz_syntax builds in the macOS fuzz graph; fuzz_syntax_corpus replays 14 seeds clean; 300 s campaign seed 1441, 66,040 executions clean; entered in docs/verification/BPR-0970-parser-fuzzing.md.","path":"docs/issues/60-done/BPR-1441-stop-the-cpp-refinement-partway-and-fuzz-the-classifier.md"}},{"criterion":"AC3","outcome":"pass","revision":"cad32aa0a","evidence":{"kind":"observation","detail":"src/adapters/syntax unchanged since 530d92eaf; syntax_classification_contract passes after the fixture restore c43efc3e3; clang-format 22, changed-line clang-tidy and documentation gate pass. Unrelated main failures in test_network_isolation_manifest recorded on the card.","path":"docs/issues/60-done/BPR-1441-stop-the-cpp-refinement-partway-and-fuzz-the-classifier.md"}}]
---

# BPR-1441: Stop the C++ refinement partway in a test and fuzz the C++ classifier

Part of [RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), milestone `evaluation-ground`. Both tests stay after the
evaluation whatever its outcome, because they test the shipped classifier.

## Acceptance

- **AC1:** A test requests a stop after the grammar pass has completed and while
  the identifier refinement is running, through the public
  `packaged_syntax_classifier`, and observes `cancelled` with no spans. The
  same blob without the stop is `available` with spans, so the test fails if the
  stop never reaches the refinement.
- **AC2:** A libFuzzer target over the public C++ classifier builds under
  `BPR_BUILD_FUZZERS`, has a seed corpus and a `fuzz_syntax_corpus` replay
  row, and asserts the classifier's properties: status and span agreement, span
  bounds and order, hunk projection and repeatability. It is entered in the
  BPR-0970 fuzz inventory.
- **AC3:** The shipped refinement and its spans are unchanged, and the focused
  syntax, format, tidy and documentation checks pass.

## Evidence

Candidate `cad32aa0a`, macOS (Apple M4 Pro, Apple clang, `dev` Debug and the
`BPR_BUILD_FUZZERS` graph with the Homebrew LLVM 23 libFuzzer runtime).

- AC1: `syntax_refinement_cancellation` passes: the refinement made 25,601
  allocations uncancelled; the stop requested at the 12,800th gave `cancelled`
  with no spans, and the next request matched the uncancelled answer. Negative
  control: with `refine_cpp_identifiers` handed a default stop token, the test
  exits 1.
- AC2: `fuzz_syntax` builds; `fuzz_syntax_corpus` replays the 14 seeds clean.
  A 300-second campaign (seed 1441, 66,040 executions) was clean. The target is
  in [BPR-0970's inventory](../../verification/BPR-0970-parser-fuzzing.md).
- AC3: `src/adapters/syntax` is unchanged since RFC base `530d92eaf`.
  `syntax_classification_contract` passes again after `c43efc3e3` restored its
  fixture (0220258f4 had reformatted it and broken the contract on `main`).
  Format (clang-format 22), changed-line clang-tidy and the documentation gate
  pass. `test_network_isolation_manifest` fails on `main` for rows this card
  did not add (`application_source_pr_*`, `gh_cli_git_smart_transport`).
- Windows: the cancellation test is a plain contract row; libFuzzer is not
  built on Windows. Neither has run on Windows or Linux from this lane.

## Correction, 2026-10-13 (after independent review)

- **The negative control first recorded above was not valid.** The patched
  refinement did not compile (unused parameter), so the command chain stopped
  before the test ran.
- **The control was redone properly** with the refinement handed a default stop
  token. The test then fails with "FAILED: a stop during the refinement is
  reported as cancelled", having confirmed that the stop was requested while
  the refinement ran.
- **The test now also identifies the role buffer under MSVC.** The review
  found that MSVC's standard library pads allocations of 4,096 bytes or more,
  so the original exact-size signature could not match on Windows. The test now
  accepts the three request sizes the three standard libraries make, with a
  blob length that no string copy or span-vector growth reaches, and it watches
  only the classifying thread.
- **macOS passes again,** with the baseline and both candidate twins. It is
  still unrun on Windows and Linux.

See the commit that carries this note.