---
schema: "implement-rfc.card.v1"
id: "BPR-1440"
title: "Commit the labelled C++ identifier corpus and gap suite"
status: "In Progress"
rfcs: ["RFC-BPR-0081"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
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
