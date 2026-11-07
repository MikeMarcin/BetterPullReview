---
schema: "implement-rfc.card.v1"
id: "BPR-1909"
title: "Split the fixture accessors out of github_application.cpp"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1908"]
priority: "P2"
metadata: {"baseline":"1537b11a9","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass; separate fixture/test support from production where appropriate."}
---

# Split the fixture accessors out of github_application.cpp

Round 5 of the `github_application.cpp` refactor. The root still holds
14,400 lines of tests-only `github_application_fixture_access` members: the
composition courses' drivers and evidence readers, with 77 anonymous helpers.

## Acceptance

- AC1: Move the accessors, unchanged, into tests-only course files under
  `src/app/src/github_application/`, one per course family; helpers used by
  one file stay private to it, and the few used by several are declared in
  shared fixture-support headers.
- AC2: Every moved definition stays under `BPR_BUILD_TESTING`, so a tests-off
  build compiles none of it.
- AC3: Preserve behavior: full dev and tests-off builds; the composition
  contracts (source and Release), round-trip and every course whose driver
  moved; format and documentation gates.
- AC4: Land the change, the completed card and a journal line on local main.
