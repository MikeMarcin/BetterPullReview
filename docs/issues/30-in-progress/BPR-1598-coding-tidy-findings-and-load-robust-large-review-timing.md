---
schema: "implement-rfc.card.v1"
id: "BPR-1598"
title: "The coding-tidy gate passes, and the large-review timing rows hold on a loaded machine"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner-approved cleanup, 2026-10-17","related_cards":["BPR-1373","BPR-0967","BPR-1247","BPR-1495","BPR-1580","BPR-1583","BPR-1560","BPR-1557"],"reservation":"docs/issues/_reservations/BPR-1598--BPR-1598--95fbc5d6a30d.json"}
acceptance: []
---

# BPR-1598: The coding-tidy gate passes, and the large-review timing rows hold on a loaded machine

Two pieces of upkeep the owner approved on 2026-10-17.

**The tidy gate.** `cmake --build --preset dev --target coding-format-check
coding-tidy` stopped on findings no card owned: a narrowing conversion in
`home_panel.cpp`'s summary flags, and the `using_` lambda in
`server_card.cpp` ([[BPR-1373]]), whose name the naming rule refuses.

**The timing rows.** `application_large_review_responsiveness`, `_launch`,
`_active_work`, `_pathological`, `_history` and `large_review_replay_contract`
fail when other builds load the machine and pass when rerun alone
([[BPR-1580]], [[BPR-1583]], [[BPR-1560]], [[BPR-1557]]). Their limits are
wall-clock limits; the rows already carry `RUN_SERIAL`, which keeps CTest from
running its own rows beside them but cannot hold back another agent's build.

## Acceptance

- AC1: The two findings are fixed without changing behaviour, and the full
  `coding-format-check` and `coding-tidy` gate (clang-format 22, clang-tidy
  22) passes over every translation unit; any other finding it reports is
  fixed when trivial and behaviour-neutral, or listed here.
- AC2: Every timed figure a large-review limit reads leaves out the time the
  course was kept off a processor by another process, so a loaded machine no
  longer fails the rows; the limits themselves are unchanged and the wall
  readings stay in each report.
- AC3: A genuine regression still fails: the timing control proves that GUI
  work and a blocked GUI thread still count in full while the processors are
  saturated, and an injected stall in the product fails its row under load.
- AC4: One full `ctest --preset dev -j6` on a loaded machine shows none of
  these rows failing; the documentation gate passes.

## Next

Implement, verify under load, land.
