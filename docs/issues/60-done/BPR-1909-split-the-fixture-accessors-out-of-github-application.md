---
schema: "implement-rfc.card.v1"
id: "BPR-1909"
title: "Split the fixture accessors out of github_application.cpp"
status: "Done"
rfcs: []
depends_on: ["BPR-1908"]
priority: "P2"
metadata: {"baseline":"1537b11a9","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass; separate fixture/test support from production where appropriate."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"768850524","evidence":{"kind":"observation","detail":"Fourteen fixture course files plus fixture_course_support and fixture_uiforetw_observation; single-use helpers private, shared ones declared."}},{"criterion":"AC2","outcome":"pass","revision":"768850524","evidence":{"kind":"command","detail":"Every moved definition stays under BPR_BUILD_TESTING; tests-off composition build passes."}},{"criterion":"AC3","outcome":"pass","revision":"768850524","evidence":{"kind":"command","detail":"Full dev build; 360-row pass: 348 pass, one load-sensitive row passes alone, six Debug timing rows nongating, six failures identical at round 1 (2fc1cb2a3). Format 22."}},{"criterion":"AC4","outcome":"pass","revision":"768850524","evidence":{"kind":"observation","detail":"Completed card and journal line accompany the source for the local main fast-forward."}}]
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

Over main's BPR-1914/1915/1918 edits to the window files, the combined tree
builds in both configurations and passes 67 of 69 composition, round-trip,
tour, agent and architecture rows; the two failures are the pre-existing ones
above.
