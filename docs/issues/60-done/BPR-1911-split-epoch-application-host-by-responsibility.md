---
schema: "implement-rfc.card.v1"
id: "BPR-1911"
title: "Split epoch_application_host.cpp by responsibility"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"baseline":"6beb7288f..BPR-1910","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"00ffa053f","evidence":{"kind":"observation","detail":"epoch_host_implementation.hpp declares the class; seven responsibility sources define it; forwarders stay in the source."}},{"criterion":"AC2","outcome":"pass","revision":"00ffa053f","evidence":{"kind":"observation","detail":"Single-use helpers private; epoch_host_support declares 23 shared helpers."}},{"criterion":"AC3","outcome":"pass","revision":"00ffa053f","evidence":{"kind":"command","detail":"Tests-off and dev builds; 32/32 epoch, diff, composition and architecture rows pass. Windows/Linux not built."}},{"criterion":"AC4","outcome":"pass","revision":"00ffa053f","evidence":{"kind":"observation","detail":"Card and journal line committed; landing waits for main integration (pinned-origin confirmation)."}}]
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

## Result

Source `00ffa053f`; `epoch_application_host.cpp` goes from 11,625 to 411 lines (the
public forwarders and test hooks). New files (lines): `epoch_host_materialization.cpp` 1643, `epoch_host_requests_and_recovery.cpp` 1617, `epoch_host_dispatch_and_imports.cpp` 1521, `epoch_host_review_reads.cpp` 1425, `epoch_host_source_search_and_diffs.cpp` 1379, `epoch_host_implementation.hpp` 1366, `epoch_host_catalog_and_history.cpp` 1360, `epoch_host_blob_authority.cpp` 906, `epoch_host_support.cpp` 539, `epoch_host_support.hpp` 163, `epoch_host_implementation.cpp` 143.

Helpers used by one source stay private to it; `epoch_host_support` declares the
23 that several sources share. `ArmedAdmissionStage`, the test-only stage the
source's hooks arm and the blob authority reads, becomes an `inline` variable so
it remains one object; it is constant-initialized, so no initialization order
changes. `main` had not changed this file or its header.

Checks on macOS arm64: application library tests-off and full dev builds; and 32
rows all passing: `application_epoch_host`, the epoch contract, catalog,
persistence, maintenance, update-line and kill-recovery rows, SQLite epoch
contracts, the anonymous diff authority, agent workspace rows, the UIforETW
selected-diff, diff workbench, review ledger, defect and workbench composition
courses, both composition contracts, `qt_composed_github_scale_100` and the
architecture rows. Windows and Linux were not built.
