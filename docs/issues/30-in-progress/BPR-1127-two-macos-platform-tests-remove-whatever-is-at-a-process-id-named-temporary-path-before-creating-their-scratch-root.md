---
schema: "implement-rfc.card.v1"
id: "BPR-1127"
title: "Two macOS platform tests remove whatever is at a process-id-named temporary path before creating their scratch root"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"BPR-1126's AC4 scan, its 'The same class, not changed here' item","reservation":"docs/issues/_reservations/BPR-1127--BPR-1128--e21108c796a5.json","related_cards":["BPR-1126","BPR-1121","BPR-1128"]}
---

# BPR-1127: Two macOS platform tests remove whatever is at a process-id-named temporary path before creating their scratch root

## Finding

[[BPR-1126]]'s scan found these and gave them this title. Linux builds
neither source. Each names its scratch root in the system temporary
directory from the process id, removes whatever is at that name, and only
then makes the directory. At `b8944a06`:

| Row | Source | Name | Remove | Create | Then |
| --- | --- | --- | --- | --- | --- |
| `macos_capsule_identity` | `tests/platform/macos_capsule_identity_tests.cpp` | `bpr-capsule-identity-<pid>`, lines 116 and 117 | line 118 | `create_directories`, line 119 | not narrowed; removed at lines 131 and 184 |
| `macos_supervisor_identity_continuity` | `tests/platform/macos_supervisor_identity_continuity_tests.cpp` | `bpr-supervisor-identity-<pid>`, lines 530 and 531 | line 532 | `create_directories`, line 533 | narrowed by `std::filesystem::permissions`, line 534; removed at line 892 |

- **It removes what is not its own.** `std::filesystem::remove_all` removes
  whatever is at the name first: a directory, or a link, that someone else
  put there.
- **It can work in what a link reaches.** A link put back at the name
  between the removal and `create_directories` is accepted: that call does
  not fail on a link to a directory. The capsule course would then build its
  capsule in the link's target. The supervisor course would narrow the
  target with `permissions`, which follows a link, and build its fixtures
  there.
- **The capsule root is not private.** It is made with the umask's mode.

The exposure is smaller than [[BPR-1126]]'s. On macOS the temporary
directory is per user, and under the CTest runner it is private to the row.
So only this user's own processes could plant at the name. The rule in
`tests/AGENTS.md` still applies: never remove a fixed or predictable
temporary path before creating it.

## Scope

- Each root is made by `mkdtemp`: a random name, created only if nothing is
  at that name, mode `0700`. [[BPR-1121]]'s `scratch_guard` owns it from the
  moment it exists and removes it, with everything in it, however `main`
  ends. Nothing is removed before the root is made.
- `tests/sqlite_contract/scratch_root.hpp` is not used. It narrows with the
  SQLite adapter's rule, which these targets do not link, and [[BPR-1128]]
  is moving its shared part to `tests/support/`. `mkdtemp` and the guard are
  that helper's POSIX branch.
- `main` becomes a function try block with [[BPR-1121]]'s handler, so an
  exception from the course also removes the root, and is named.

## Acceptance

- AC1: Both roots are made by `mkdtemp`, at a random name, only if nothing is
  at the name, mode `0700`, and owned from creation by `scratch_guard`.
  Neither source removes anything before it makes its root, and the
  supervisor course no longer narrows its root with `permissions`.
- AC2: Negative control. At `HEAD`, a sentinel directory planted at the
  row's own process-id name is removed by the row. The card records what
  `HEAD` does with a link planted at that name that points to a sentinel
  directory the trial owns. With the candidate, both plants and the sentinel
  directory's contents are left exactly as they were.
- AC3: Both rows pass through `ctest` on macOS with the candidate and print
  the same lines as at `HEAD`. A direct sandboxed run of each with a kept
  `TMPDIR` leaves it empty.
- AC4: Gates: `clang-format` 22, `git diff --check`, `DOCUMENTATION_OK`,
  and no scoped `tidy.py` finding on a changed line.

## Boundaries

- The two test sources only. No product source, no row registration and no
  composition pin.
- Trial plants are made only at names the trial checked were absent, inside
  a temporary directory the trial made and keeps. They hold only sentinel
  data the trial wrote. A link's target is a sentinel directory the trial
  made. Every plant is removed by its exact name afterwards.
