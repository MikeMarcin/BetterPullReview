---
schema: "implement-rfc.card.v1"
id: "BPR-1908"
title: "Split the window composition out of github_application.cpp"
status: "Ready"
rfcs: []
depends_on: ["BPR-1907"]
priority: "P2"
metadata: {"baseline":"3e19d0e87","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass; subfolders welcome."}
---

# Split the window composition out of github_application.cpp

Round 4 of the `github_application.cpp` refactor. `github_application::implementation`,
one window's composition, is a 19,300-line class: 552 members, 467 member
functions (a 1,564-line constructor among them) and 42 nested types, all
defined in the class.

## Acceptance

- AC1: Declare the class in a private header and define its members,
  unchanged, in responsibility sources: construction, navigation and
  restoration, sign-in, workbench, defects, tours, agent setup, agent chat and
  notes, review drafts, review mutations, review ledger, commit filter, read
  state and Overview, and shutdown.
- AC2: Keep the security-pinned span, `compose_review_mutation` through
  `refresh_review_projection`, in the root, so the live provider's single
  construction site does not move. Reseal the production mutation-branch pin
  only after checking that its bytes differ by one leading tab per line, and
  update only the indentation-dependent markers of the contracts that find it.
- AC3: The composition contracts that scan composition source read the
  composition root's source set; counts keep their meaning (definitions, not
  declarations).
- AC4: Preserve behavior: full dev and tests-off builds; architecture,
  composition (source and Release), round-trip and the composed courses;
  format and documentation gates.
- AC5: Land the change, the completed card and a journal line on local main.
