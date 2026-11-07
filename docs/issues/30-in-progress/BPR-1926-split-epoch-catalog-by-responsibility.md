---
schema: "implement-rfc.card.v1"
id: "BPR-1926"
title: "Split epoch_catalog.cpp by responsibility"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"cec2b3b57","mandate":"Owner, 2026-11-06/07: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round; at most about 2,000 lines per file for this initial pass; push and pull as needed."}
---

# Split epoch_catalog.cpp by responsibility

Round 12 of the largest-files refactor.
`src/adapters/sqlite/src/epoch_catalog.cpp` is 6,446 lines. Everything but
`make_epoch_catalog` and the testing checkpoint setter sits in one anonymous
namespace: statement and transaction wrappers, the catalog state, the SQLite
catalogue session and port classes, and 27 out-of-line members (open, reads,
append reservation and finalization, maintenance operations, prune and cache
deletion).

## Acceptance

- AC1: Move the definitions, unchanged, into responsibility-named sources
  beside it. The shared wrappers and classes move from the anonymous
  namespace into one private header's
  `bpr::adapters::sqlite::epoch_catalog_detail`; helpers used by one source
  stay private to it. `bpr/adapters/sqlite/epoch_catalog.hpp` does not change.
- AC2: Preserve behavior: SQLite adapter and app build in both
  configurations; the SQLite and epoch rows; architecture and format gates.
- AC3: Land the change, the completed card and a journal line on main.
