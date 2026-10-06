---
schema: "implement-rfc.card.v1"
id: "BPR-1229"
title: "Windows: qt_agent_launch_canary fails contained at its durable child course, which opens a private SQLite store inside the AppContainer"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json","related_cards":["BPR-1128","BPR-1139"]}
acceptance: [{"criterion":"The owner's choice is recorded on this card","outcome":"pass","revision":"2c62d2f218b09fba93a0c41553672ae3568d9ebc","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1229-windows-qt-agent-launch-canary-durable-child-course-fails-contained.md","detail":"Decided by the integrating coordinator on 2026-10-06 as test configuration: option 2, the durable child course as its own row on every platform, uncontained and pinned on Windows; the canary's launch courses stay contained. Recorded in the card's 'Decided: option 2' section."}},{"criterion":"qt_agent_launch_canary passes on Windows, and the durable course runs somewhere on Windows","outcome":"pass","revision":"2c62d2f218b09fba93a0c41553672ae3568d9ebc","evidence":{"kind":"command","path":"tests/adapters_qt/agent_launch_canary_tests.cpp","detail":"bpr-main-win windows-dev Debug, MSVC 14.51: qt_agent_launch_canary passes contained (108.6 s, 109.2 s) with every launch course, after three courses take make_scratch_directory roots the AppContainer can write; qt_agent_launch_canary_durable_course passes uncontained (2.8 s); agent_parent_death, architecture_boundaries and architecture_boundary_contract pass. macOS and Linux runs of both rows are owed."}},{"criterion":"test_network_isolation_manifest names no row this card changes","outcome":"pass","revision":"2c62d2f218b09fba93a0c41553672ae3568d9ebc","evidence":{"kind":"command","path":"scripts/verify_test_network_isolation.py","detail":"qt_agent_launch_canary_durable_course pinned dos-volume-name with its measured line ('FAILED: open durable child course', agent_launch_canary_tests.cpp:281); test_network_isolation_manifest and test_network_isolation_structure_contract pass on Windows."}}]
---

# BPR-1229: Windows: qt_agent_launch_canary fails contained at its durable child course, which opens a private SQLite store inside the AppContainer

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`qt_agent_launch_canary` fails on Windows in 0.05 s, contained:
`FAILED: open durable child course` (`tests/adapters_qt/agent_launch_canary_tests.cpp:281`).
The course `test_durable_private_input_crosses_only_after_admission` came with
RFC-BPR-0023 (`8e20e9e8`, 2026-09-27). It makes a `scratch_root` and opens a
private SQLite review store under it with `make_private_review_store`. It runs
before any canary launch, so the row stops before the courses it had before.

Other new rows that open a private store under a scratch root fail the same
way contained and pass run directly: `sqlite_review_tour_storage`,
`sqlite_review_cycle_storage` and `sqlite_review_cycle_runtime` (made
uncontained in `532cffaf`), and the five snapshot workspace rows.

This row blocks the Windows halves of [[BPR-1128]] (AC4) and [[BPR-1139]] (AC5).

Updated 2026-10-01 by bpr-main-mac. Both cards were finished with this row
classified here: on Windows they compile `HEAD`'s canary code, and their
Windows remainders moved to [[BPR-1138]]. Its Part B, the three Windows
names in `agent_launch_canary_tests.cpp`, waits on this card's decision,
because only this row runs them.

## The decision, which is the owner's

The row launches the canary as an agent. Whether it may leave the AppContainer
is a containment decision, so this card does not make it. Two options:

1. **Uncontain the row on Windows.** Mark it `WINDOWS_UNCONTAINED` and pin it
   in `scripts/verify_test_network_isolation.py` with the measured cause. This
   is the smallest change. The cost is that the canary's launch courses no
   longer run under the Windows network boundary.
2. **Keep the row contained, and move the durable course.** Move
   `test_durable_private_input_crosses_only_after_admission` into its own
   executable and row, and uncontain only that row. The launch courses keep
   their boundary. The cost is a new row and a split source.

## Acceptance

- The owner's choice is recorded on this card.
- `qt_agent_launch_canary` passes on Windows, and the durable course runs
  somewhere on Windows.
- `test_network_isolation_manifest` names no row this card changes.

## Parked for the owner's choice (bpr-main-win, 2026-10-05)

Reproduced at current `main` (`ade8a312d`), `build/windows-dev` Debug:
`qt_agent_launch_canary` fails contained in 0.06 s at `FAILED: open durable
child course` (`agent_launch_canary_tests.cpp:281`), as described above.
Nothing was changed. The containment choice above is the owner's, so the card
is parked on this one question:

> **Should `qt_agent_launch_canary` leave the Windows AppContainer whole
> (option 1), or should its durable child course move to its own uncontained
> row while the canary's launch courses stay contained (option 2)?**

The station recommends option 2. Its launch courses are the ones whose
network boundary matters, and the durable course is the only part that
needs a private SQLite store, which the container refuses. That is the same
refusal as the other store rows pinned `dos-volume-name`.

## Decided: option 2 (coordinator, 2026-10-06)

The integrating coordinator decided option 2 on 2026-10-06, as test
configuration rather than an owner decision. The durable child course
becomes its own row, `qt_agent_launch_canary_durable_course`, on every
platform. On Windows it is pinned `WINDOWS_UNCONTAINED` with its measured
line, like the other private-store rows. `qt_agent_launch_canary` keeps its
launch courses contained. The card is un-parked.

## Split and Windows evidence (bpr-main-win, 2026-10-06)

**The split, the same on every platform.** `agent_launch_canary_tests.cpp`
runs `test_durable_private_input_crosses_only_after_admission` only when
given `--durable-course`, and prints `agent-launch-durable-course-tests:pass`.
Its default run no longer includes it. The new row
`qt_agent_launch_canary_durable_course` runs that argument on every
platform. On Windows it is registered `WINDOWS_UNCONTAINED` and pinned
`dos-volume-name` in `scripts/verify_test_network_isolation.py`, with the
measured line: contained, the private-review store under its scratch root is
refused at "FAILED: open durable child course" (line 281).
`qt_agent_launch_canary` stays contained.

**What the split uncovered.** With the durable course first, the contained
row had stopped before every launch course since 2026-09-27, so those courses
had not run contained on Windows since then. Once they ran, three of them
failed. Each gave its contained child a bare `QTemporaryDir`, which carries
no entry for the AppContainer, so the child could not write its marker:

- `test_input_budget_refuses_before_real_child_effect` failed at "negative
  control proves the same child creates the marker when admitted";
- the private-claim course failed at "child reaches delayed-input
  checkpoint";
- the stdin-context course shared the same root shape.

All three now use `bpr::tests::make_scratch_directory` with a
`scratch_guard`, as the descendant course already did, following
`tests/AGENTS.md` ("a row inside the AppContainer needs the container's
SID"). The bare `std::filesystem::permissions` calls on those roots, which do
nothing on Windows, are gone with them.

**Windows** (`build/windows-dev` Debug, MSVC 14.51, no `warning C` line):

- `qt_agent_launch_canary` passes contained (108.6 s, then 109.2 s), every
  launch course included, the Windows breakaway and assignment courses among
  them.
- `qt_agent_launch_canary_durable_course` passes uncontained (2.8 s).
- `agent_parent_death`, `test_network_isolation_manifest`,
  `test_network_isolation_structure_contract`, `architecture_boundaries` and
  `architecture_boundary_contract` pass.

**Owed:** macOS and Linux runs of both rows. The split is the same there; on
those platforms the durable course moves to its own row, contained as before.

## macOS run (coordinator, 2026-10-06)

At 4cf46a609 on macOS arm64 Debug (build/dev in a clean worktree): 12/12 of the
canary (contained launch courses and the durable-course row), agent parent death,
cycle export/bundle/publish-desk/workbench, git cache and process contracts,
architecture boundaries and the network isolation manifest passed.

**Note: Linux run at `4cf46a609` (station `bpr-main-linux`, WSL2, GCC 15.2.0,
`build/dev` rebuilt whole, `-j1`).** The build had no GCC warning or
error; the only "Warning" lines are CMake configure notices in Qt's own CMake
files. 12 of 12 passed: `qt_agent_launch_canary` (63.1 s),
`qt_agent_launch_canary_durable_course`, `agent_parent_death` (20.8 s),
`review_cycle_export`, `review_cycle_bundle`, `review_cycle_publish_desk`,
`review_cycle_workbench` (34.7 s), `git_cache_contract`,
`git_process_contract`, `architecture_boundaries`,
`architecture_boundary_contract` (188.2 s) and
`test_network_isolation_manifest`. Nothing needed a fix. Done is not
reopened.
