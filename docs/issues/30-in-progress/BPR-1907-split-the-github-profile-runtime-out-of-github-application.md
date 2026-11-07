---
schema: "implement-rfc.card.v1"
id: "BPR-1907"
title: "Split the GitHub profile runtime out of github_application.cpp"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1906"]
priority: "P2"
metadata: {"baseline":"6b521dc18","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass; subfolders welcome."}
---

# Split the GitHub profile runtime out of github_application.cpp

Round 3 of the `github_application.cpp` refactor. `github_profile_runtime`
(one BPR profile's providers, hosts, stores, effect lanes and windows) is a
2,561-line class with a 790-line constructor, plus 300 lines of members
already defined after it. The startup, public `github_application` members,
`implementation` and the fixture accessors all use it, so it needs a
declaration header before `implementation` can be split.

## Acceptance

- AC1: Declare `github_profile_runtime` in a private header and define its
  members, unchanged, in responsibility sources: construction (constructor,
  service start, identity), sign-in and connections, release access,
  shutdown, and presentation/activity/windows.
- AC2: Give the root's remaining shared helpers their own files: CLI account
  use and the CLI read offer, startup failure records, defect identity and
  Insights process metrics.
- AC3: The composition contract's source scans read the composition root's
  whole source set (the root plus `github_application/`), with the same
  required and hostile cases.
- AC4: Preserve behavior: full dev and tests-off builds; architecture,
  composition, round-trip and the composed courses that drive a profile;
  format and documentation gates; reseal the moved code's pins only.
- AC5: Land the change, the completed card and a journal line on local main.
