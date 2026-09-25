---
schema: "implement-rfc.card.v1"
id: "BPR-1126"
title: "Four sqlite storage contracts remove whatever is at /tmp/bpr-<store>-<N> before creating their scratch root"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux", "mac", "win"]
metadata: {"found_by":"BPR-1121's 'Seen, not changed' section","reservation":"docs/issues/_reservations/BPR-1126--BPR-1126--97722ada3a17.json","related_cards":["BPR-1121","BPR-1118","BPR-0845"]}
---

# BPR-1126: Four sqlite storage contracts remove whatever is at /tmp/bpr-<store>-<N> before creating their scratch root

## Finding

[[BPR-1121]] saw this and left it for its own card. Four sqlite storage
contracts name each scratch root from a per-process counter, in the system
temporary directory. They remove whatever is at that name, and only then make
the directory. At `be8b0fad`:

| Row | Source | Name | Remove | Create |
| --- | --- | --- | --- | --- |
| `sqlite_appearance_settings_storage` | `tests/sqlite_contract/appearance_settings_storage_tests.cpp` | `bpr-appearance-<N>`, line 31 | line 32 | line 33 |
| `sqlite_queue_enrollment_storage` | `tests/sqlite_contract/queue_enrollment_storage_tests.cpp` | `bpr-queue-<N>`, line 38 | line 39 | line 40 |
| `sqlite_review_ledger_storage` | `tests/sqlite_contract/review_ledger_storage_tests.cpp` | `bpr-ledger-<N>`, line 104 | line 105 | line 106 |
| `sqlite_workbench_settings_storage` | `tests/sqlite_contract/workbench_settings_storage_tests.cpp` | `bpr-workbench-<N>`, line 32 | line 33 | line 34 |

`<N>` counts up from 0 in each process (`Counter`, lines 57, 88, 170 and 58).
Unless `TMPDIR` is set, the directory is `/tmp`, which every local user can
write. So each name is known before the run starts.

- **It removes what is not its own.** `std::filesystem::remove_all(Path)`
  removes whatever is at the name first: another user's directory, another
  run's root, or a link.
- **It can work in, and remove, what a link reaches.** A link put back at the
  name between the removal and `create_directories` is accepted: that call
  does not fail on a link to a directory. The constructor then makes `Path` the
  link's target with `std::filesystem::canonical` (lines 35, 42, 112 and 36).
  The course writes its databases there. At the end the destructor removes
  the target, with everything in it (lines 47, 54, 124 and 48).
- **The root is not private.** It is made with the umask's mode. Only the
  queue and ledger courses that call `make_private()` narrow it.

## Scope

- One shared helper makes every scratch root for the four contracts. The
  test makes the root itself, under a name no one can predict, created only
  if nothing is at that name. It is owner-private from creation and owned
  from creation by [[BPR-1121]]'s `scratch_guard`. It is removed only by the
  test that made it, and nothing is removed before it is made.
- The four sources use the helper instead of their own constructors.
- Search the other tests for a removal of a fixed or predictable temporary
  path. Record what is found. Fix a small one in the same class here, and
  give a title for anything larger.

## Acceptance

- AC1: No scratch root of the four contracts is made by removing something
  first. Each root is made by one shared helper: under a random name, created
  only if nothing is at the name, owner-private before anything is written in
  it, and owned from creation. None of the four sources removes anything
  before it makes a root.
- AC2: Negative control. At `HEAD`, a sentinel directory planted at each
  row's first name, `/tmp/bpr-<store>-0`, is removed by the row. The card
  records what `HEAD` does with a link planted at that name that points into
  a sentinel directory the trial owns. With the candidate, both plants and
  the sentinel directory's contents are left exactly as they were.
- AC3: The four Linux rows pass with the candidate and print the same lines
  as at `HEAD`, apart from random names. A passing run leaves no scratch root.
  Forced failures, made with [[BPR-1121]]'s trial mechanism, also leave none.
- AC4: The other tests were searched for a removal of a fixed or predictable
  temporary path. The card lists what was found, and what was fixed or given
  a title.
- AC5: The change is portable: the four sources build, and their rows pass,
  on macOS and Windows. The helper's Windows branch is compiled first on
  Windows.
- AC6: Gates: `/usr/bin/clang-format` 22, `git diff --check`,
  `msvc_secure_crt_contract`, `DOCUMENTATION_OK`, and no scoped `tidy.py`
  finding on a changed line.

## Boundaries

- Test sources and test support only. No product source, no row
  registration and no composition pin.
- Every run goes through `ctest`, serially and offscreen. `DISPLAY`,
  `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` are unset, and `HOME` and
  the XDG roots are fresh absolute paths. Nothing is created in
  `/run/user/1000`.
- Trial plants are made only at `/tmp` paths the trial checked were absent
  and made for itself. They hold only sentinel data the trial wrote. A
  link's target is a sentinel directory the trial made. Every plant is
  removed by its exact name afterwards.
