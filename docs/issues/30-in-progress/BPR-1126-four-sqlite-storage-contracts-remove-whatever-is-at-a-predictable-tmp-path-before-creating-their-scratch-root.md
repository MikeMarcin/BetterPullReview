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

## Fixed at `ad117024`, verified on Linux — bpr-main-linux, 2026-09-24

Claimed at `43c7cf32`. The change is one commit, `ad117024`, pushed without
a rebase. It touches test sources, one new test-support header and
`tests/AGENTS.md`. No product source, row registration or composition pin
changed.

### The change

- **One helper.** `tests/sqlite_contract/scratch_root.hpp` holds
  `bpr::tests::sqlite_contract::scratch_root`. The root is
  `bpr-<label>-` and a random part, in the system temporary directory.
  - On POSIX, `mkdtemp` makes it: a name nothing had, created mode `0700`.
  - On Windows, `make_directory_at_random_name` draws a 64-bit name from
    `std::random_device`. It creates the directory only if nothing is at
    that name, and draws again if the name is taken, up to 16 times. It
    then narrows the directory by the stores' own rule,
    `make_storage_root_owner_private`. That rule also grants the
    AppContainer that `sqlite_appearance_settings_storage` runs in on
    Windows. The function is compiled on every platform and called only on
    Windows.
  - [[BPR-1121]]'s `scratch_guard` owns the root from the moment it exists,
    until it is canonical. The destructor removes the root and everything
    in it. Nothing is removed before the root is made.
- **The four contracts.** Each derives its `scratch_directory` from the
  helper, with its own label. The per-process counter, the `remove_all`
  and the `create_directories` are gone. The queue and ledger contracts
  keep `make_private()`. It now narrows a root that is already private, so
  each course that depends on privacy still says so.
- **The syntax contract** (AC4, below). Its control root was the fixed
  `/tmp/bpr-syntax-canary-control`, removed and then made by `owned_tree`.
  It is now a `QTemporaryDir` at `bpr-syntax-canary-control-XXXXXX`.
- **`tests/AGENTS.md`** states the rule.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2. `build/dev` is Debug with
warnings as errors, and neither build printed a `warning:` line.
`HEAD`'s sources were built and trialled in `build/dev` at `43c7cf32`,
before the change. That commit changed only this card, so its test sources
are `be8b0fad`'s. Every run went through `ctest`, serially and offscreen.
`DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` were unset,
`TMPDIR` was unset unless a trial below sets it, and `HOME` and the XDG
roots were fresh absolute paths. No run added a `bpr-` name to
`/run/user/1000`.

**AC2, the negative control.** A trial script, not committed, planted
sentinels for each row. It planted only at a name it had just seen was
absent, and a link's target was a sentinel directory the trial had made in
`/tmp`. It removed every plant by its exact name afterwards.

| Trial | `HEAD` | Candidate |
| --- | --- | --- |
| A sentinel directory, holding a sentinel file, at `/tmp/bpr-<store>-0` | removed by all four rows | left in all four, with the same inode and the same file bytes |
| A link at that name, to a sentinel directory | the link is removed, and its target is left: `remove_all` does not follow a link | the link is left, with the same inode and target |
| The same link, put back by a thread whenever it is missing, while the row runs | followed on the first run by all four rows. The course worked in the target and then removed it, sentinel file and all | the link is not touched in five runs of each row, and the target is left |
| `TMPDIR` is a private directory holding sentinel directories at `bpr-<store>-0` to `-4` and links at `-5` to `-9` | 0 of the 10 plants left, in each row | 10 of 10 left, in each row. Afterwards the directory holds exactly the plants |

The syntax contract ran the first three trials at its fixed name. At `HEAD`
it removed the planted directory and the link. In the race trial it
followed the link and wrote `syntax/bpr-canary.xml` into the target. With
the candidate, all three plants were left.

**AC1, the roots as made.** A trial polled a private, empty `TMPDIR` while
each row ran. Appearance made 22 roots, queue 21, ledger 83 and
workbench 77, and the syntax contract made 1. Each root was a real
directory that this user owned, mode `0700`, at a random name such as
`bpr-queue-8gJzpQ`. One ledger course widens its root to `0750` on purpose
before it expects a refusal, and one mode was seen at that value. The
directory was empty after every run.

**AC3, the rows.**

| Check | Result |
| --- | --- |
| The four rows and `syntax_classification_contract`, verbose, at `HEAD` and with the candidate | all pass. Each prints the same lines as at `HEAD` apart from timings: 32, 32, 34, 32 and 32 lines |
| A passing run's top-level `/tmp` entries | none added by any of these rows |
| Forced failures with [[BPR-1121]]'s trial hook in the shared `expect`, not committed, in `build/core-only`, with `TMPDIR` private. N was a third and two thirds of each row's count: appearance 487 and 975 of 1463, queue 165 and 331 of 497, ledger 535 and 1071 of 1607, workbench 401 and 802 of 1204. The syntax contract failed at `the canary definition loads through an explicit search path`, while its control root exists | 9 of 9 fail with their `FAILED:` line and leave no root |
| The same failures with unwinding turned off (`BPR_TRIAL_NO_UNWIND`), the negative control that shows the trial can see a leak | 9 of 9 leave a root: one each, and two for workbench at 802 |
| The Windows branch on Linux, with the `#if` forced for a trial build and not committed: the four rows, the watch trial and the private-`TMPDIR` trial | all pass. Each root is `0700`, at a 64-bit hexadecimal name such as `bpr-workbench-1047e3515ae2370a`, and every plant is left |
| Integration: the 19 `sqlite_` and `syntax_` rows, after the whole `build/dev` graph was rebuilt | 19 of 19 pass |

After each trial build, the hooked or forced header was restored to its
recorded SHA-256 and the tree rebuilt.

**AC6, the gates.**

| Gate | Result |
| --- | --- |
| `/usr/bin/clang-format` 22 `--dry-run -Werror` on the six C++ files; `git diff --check` | pass |
| `architecture_boundaries`, `coding_format_check`, `msvc_secure_crt_contract` | pass |
| `_coding-standard/scripts/tidy.py` on the five sources, which also covers the new header | two findings, both in the syntax contract on lines 863 and 878 (`span`), which this change does not touch. None in the header |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The logs are station records and are not committed. They are in
`build/logs/bpr-1126/`:

- `candidate.patch`, the commit as tested;
- `build-*.log`, `gates.log`, `tidy-candidate.log` and
  `integration-sqlite-syntax.log`;
- `trial/`, holding the plant script `plant_trial.py`, BPR-1121's hook and
  `apply_hooks.py`, and the summaries `head-*.txt`, `candidate-*.txt`,
  `forced-summary.txt` and `windows-branch-on-linux.txt`. The per-run logs
  and `results.json` files are in `trial/head/`, `trial/candidate/`,
  `trial/forced/` and `trial/windows-branch/`.

### AC4: other tests that remove a temporary path

**Method.** The scan covered every `temp_directory_path`,
`QDir::tempPath` and literal `/tmp/` in `tests/`. It also covered every
removal followed within five lines by a creation, in C++ and Objective-C++,
Python's `gettempdir` and removals, and CMake's `REMOVE_RECURSE`.

- **Fixed here.** `tests/syntax_contract/syntax_classification_contract_tests.cpp`:
  `owned_tree` (lines 381 to 387 at `be8b0fad`) removed its root and then
  made it. The control root was the fixed
  `temp_directory_path() / "bpr-syntax-canary-control"` (lines 478 to 480).
  The same class also makes `plantedRoot`, below.
- **The same class, not changed here. Title given.** Two macOS-only
  sources remove a root named after the process id, then make it. This
  station builds neither.
  - `tests/platform/macos_capsule_identity_tests.cpp`, lines 116 to 119:
    `bpr-capsule-identity-<pid>`.
  - `tests/platform/macos_supervisor_identity_continuity_tests.cpp`,
    lines 530 to 534: `bpr-supervisor-identity-<pid>`. It also narrows
    the root with `permissions`, which follows a link.

  On macOS the temporary directory is per user, and under the CTest runner
  it is private, so the exposure is to this user's own processes.
- **Weaker, not changed here. Title given.** Thirteen roots are named from
  the clock (`steady_clock` nanoseconds), or from an address and the
  process id. None of them checks that the name was free. So a directory or
  a link already at that exact name is reused, and some of them narrow it
  with `permissions`, which follows a link. It is then removed at the end.
  To plant, someone would have to guess the exact name first. No fixed or
  counter name is removed.
  - `tests/sqlite_contract/`:
    - `review_mutation_recovery_storage_tests.cpp`, lines 41 and 42;
    - `profile_storage_root_tests.cpp`, lines 29 and 30;
    - `profile_registry_storage_tests.cpp`, lines 30 and 31;
    - `window_review_restoration_storage_tests.cpp`, lines 28 and 29;
    - `private_store_open_tests.cpp`, lines 71 to 73;
    - `device_binding_storage_tests.cpp`, lines 541 and 542;
    - `contained_catalog_probe_tests.cpp`, lines 33 to 37.
  - `tests/git_contract/`:
    - `anonymous_public_read_tests.cpp`, lines 62 and 63;
    - `git_cache_contract_tests.cpp`, lines 340 and 341;
    - `git_process_contract_tests.cpp`, lines 331 and 332;
    - `haves_trimmed_pack_integration_tests.cpp`, lines 194 to 198;
    - `epoch_maintenance_integration_tests.cpp`, lines 567 and 568.
  - `tests/adapters_qt/agent_launch_canary_tests.cpp`, lines 925 to 927,
    on Windows only.

  The same canary also removes a marker at a clock name before a
  descendant writes it: lines 679 to 685 on every platform, and lines
  1044 to 1050 on Windows only.
- **Not in the class.** These are made only at a name that was free. On a
  taken name they draw again or fail, and they remove nothing first:
  - `anonymous_epoch_authority`, `epoch_catalog_storage` and
    `epoch_catalog_behavior`: the process id and an attempt number;
  - `profile_runtime_lease`, `profile_activation`,
    `release_maintenance_files`, `review_submission_recovery_storage`,
    `live_review_submission_provider`, `designated_review_authority_bootstrap`,
    `windows_profile_storage_startup_contract`,
    `installed_resources_contract`, `gh_cli_transport` and the canary's
    working directory at line 364;
  - `mcp_context_helper`: 64 random bits;
  - the Secret Service and Credential Manager native tests: `mkdtemp`, and
    128 random bits;
  - the Linux launcher's own `/tmp/bpr-run-XXXXXX`.

  The CMake courses' `REMOVE_RECURSE` roots, `storage_root_tests` and
  `appcontainer_private_descriptor_tests` work in the build tree. The syntax
  contract's `plantedRoot` is removed and then made under Qt's test-mode
  data location in `HOME`, not in a temporary directory. The canary has to
  be planted at that standard path. The Python tests use
  `tempfile.mkdtemp`, the repository's contained temporary helpers, or
  paths under a root they made.

### Evidence owed elsewhere (AC5)

This station compiles the Windows branch but cannot run it on Windows, and
it runs nothing on macOS. On the macOS station:

```sh
cmake --build --preset dev
ctest --preset dev -j1 --output-on-failure -R '^(sqlite_(appearance_settings|queue_enrollment|review_ledger|workbench_settings)_storage|syntax_classification_contract)$'
```

On the Windows station, including `sqlite_appearance_settings_storage`,
which runs inside the AppContainer:

```powershell
cmake --build --preset windows-dev
ctest --preset windows-dev -j1 --output-on-failure -R "^(sqlite_(appearance_settings|queue_enrollment|review_ledger|workbench_settings)_storage|syntax_classification_contract|msvc_secure_crt_contract)$"
```

On each station, every row should pass. Afterwards no `bpr-appearance-`,
`bpr-queue-`, `bpr-ledger-`, `bpr-workbench-` or
`bpr-syntax-canary-control-` entry should be left in the temporary
directory. On Windows that also means the AppContainer's own temporary
directory for the appearance row. The card stays In Progress on this
station's claim until those runs are supplied.

## macOS, AC5 (bpr-main-mac, 2026-09-24)

Station `bpr-main-mac`: macOS 26.6.2 on Apple Silicon, Apple clang 21.0.0,
Qt 6.11.2, `build/dev` Debug with warnings as errors. The head was
`b8944a06`, which contains `ad117024`. This entry supplies the macOS run
that AC5 owes. It does not move the card, which stays on bpr-main-linux's
claim. The Windows run is still owed.

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | exit 0. It recompiled and relinked the five changed sources, and printed no `warning:` line |
| The card's `ctest` command | **5 of 5 passed**, in 3.58 s |
| A direct sandboxed run of each of the five rows, with a kept `TMPDIR` (below) | **5 of 5 passed. `TMPDIR` was empty afterwards**: no `bpr-appearance-`, `bpr-queue-`, `bpr-ledger-`, `bpr-workbench-` or `bpr-syntax-canary-control-` entry. `/private/tmp` gained no entry |

On macOS a ctest run cannot show a leftover ([[BPR-1047]]). The runner
gives the row its own `/tmp/bpr-net-*/fixture` as `TMPDIR` and removes it
when the row ends. So the leftover check is the direct run. It uses the
driver described in [[BPR-1121]]'s macOS entry. The driver runs the row's
command under the runner's own derived profile, from the row's working
directory, with the row's environment. `TMPDIR` is a `fixture/` directory,
mode `0700`, that the driver keeps and lists afterwards.

**The route sees a root when there is one.** [[BPR-1121]]'s trial hook
was rebuilt into the shared `expect` and never committed. A counting run
gave the same totals as Linux: appearance 1463, queue 497, ledger 1607 and
workbench 1204. The syntax contract gave 236. Each row was then failed at
Linux's first N (487, 165, 535 and 401), and the syntax contract at
`the canary definition loads through an explicit search path`.

| Row | Forced failure | The same with `BPR_TRIAL_NO_UNWIND` |
| --- | --- | --- |
| `sqlite_appearance_settings_storage` | exit 1, `FAILED:` line, `TMPDIR` empty | left `bpr-appearance-vcwGxe/` (`v4.sqlite3` and its `-shm`/`-wal`) |
| `sqlite_queue_enrollment_storage` | exit 1, `FAILED:` line, `TMPDIR` empty | left `bpr-queue-It5mvK/` (`version-five.db` and its `-shm`/`-wal`) |
| `sqlite_review_ledger_storage` | exit 1, `FAILED:` line, `TMPDIR` empty | left `bpr-ledger-kk4DZb/` (`l.db` and its `-shm`/`-wal`) |
| `sqlite_workbench_settings_storage` | exit 1, `FAILED:` line, `TMPDIR` empty | left `bpr-workbench-WLOp3f/` (`interrupted.sqlite3` and its `-shm`/`-wal`) |
| `syntax_classification_contract` | exit 1, `FAILED:` line, `TMPDIR` empty | left `bpr-syntax-canary-control-GFpytJ/syntax/bpr-canary.xml` |

So each row makes its root in the `TMPDIR` it is given, at a random name,
and a failure removes the root. The hook was then removed: the header is
byte for byte `1a2cdc9d…c6c25` again, and its dependents were rebuilt.
After that, the five rows passed once more, through ctest and by the
direct route, and each left `TMPDIR` empty. The kept roots were removed by
exact name. The logs and the per-run JSON are station records under
`build/logs/bpr-1126-mac/` and `build/logs/bpr-1121-mac/trial/`, and are
not committed.

**AC5 on macOS: met.** The four sources build here with no warning, and
their rows pass. So does the syntax contract's. No root is left. The
Windows run, which also compiles the helper's Windows branch there, is
still owed.

## Windows at `83e4ab72` (bpr-main-win, 2026-09-25): the helper compiles; the two rows inside the AppContainer are refused access to their own root

**MSVC compiles `scratch_root.hpp`'s Windows branch, and four of the six rows
pass. The two rows that run inside the AppContainer fail:** each is denied
access to the new root it just made there.

The run was in the station's main checkout at `83e4ab72`, which contains
`ad117024`. The tree held bpr-main-win's uncommitted [[BPR-1123]],
[[BPR-1124]] and [[BPR-1125]] edits, none of which these six rows build or
read. Windows 11 25H2, MSVC 19.51, Qt 6.11.2.

- **Build.** `build/windows-dev` (Visual Studio 18, Debug) was rebuilt with no
  error and no compiler warning, in 16 min 17 s. So the Windows branch of
  `make_directory_at_random_name` compiles; there is no compile finding.
- **Rows.** The command owed above, run from a `.cmd`, at
  2026-09-25T03:45:50Z to 03:46:01Z:

| Row | Registered | Result |
| --- | --- | --- |
| `msvc_secure_crt_contract` | contained | pass, 0.65 s |
| `sqlite_review_ledger_storage` | Windows-uncontained | pass, 2.27 s |
| `sqlite_queue_enrollment_storage` | Windows-uncontained | pass, 2.16 s |
| `sqlite_workbench_settings_storage` | Windows-uncontained | pass, 4.05 s |
| `sqlite_appearance_settings_storage` | contained | **fail**, 0.03 s |
| `syntax_classification_contract` | contained | **fail**, 0.23 s |

The two failures, verbatim:

```text
sqlite_appearance_settings_storage:
FAILED: unexpected exception: canonical: Access is denied.: "C:\Users\mikem\AppData\Local\Packages\bprautomatedtest\AC\Temp\bpr-appearance-7dec1f7dcbc81182"

syntax_classification_contract:
FAILED: unexpected exception: create_directories: Access is denied.: "C:/Users/mikem/AppData/Local/Packages/bprautomatedtest/AC/Temp/bpr-syntax-canary-control-hyYGyK\syntax"
```

**Why.** A contained row runs with the AppContainer's token, and its
temporary directory is the container's own,
`%LOCALAPPDATA%\Packages\bprautomatedtest\AC\Temp`. That directory grants
the container's SID full control, which a new child inherits:

```text
...\AC\Temp  S-1-15-2-2501375856-1686288671-3684386043-353760509-1080245170-1858810467-2136715157:(I)(OI)(CI)(CR)(F)
             NT AUTHORITY\SYSTEM:(I)(OI)(CI)(F)
             BUILTIN\Administrators:(I)(OI)(CI)(F)
             JAEGAW\mikem:(I)(OI)(CI)(F)
             Mandatory Label\Low Mandatory Level:(I)(OI)(CI)(NW)
```

An AppContainer's access check needs the container's SID granted as well as
the user's. The rows that pass run uncontained, with the user's own token.
The two failures differ:

- **`syntax_classification_contract`.** Its control root is now a
  `QTemporaryDir`: the six-character suffix is Qt's. The root it left behind
  carries an explicit list, not the inherited one. It gives full control to
  the owner, gives read-control, synchronize and read-attributes to
  `Everyone` and the low-integrity label, and names no container SID. So the
  contained row cannot create `syntax` inside its own root:

```text
...\AC\Temp\bpr-syntax-canary-control-hyYGyK  JAEGAW\mikem:(OI)(CI)(F)
                                              Mandatory Label\Low Mandatory Level:(OI)(CI)(Rc,S,RA)
                                              Everyone:(OI)(CI)(Rc,S,RA)
                                              Mandatory Label\Low Mandatory Level:(I)(OI)(CI)(NW)
```

- **`sqlite_appearance_settings_storage`.** Its root is the helper's
  `bpr-appearance-<64-bit hex>`, narrowed by `make_storage_root_owner_private`.
  That function's descriptor does name the container SID when the process
  runs in one: `private_security::load` reads `TokenAppContainerSid`. Even so,
  `std::filesystem::canonical` on the root is denied. The row removed the
  root as it failed, so its list could not be read here, and which access
  is missing is not established. Before `ad117024` the same course resolved
  a root that inherited `AC\Temp`'s list, and the row passed on Windows at
  `3f8f954d`.

This is the case this card's plan expected might fail: a course that relied
on the temporary directory's inherited access list. It is not a
portability slip, so nothing was changed here. Next steps for the owner:

- read the appearance root's list at the moment `canonical` is denied, with
  [[BPR-1121]]'s no-unwind trial;
- make the syntax control root in a way that grants the container SID, for
  example with the shared helper.

**Leftovers.** Checked before and after, by the five prefixes, in `%TEMP%`
and in the container's temporary directory:

- `%TEMP%`: no `bpr-appearance-`, `bpr-queue-`, `bpr-workbench-` or
  `bpr-syntax-canary-control-` entry. There are 77 `bpr-ledger-preflight-*`
  directories, the same 77 before and after the run. They are dated
  2026-08-18T18:42 to 2026-09-23T20:49 and are made by the product's review
  ledger preflight (`src/adapters/sqlite/src/review_ledger_database.cpp:910`),
  not by these rows, whose roots have no `preflight-` part. They are recorded
  here and were left alone.
- `AC\Temp`: **`bpr-syntax-canary-control-hyYGyK`**, left by the failed
  syntax row, empty, with the narrowed list above. It was removed afterwards
  by its exact name. The failed appearance row left no
  `bpr-appearance-` root there. The only other entry, `qt_temp-dxnvpP`,
  dates from 18:35, before the run, and was left alone.

The card stays In Progress. The logs are station records under
`build/logs/bpr-1126-main-win/`, not committed.
