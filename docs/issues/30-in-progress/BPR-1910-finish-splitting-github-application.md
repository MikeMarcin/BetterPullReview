---
schema: "implement-rfc.card.v1"
id: "BPR-1910"
title: "Finish splitting github_application.cpp"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1909"]
priority: "P2"
metadata: {"baseline":"72f469cef","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass."}
---

# Finish splitting github_application.cpp

Round 6, the last for `github_application.cpp`. What remains besides the
security-pinned review-mutation span is the startup implementation, the public
`github_application` members and the startup fixture accessors, under an
include list sized for the 42,000-line file it used to be.

## Acceptance

- AC1: Move the startup implementation and `github_application::start`, the
  desktop-integration members and the window lifecycle and status members,
  unchanged, into their own sources; the startup fixture accessors join the
  test-only fixture files.
- AC2: The root keeps the automated-test boundary reader, the provider
  using-declarations and the pinned mutation span, and includes only what that
  code uses (platform-guarded and contract-required includes stay).
- AC3: Preserve behavior: full dev and tests-off builds; composition (source
  and Release), round-trip, startup, profile and window courses; format and
  documentation gates.
- AC4: Land the change, the completed card and a journal line on local main.
