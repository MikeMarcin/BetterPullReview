---
schema: "implement-rfc.card.v1"
id: "BPR-1922"
title: "Split github_mutation_contract.cpp by responsibility"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"6c52dc5b7","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split github_mutation_contract.cpp by responsibility

Round 10 of the largest-files refactor.
`src/adapters/github/src/github_mutation_contract.cpp` is 6,972 lines: 2,900
lines of anonymous wire helpers and 92 contract functions declared in
`github_mutation_contract.hpp` (request compilation, response and observation
parsing, edits and publication).

## Acceptance

- AC1: Move the definitions, unchanged, into responsibility-named sources
  beside it; helpers used by one source stay private to it, shared ones are
  declared in one support header. `github_mutation_contract.hpp` does not change.
- AC2: Preserve behavior: GitHub adapter and app build in both
  configurations; the GitHub contract, live mutation provider and recovery,
  live-mutation isolation, review-mutation application and composition
  courses; architecture, format and documentation gates.
- AC3: Land the change, the completed card and a journal line on main.
