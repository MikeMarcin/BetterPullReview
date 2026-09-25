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

## Fixed and verified on macOS (bpr-main-mac, 2026-09-24)

Claimed at `12d9bd80`. Station `bpr-main-mac`: macOS 26.6.2 on Apple
Silicon, Apple clang 21.0.0, Qt 6.11.2, `build/dev` Debug with warnings as
errors. The change touches the two test sources and nothing else.

### The change

- **The roots.** Each source has a `make_scratch_root()` that calls
  `mkdtemp` on `bpr-capsule-identity-XXXXXX` or
  `bpr-supervisor-identity-XXXXXX` in the system temporary directory. A
  `bpr::tests::scratch_guard` from `tests/support/test_expectation.hpp`
  holds the root from the next line and removes it, with everything in it,
  however `main` ends. The `remove_all` before creation is gone, and so are
  the explicit removals at the end and on the capsule's early return. The
  supervisor's `permissions` call is gone as well: `mkdtemp` makes the root
  mode `0700`.
- **`main`** in each source is a function try block whose handler is
  `bpr::tests::failing_exit()`. So an exception from the course unwinds
  through the guard, and it is named on a `FAILED:` line. It used to end in
  `std::terminate` and leave the root.
- The courses' own `expect` still counts failures and returns at the end,
  as before. Neither source used the shared `expect`, and neither does now.

### Verification

Both builds exited 0 with no `warning:` line: the whole `build/dev` graph
at `b8944a06`, and the two targets with the candidate. The supervisor
target was re-signed by its post-build step.

**AC2, the negative control.** A trial driver, not committed, ran each
row's command directly under the runner's own derived profile, from the
row's working directory, with the row's environment. `TMPDIR` was a
`fixture/` directory, mode `0700`, that the driver kept and listed
afterwards ([[BPR-1121]]'s macOS entry describes the driver). The driver
ran the row through a `/bin/sh` prelude that planted at
`$TMPDIR/<prefix>$$` and then `exec`ed the row's command. `exec` keeps the
process id, so the plant sat at exactly the name the row computes. The
prelude first checked that nothing was at that name. A link's target was
a directory `target/`, holding `sentinel.txt`, beside `fixture/` in the
trial's own root.

| Plant | `HEAD` (`b8944a06`) | Candidate |
| --- | --- | --- |
| A sentinel directory, mode `0700`, holding `sentinel.txt` | **removed by both rows.** Nothing was at the name afterwards, and both rows passed | **left by both rows**, with the same inode, mode `0700`, and the same `sentinel.txt` bytes. Both rows passed |
| A link to the trial's sentinel directory | **the link is removed by both rows.** Its target is left, with the same inode and bytes: `remove_all` does not follow a link | **the link is left by both rows**, with the same inode and target. The target is left, with the same inode and bytes |

At `HEAD`, a link put back between the removal and `create_directories`
would be accepted. The capsule course would then build its capsule in the
link's target, and the supervisor course would narrow the target with
`permissions`. That race was not run.

**AC1, the roots as made.** During each candidate run, a watcher polled the
top level of `TMPDIR` every millisecond. Each row made one root, at a
random name such as `bpr-capsule-identity-9c8e1E` or
`bpr-supervisor-identity-0h5o0r`. Each was a real directory, mode `0700`,
owned by this user. With a plant present, the root was made beside it at
a different random name, and the plant was not touched.

**AC3, the rows.**

| Check | Result |
| --- | --- |
| `ctest --preset dev -j1 -V -R '^macos_(capsule_identity\|supervisor_identity_continuity)$'`, at `HEAD` and with the candidate | 2 of 2 pass both times: capsule 0.07 s and 0.39 s, supervisor 2.18 s and 2.26 s. Each prints the same 18 lines as at `HEAD`. The only differences are the runner's random directory and the root's name, which was the process id and is now random |
| A direct sandboxed run of each row with the candidate, with no plant | both pass, and `TMPDIR` is empty afterwards |
| The same with a plant | both pass, and `TMPDIR` holds exactly the plant afterwards |
| `/private/tmp` across all these runs | gained no entry |

**AC4, the gates.**

| Gate | Result |
| --- | --- |
| `clang-format` 22.1.8 (`llvm@22`) `--dry-run -Werror` on the two sources; `git diff --check` | pass. Both files were clean at `HEAD`. A blank line after the new declarations keeps the old `verifier` lines' alignment unchanged |
| `_coding-standard/scripts/tidy.py --paths` on the two sources, with clang-tidy 22.1.8 and with 23.1.1 | `clang-tidy passed.`, with no finding in either source. Both versions report `bugprone-throwing-static-initialization` at `tests/support/test_expectation.hpp:137`, which this card includes but does not change. Version 23 also reports `bugprone-std-exception-baseclass` at line 152 |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The kept trial roots were removed by exact name. The logs, the driver, the
plant prelude and the per-run JSON are station records under
`build/logs/bpr-1127-mac/` and are not committed.
