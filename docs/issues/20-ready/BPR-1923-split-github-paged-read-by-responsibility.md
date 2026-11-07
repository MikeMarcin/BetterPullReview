---
schema: "implement-rfc.card.v1"
id: "BPR-1923"
title: "Split github_paged_read.cpp by responsibility"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"e71bb6995","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split github_paged_read.cpp by responsibility

Round 11 of the largest-files refactor.
`src/adapters/github/src/github_paged_read.cpp` is 6,407 lines: 4,800 lines of
anonymous page, JSON, failure, rate-limit and record-parsing helpers and the
paged reads declared in `github_paged_read.hpp` (response metadata and
timeline, point reads, personal discovery and the pull-request read).

## Acceptance

- AC1: Move the definitions, unchanged, into responsibility-named sources
  beside it; helpers used by one source stay private to it, shared ones are
  declared in narrow support headers. `github_paged_read.hpp` does not change.
- AC2: Preserve behavior: GitHub adapter and app build in both
  configurations; the GitHub contract, paged-read, markdown image, large
  review replay and composition rows; architecture, format and documentation
  gates.
- AC3: Land the change, the completed card and a journal line on main.
