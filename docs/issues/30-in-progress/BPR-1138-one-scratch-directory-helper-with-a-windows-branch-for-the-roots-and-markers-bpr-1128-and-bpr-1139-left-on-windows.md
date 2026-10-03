---
schema: "implement-rfc.card.v1"
id: "BPR-1138"
title: "One scratch-directory helper with a Windows branch, for the Windows roots and markers that BPR-1128 and BPR-1139 left as they were"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-1128's 'Left for the follow-up' section and BPR-1139's 'Owed to bpr-main-win'","reservation":"docs/issues/_reservations/BPR-1138--BPR-1140--96a55e982e06.json","related_cards":["BPR-1128","BPR-1139","BPR-1126","BPR-1121","BPR-1229"],"written":"2026-10-01, from the reservation of 2026-09-25, when BPR-1128 and BPR-1139 were finished"}
---

# BPR-1138: One scratch-directory helper with a Windows branch, for the Windows roots and markers that BPR-1128 and BPR-1139 left as they were

## Why

[[BPR-1128]] made every POSIX root in its class with `mkdtemp`, at a name
nothing had, mode `0700` from creation. [[BPR-1139]] moved the agent
canary's default heartbeat marker on POSIX out of the temporary directory and
beside the canary's own executable, in the course's root. On Windows both
cards kept `HEAD`'s code, in `#if defined(_WIN32)` branches, because a root
there has to carry the access its row needs, and a row inside the
AppContainer needs the container's SID. The Windows branch of
`tests/sqlite_contract/scratch_root.hpp` was then still being changed.

That change has landed ([[BPR-1126]]). `create_directory_only_if_absent`
makes the directory with the platform's owner-private descriptor: the token
user's grant, and the AppContainer's when the process runs in one. So the
Windows hazard [[BPR-1128]] describes is still there, and the helper it
needs exists. Each kept branch below names its root from the clock or from an
address, makes it with `create_directory` or `create_directories`, which
return `false` and throw nothing when a directory or a link is already at the
name, and then works in whatever is there and removes it, with everything in
it.

Find the sites with:

```sh
grep -rn 'BPR-1128\]\]. Windows keeps' tests
grep -rn 'BPR-1139\]\]. Windows keeps' tests/fixtures
```

## The Windows remainders

**Part A, free to take now.** None of these needs the canary row.

| # (BPR-1128) | Row | Source | Windows root at `HEAD` |
| --- | --- | --- | --- |
| 1 | `sqlite_review_mutation_recovery_storage` | `tests/sqlite_contract/review_mutation_recovery_storage_tests.cpp` | `bpr-mutation-recovery-<address>`, then `test_platform::make_owner_private` |
| 5 | `sqlite_private_store_open` | `tests/sqlite_contract/private_store_open_tests.cpp` | `<name>-<clock>`, then the stores' rule |
| 7 | `contained_catalog_probe`, contained | `tests/sqlite_contract/contained_catalog_probe_tests.cpp` | `bpr-contained-catalog-probe-<clock>`, inheriting the container's temporary directory |
| 8 | `git_anonymous_public_read` | `tests/git_contract/anonymous_public_read_tests.cpp` | `bpr-git-anon-<clock>` |
| 9 | `git_cache_contract` | `tests/git_contract/git_cache_contract_tests.cpp` | `bpr-git-cache-<clock>` |
| 10 | `git_process_contract` | `tests/git_contract/git_process_contract_tests.cpp` | `bpr-<label>-<clock>` |
| 11 | `git_haves_trimmed_pack_update`, `_missing-object`, `_unconnected` | `tests/git_contract/haves_trimmed_pack_integration_tests.cpp` | `bpr-haves-<label>-<clock>` and its five children |
| 12 | `epoch_maintenance_integration` | `tests/git_contract/epoch_maintenance_integration_tests.cpp` | `bpr-maintenance-<label>-<clock>`; the two kill-recovery rows still pass a root their driver made |

And [[BPR-1139]]'s marker: `default_descendant_marker` in
`tests/fixtures/agent/agent_canary_main.cpp` returns
`bpr-agent-descendant-<pid>` in the temporary directory on Windows. Its one
user is `application_agent_invocation_cancellation_contract`, which is
`WINDOWS_UNCONTAINED`. The POSIX design puts the marker beside the canary's
copy, in the course's owner-private data root. Whether the launched canary
can write there on Windows depends on the access the agent launcher gives
it, and is measured here, not guessed.

**Part B, after [[BPR-1229]].** These three are in
`tests/adapters_qt/agent_launch_canary_tests.cpp`, whose row
`qt_agent_launch_canary` runs contained and stops on Windows at
`open durable child course`, before any of them. [[BPR-1229]] holds the
owner's containment decision for that row. Until the row runs past that
course on Windows, a change here cannot be shown to work inside the
container.

- `test_timeout_terminates_the_descendant_process_tree`: the marker
  `bpr-agent-descendant-<clock>`, removed before the descendant writes it.
- `test_breakaway_descendant_dies_with_supervised_job` ([[BPR-1128]] row
  13): the root `bpr-agent-breakaway-<clock>`, made with `create_directory`.
- `test_windows_assignment_failure_has_no_pre_assignment_side_effect`: the
  marker `bpr-agent-pre-assignment-<clock>`, removed before the child could
  write it.

## Scope

- One helper in `tests/support/` for both platforms, replacing
  `tests/support/scratch_directory.hpp`'s POSIX-only
  `make_scratch_directory` and sharing its Windows branch with
  `tests/sqlite_contract/scratch_root.hpp`: a random name, made only if
  nothing is there, owned by a `scratch_guard` from creation, with the token
  user's grant and the AppContainer's when contained. The SQLite contracts'
  `scratch_root` keeps its behaviour and its label shape.
- A helper used by a Git contract must not need the SQLite adapter. The
  stores' own rule stays with the rows that open a store.
- Each Part A site drops its `#if defined(_WIN32)` branch and uses the
  helper. A marker goes inside such a root, as on POSIX.
- Part B moves the same way once [[BPR-1229]] is decided. If the owner
  uncontains `qt_agent_launch_canary`, Part B still moves its three names
  into helper roots, and the AppContainer grant is beside the point there.
- `tests/AGENTS.md` names the one helper and drops the POSIX-only wording.

## Acceptance

- AC1: No source in Part A, and after [[BPR-1229]] none in Part B, names a
  Windows scratch root or marker from the clock, an address or a process id.
  `grep -rn 'Windows keeps' tests` finds none of these sites.
- AC2: Negative control on Windows. At this card's base, a trial hook, never
  committed, pins each Part A name to a value the trial knows. A sentinel
  directory planted there, and a directory junction planted there to a
  sentinel directory the trial owns, are used and removed by the row. With
  the change, every plant and its target are left exactly as they were.
- AC3: On Windows the Part A rows pass, `contained_catalog_probe` contained,
  and the cancellation contract finds its marker in the course's root. All
  eight `application_agent_invocation_*_contract` rows pass there: two of
  them failed on 2026-09-29 for [[BPR-1245]]'s cause, which that card has
  since repaired on macOS. Neither the user's temporary directory nor the
  AppContainer's gains an entry with any of the card's prefixes.
- AC4: After [[BPR-1229]], `qt_agent_launch_canary` passes on Windows with
  Part B moved, and the same listings stay clean.
- AC5: Portable: Linux and macOS build the changed sources with no warning,
  and [[BPR-1128]]'s and [[BPR-1139]]'s rows still pass there.
- AC6: Gates: `/usr/bin/clang-format` 22, `git diff --check`,
  `architecture_boundaries`, `msvc_secure_crt_contract`, `DOCUMENTATION_OK`,
  and no scoped `tidy.py` finding on a changed line.

## Boundaries

- Test sources, test support and `tests/AGENTS.md` only. No product source,
  no row registration and no composition pin. A row's containment is
  [[BPR-1229]]'s, not this card's.
- Trial plants only at names checked absent, holding only sentinel data, and
  removed by exact name afterwards.

## Part A — bpr-main-win, 2026-10-01

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, `build/windows-dev` Debug
with warnings as errors, at `02837c09`, with `main` later at `3ee80cb3`.
Part B is untouched and waits on [[BPR-1229]].

### The change

- **One helper.** `tests/support/scratch_directory.hpp`'s
  `make_scratch_directory` now has a Windows branch. It picks a random name,
  `<stem>-` and 16 hex digits, and makes it with `create_directory_only_if_absent`,
  which fails rather than reuse a taken name. The directory carries the
  platform's owner-private descriptor from creation: the token user's grant,
  and the AppContainer's when contained. A `scratch_guard` owns it from
  creation. It is resolved with `canonical`, or kept as made inside a
  container, which may not ask for the DOS form.
  - The stem stays a path, because `string()` is the ANSI code page on Windows.
    `sqlite_private_store_open`'s Unicode root (`bpr-0883-日本語-😀`) threw
    until it did.
  - POSIX keeps `mkdtemp`.
- **Shared with the SQLite contracts.** `tests/sqlite_contract/scratch_root.hpp`
  now takes `create_directory_only_if_absent`, `make_directory_at_random_name`
  and the resolution from the shared header. It keeps the stores' own check,
  `make_storage_root_owner_private`, and its label shape. The helper needs no
  SQLite adapter: its token functions come from
  `tests/sqlite_contract/platform_test_support.hpp`, which uses only system
  headers.
- **The eight Part A sites** drop their `#if defined(_WIN32)` branches and call
  the helper. They are rows 1, 5, 7, 8, 9, 10, 11 and 12. Mutation recovery
  keeps a Windows narrowing call, because its POSIX branch uses `chmod`.
  `grep -rn 'Windows keeps' tests` now finds only Part B's site, and a Python
  docstring that is not one.
- **[[BPR-1139]]'s marker.** `default_descendant_marker` now puts it beside the
  canary's own executable on Windows too, in the course's owner-private data
  root.
- **Build.** The four targets the helper newly reaches link
  `BetterPullReview::Platform` under `if(WIN32)`: haves, the catalog probe,
  mutation recovery and private-store-open. No row is registered or changed.
- `tests/AGENTS.md` names the one helper.

### AC2: plants at the name

A hook, never committed, pinned a name the trial knew. On the change side it
pinned the helper's first random draw, and printed whether that name was
taken. On the base side it put `HEAD`'s Windows branch back in three sources,
with the clock or address seed fixed. Before each run, the trial checked the
name was absent and planted one of two things there: a sentinel directory
holding `sentinel.txt`, or a directory junction to a sentinel directory the
trial owned.

| Row (root's name source at base) | Plant | Base (`HEAD`'s branch) | With the change |
|---|---|---|---|
| `git_cache_contract` (clock) | directory | **used and removed** | name reported `taken` 3 times; plant unchanged; row passes |
| | junction | **worked through it: the trial's sentinel target was deleted** | `taken`; junction and target unchanged; passes |
| `sqlite_review_mutation_recovery_storage` (address) | directory | **used and removed** | `taken` 10 times; unchanged; passes |
| | junction | **junction removed** | `taken`; unchanged; passes |
| `contained_catalog_probe` (clock, contained) | directory | **used and removed** | `taken` (inside the container's temporary directory); unchanged; passes |
| | junction | **junction removed** | `taken`; unchanged; passes |

AC2 was shown at these three sites, not all eight. They cover the clock, the
address and the contained case. Every changed site now goes through the one
helper, which the change side exercised. The hooks were removed:
`scratch_directory.hpp` is back to its pre-hook digest, and no `TRIAL` text
remains. Every plant and sentinel was removed by exact name.

### AC3: the rows

After the restore and a full rebuild, 28 of 28 pass, serially, in 449.45 s:

- the Part A rows: rows 1, 5 and 7 (the probe contained), the three Git
  contracts, the three haves rows, the epoch maintenance row and both epoch
  kill rows;
- all eight `application_agent_invocation_*_contract` rows. The cancellation
  contract finds its marker in the course's root;
- the `scratch_root` users: the ledger, queue, workbench and appearance
  storage rows, and `syntax_classification_contract`;
- `architecture_boundaries`, `coding_format_check` and
  `msvc_secure_crt_contract`.

The user's temporary directory and the AppContainer's were listed before and
after. Neither gained a `bpr-` entry.

### Still owed

- **AC4,** after [[BPR-1229]].
- **AC5:** a macOS and a Linux build of the changed sources, and
  [[BPR-1128]]'s and [[BPR-1139]]'s rows there. The POSIX paths are
  unchanged apart from the shared header's new cross-platform functions, which
  `scratch_root.hpp` compiles there.
- **AC6's** `tidy.py` on the changed lines; clang-format and `git diff --check`
  pass.

The card stays In Progress.

## macOS, AC5 and AC6 — bpr-main-mac, 2026-10-01

macOS 26.6.2 (25G83) on Apple Silicon, Apple clang 21.0.0, Qt 6.11.2,
`build/dev` Debug with warnings as errors, at `2ef17fd5`, which contains
Part A (`aba89bae`).

**The build.** Every source Part A changed was touched and the tree rebuilt:
91 steps, including both shared headers' includers, with no `warning:` line.

**The rows.** [[BPR-1128]]'s and [[BPR-1139]]'s macOS selectors together,
with the `scratch_root` users this card lists and the three gates, serially
and offscreen:

```sh
ctest --preset dev -j1 --output-on-failure -R '^(sqlite_(review_mutation_recovery_storage|profile_storage_root|profile_registry_storage|window_review_restoration_storage|private_store_open|device_binding_storage|review_ledger_storage|queue_enrollment_storage|workbench_settings_storage|appearance_settings_storage)|contained_catalog_probe|git_(anonymous_public_read|cache_contract|process_contract|haves_trimmed_pack_(update|missing-object|unconnected))|epoch_(maintenance_integration|kill_recovery|transaction_kill_recovery)|qt_agent_launch_canary|agent_parent_death|application_agent_invocation_(composition|construction_cut|launcher_construction_cut|gesture_cut|cancellation|staleness|cleanup_retry|workspace_diagnostic)_contract|application_agent_(materialization_)?crash_recovery|application_agent_finding_protocol|syntax_classification_contract|architecture_boundaries|coding_format_check|msvc_secure_crt_contract)$'
```

37 of 37 pass in 347.09 s, `qt_agent_launch_canary` among them (89.44 s).
The user's `TMPDIR` was listed before and after. It gained no entry with a
`bpr-` prefix. The only churn was the system's own `att-*-postings-*.bin`
files, which come and go without a run.

**AC6 on macOS.** `_coding-standard/scripts/tidy.py` with Homebrew
clang-tidy 23.1.1 over `build/dev`'s compilation database, on the ten `.cpp`
files Part A changed, reports eight findings. None is on a line Part A
changed. Blame puts each in an older commit:
`epoch_catalog_legacy_fixture.hpp:1` (`21255d9c`), `git_cache_contract_tests.cpp:1339`
(`654235fe`), `haves_trimmed_pack_integration_tests.cpp:106` (`bbdf8cc0`) and
`:667` (`9c1c9447`, four naming findings), and
`review_mutation_recovery_storage_tests.cpp:304` (`d9ce7daf`). There are no
findings in the two shared headers. `git diff --check` on `aba89bae` is clean,
and `coding_format_check`, `architecture_boundaries` and
`msvc_secure_crt_contract` pass in the run above.

**What this leaves.** AC5's macOS half and AC6's `tidy.py` are met. AC5 still
needs the Linux build and rows. AC4 waits on [[BPR-1229]]. The card stays
In Progress.

## AC5 on Linux — 2026-10-01

Run by bpr-main-win through `wsl` in the bpr-main-linux checkout
`/home/mmarcin/BetterPullReview`, which was offline, under the owner's
authorization relayed by the coordinator. Nothing was committed from WSL.
Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, Qt 6.11.2, `build/dev`
Debug, at `aac225a4`. `cmake --build --preset dev` printed no `warning:`
line. Rows ran serially through `ctest`, offscreen, with `DISPLAY`,
`WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` unset, and with fresh `HOME`
and XDG roots. No set added a `/tmp` entry. The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\linux\`, and are not
committed.

AC5, the Linux half. The changed sources build with no warning, and the 28
rows of the Part A selection pass, in 113.24 s:

- rows 1, 5 and 7 to 12;
- all eight `application_agent_invocation_*_contract` rows, the canary's
  marker among them;
- the `scratch_root` users;
- `architecture_boundaries` and `coding_format_check`.

On POSIX the helper's `make_scratch_directory` still uses `mkdtemp`. The
cross-platform `create_directory_only_if_absent` and
`make_directory_at_random_name`, which `scratch_root.hpp` now takes, compile and
run there. AC5's macOS half is the Mac station's.

## AC5 on Linux again — bpr-main-linux, 2026-10-01

Run by bpr-main-linux at `1b66d0487` (main, after `8fe4eafad`). WSL2, kernel
6.18.33.2, 20 cores, GCC 15.2, Qt 6.11.2, `build/dev` Debug.

- `cmake --preset dev && cmake --build --preset dev` exited 0, and the
  teed log `build/logs/bpr-station-rounds/1138-build.log` (not committed)
  has no `warning:` line.
- The coordinator's full 37-row selection ran with `ctest --preset dev -j1
  --output-on-failure`: **37 of 37 pass**, 231.94 s. Load average was 9.4
  at the start, from the build just finished, and 0.7 at the end. The selection
  covered the ten `sqlite_*` storage rows, `contained_catalog_probe`, the
  seven `git_*` rows, the three `epoch_*` rows, `qt_agent_launch_canary`,
  `agent_parent_death`, the eight `application_agent_invocation_*_contract`
  rows, both crash-recovery rows, `application_agent_finding_protocol`,
  `syntax_classification_contract`, `architecture_boundaries`,
  `coding_format_check` and `msvc_secure_crt_contract`.
- `/tmp` listed before the build and after the rows: no new `bpr*` entry.
  The 12 `bpr*` entries already present (`bpr-cycle-workbench-*` from
  2026-09-29, `bpr-run-*` runtime directories, and `bpr0905-four.log`)
  predate this run and are unchanged.

AC5's Linux half holds at this revision.

## AC3 on Windows, after BPR-1245 (bpr-main-win, 2026-10-03)

AC3's two `application_agent_invocation_*` rows that failed on 2026-09-29 for
[[BPR-1245]]'s cause were run again, with the other six. This was at
`d922bbee8` (MSVC 14.51, Qt 6.11.2, `build/windows-dev` Debug), serial, with
the executables deleted and rebuilt. All eight
`application_agent_invocation_*_contract` rows pass:

- composition 28.44 s;
- construction cut 3.38 s;
- launcher construction cut 3.33 s;
- gesture cut 6.89 s;
- cancellation 21.21 s, which finds its marker in the course's root;
- staleness 21.92 s;
- cleanup retry 21.08 s;
- workspace diagnostic 9.73 s.

The Part A rows passed in this card's earlier AC3 run, and nothing they
exercise has changed since. AC3 is met on Windows. Part B and AC4 still wait
on [[BPR-1229]].
