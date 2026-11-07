---
schema: "implement-rfc.card.v1"
id: "BPR-1928"
title: "Split github_application_host.cpp by responsibility"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"8608b6ab7","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split github_application_host.cpp by responsibility

Round 14 of the largest-files refactor.
`src/application/src/github_application_host.cpp` is 5,119 lines. Its
`github_application_host::implementation` struct is 4,272 of them, with
about 110 members defined in the class body: dispatch and enrollment, public
preview and projection, profile sessions, viewer and capability reads,
credential refresh, store and retrieval, retained-refresh deletion, and
failure, cancellation, shutdown and launch renewal.

The round does not touch `native_https_transport.cpp`, `main.cpp`,
`installed_git_process.cpp` or `installed_git_cache.cpp`. Architecture rules
name each of those files as a designated effect owner or read its text, so
splitting them is a separate decision.

## Acceptance

- AC1: Move the struct into a private `github_host_implementation.hpp` and
  its member definitions, unchanged, into responsibility-named sources, as
  BPR-1906 did for the epoch host. Helpers used by one source stay private to
  it, and shared ones go in one support header.
  `bpr/application/github_application_host.hpp` does not change.
- AC2: Preserve behavior: application and app build in both configurations;
  the application, device-authorization and public-preview rows; the
  architecture and format gates.
- AC3: Land the change, the completed card and a journal line on main.
