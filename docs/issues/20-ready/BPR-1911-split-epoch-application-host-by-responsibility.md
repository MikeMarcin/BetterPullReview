---
schema: "implement-rfc.card.v1"
id: "BPR-1911"
title: "Split epoch_application_host.cpp by responsibility"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"6beb7288f..BPR-1910","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass."}
---

# Split epoch_application_host.cpp by responsibility

Round 7 of the largest-files refactor, the first after `github_application.cpp`.
`src/application/src/epoch_application_host.cpp` is 11,625 lines: 1,027 lines
of anonymous helpers and `epoch_application_host::implementation`, a
10,171-line pimpl with 144 member functions defined in the class, followed by
the public forwarders.

## Acceptance

- AC1: Declare the implementation in a private header and define its members,
  unchanged, in responsibility sources (blob authority, requests and recovery,
  review reads, source search and selected diffs, materialization, dispatch and
  imports, catalog and history); the public forwarders stay in the source.
- AC2: Helpers used by one source stay private to it; shared ones are declared
  in one support header.
- AC3: Preserve behavior: full dev and tests-off builds; the epoch host's
  application tests and the epoch, diff and composition courses that drive it;
  architecture boundaries; format and documentation gates.
- AC4: Land the change, the completed card and a journal line on local main.
