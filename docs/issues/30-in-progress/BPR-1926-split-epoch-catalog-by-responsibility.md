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

## Result

Source `f4e972f28`; `epoch_catalog.cpp` goes from 6,446 to 43 lines. It keeps
`make_epoch_catalog` and the testing checkpoint setter. New files (lines):
`epoch_catalog_maintenance_operations.cpp` 1226 (receiving, spool, install
and candidate records, operation transitions and the recovery page),
`epoch_catalog_reads.cpp` 1054 (residency, snapshot, canonical review,
anchors, occurrences, history), `epoch_catalog_prune_and_cache_delete.cpp`
1043, `epoch_catalog_append_reservation.cpp` 849,
`epoch_catalog_support.cpp` 767, `epoch_catalog_open.cpp` 637,
`epoch_catalog_append_finalization.cpp` 637 and `epoch_catalog_support.hpp`
486. All are under 2,000. The shared statement and transaction wrappers,
catalog state, and the session and port classes moved from the anonymous
namespace into `bpr::adapters::sqlite::epoch_catalog_detail`, so they now
have external linkage in that private namespace; the journal checkpoint is an
inline variable there, still only under `BPR_SQLITE_TESTING`.

The round also renames the include guards of the five adapter support
headers from rounds 8 to 11 (`d8005d413`). They had taken `src/app`'s
`BPR_APP_` prefix; adapter headers spell the guard from their path.

Checks on macOS arm64: full tests-off build; full dev build except `main`'s
broken `agent_context_supplement_tests.cpp`; format gate; and 55 rows (every
SQLite and epoch row, architecture and coding rows): all pass. Windows and
Linux were not built.
