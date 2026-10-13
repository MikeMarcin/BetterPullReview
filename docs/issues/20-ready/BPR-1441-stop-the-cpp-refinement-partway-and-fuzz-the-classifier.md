---
schema: "implement-rfc.card.v1"
id: "BPR-1441"
title: "Stop the C++ refinement partway in a test and fuzz the C++ classifier"
status: "Ready"
rfcs: ["RFC-BPR-0081"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
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
