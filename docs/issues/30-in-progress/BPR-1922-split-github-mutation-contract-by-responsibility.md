---
schema: "implement-rfc.card.v1"
id: "BPR-1922"
title: "Split github_mutation_contract.cpp by responsibility"
status: "In Progress"
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

## Result

Source `64e5dc0dd`; `github_mutation_contract.cpp` goes from 6,972 to 46 lines.
New files (lines): `github_mutation_contract_edit_requests.cpp` 1898, `github_mutation_contract_responses_and_observation.cpp` 1506, `github_mutation_contract_review_and_meta_requests.cpp` 1341, `github_mutation_contract_publication.cpp` 927, `github_mutation_contract_edits.cpp` 627, `github_mutation_contract_support.cpp` 586, `github_mutation_contract_support.hpp` 483. All are under 2,000; a first cut at three sources put
the private wire helpers beside their single users and reached 3,145 lines, so
the round uses five narrower ones.

Checks on macOS arm64: GitHub adapter and composition library tests-off builds;
full dev build except `main`'s broken `agent_context_supplement_tests.cpp`; and
47 rows (every `github_*` contract row, live review mutation provider and
recovery rows, live-mutation isolation, designated live checks, review-mutation
application rows, publication, reviewer, description and reaction courses, both
composition contracts, the round-trip contract, architecture): 46 pass, and
`application_review_tour_publication_manual` fails as it does at round 1.
Windows and Linux were not built.
