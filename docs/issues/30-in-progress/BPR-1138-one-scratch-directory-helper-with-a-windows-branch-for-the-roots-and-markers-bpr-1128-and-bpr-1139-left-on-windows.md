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
