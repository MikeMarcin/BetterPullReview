---
schema: "implement-rfc.card.v1"
id: "BPR-1126"
title: "Four sqlite storage contracts remove whatever is at /tmp/bpr-<store>-<N> before creating their scratch root"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux", "mac", "win"]
metadata: {"found_by":"BPR-1121's 'Seen, not changed' section","reservation":"docs/issues/_reservations/BPR-1126--BPR-1126--97722ada3a17.json","related_cards":["BPR-1121","BPR-1118","BPR-0845"]}
acceptance: [{"criterion":"AC1 No scratch root of the four contracts is made by removing something first; one shared helper makes each root under a random name, created only if nothing is at the name, owner-private before anything is written in it, and owned from creation; none of the four sources removes anything before it makes a root","outcome":"pass","revision":"dbe1bcb5","evidence":{"kind":"command","detail":"Linux (bpr-main-linux, ad117024): tests/sqlite_contract/scratch_root.hpp; a poller over a private TMPDIR saw appearance 22, queue 21, ledger 83, workbench 77 and syntax 1 roots, each a real 0700 directory at a random name. Windows (bpr-main-win, dbe1bcb5 on 1ec9157c): the Windows branch creates the root with CreateDirectoryW and the BPR-0825 platform descriptor; a poller read 152 roots while the rows ran, all O:user with a protected list (user and container in AC\\Temp, user only in %TEMP%), none with an inherited list, against 2 of 152 with %TEMP%'s inherited list when the root was created and then narrowed.","path":"docs/issues/60-done/BPR-1126-four-sqlite-storage-contracts-remove-whatever-is-at-a-predictable-tmp-path-before-creating-their-scratch-root.md#windows-the-fix-bpr-main-win-2026-09-25-the-contained-rows-make-and-resolve-their-root"}},{"criterion":"AC2 Negative control: at HEAD a sentinel directory planted at each row's first name is removed and the link behaviour is recorded; with the candidate both plants and the sentinel directory's contents are left exactly as they were","outcome":"pass","revision":"ad117024","evidence":{"kind":"command","detail":"Linux: at HEAD all four rows removed the planted sentinel directory, removed the link without following it, and followed a racing link on the first run; with the candidate every plant (directory, link, raced link, 10 in a private TMPDIR) was left with the same inode and bytes. Windows (dbe1bcb5, uncommitted trial hook): a sentinel directory with a sentinel file at each root's first drawn name, 153 plants across the five rows, all left intact while every row passed; a sentinel file at each name, 153 of 153 left.","path":"docs/issues/60-done/BPR-1126-four-sqlite-storage-contracts-remove-whatever-is-at-a-predictable-tmp-path-before-creating-their-scratch-root.md#verification-on-linux"}},{"criterion":"AC3 The four Linux rows pass with the candidate and print the same lines as at HEAD apart from random names; a passing run leaves no scratch root; forced failures made with BPR-1121's trial mechanism also leave none","outcome":"pass","revision":"ad117024","evidence":{"kind":"command","detail":"bpr-main-linux: the four rows and syntax_classification_contract pass and print 32, 32, 34, 32 and 32 lines, the same as HEAD apart from timings; no /tmp entry added; forced failures at a third and two thirds of each row's count and the syntax contract's control-root step: 9 of 9 fail and leave no root, and with unwinding off 9 of 9 leave one.","path":"docs/issues/60-done/BPR-1126-four-sqlite-storage-contracts-remove-whatever-is-at-a-predictable-tmp-path-before-creating-their-scratch-root.md#verification-on-linux"}},{"criterion":"AC4 The other tests were searched for a removal of a fixed or predictable temporary path; the card lists what was found, and what was fixed or given a title","outcome":"pass","revision":"ad117024","evidence":{"kind":"observation","detail":"The card's AC4 section: the syntax contract's fixed control root was fixed here; two macOS-only pid-named roots and thirteen clock- or pid-named roots were listed and given titles (BPR-1128 took the clock-named class); the not-in-class list is recorded.","path":"docs/issues/60-done/BPR-1126-four-sqlite-storage-contracts-remove-whatever-is-at-a-predictable-tmp-path-before-creating-their-scratch-root.md#ac4-other-tests-that-remove-a-temporary-path"}},{"criterion":"AC5 The change is portable: the four sources build, and their rows pass, on macOS and Windows; the helper's Windows branch is compiled first on Windows","outcome":"pass","revision":"dbe1bcb5","evidence":{"kind":"command","detail":"macOS (bpr-main-mac, b8944a06 containing ad117024): build clean, the card's ctest command 5 of 5, and a direct sandboxed run left TMPDIR empty. Windows (bpr-main-win, dbe1bcb5 on 1ec9157c, windows-dev Debug, MSVC 19.51, Qt 6.11.2): the five targets build with no warning; the card's ctest command passes 6 of 6 including msvc_secure_crt_contract and the two contained rows; %TEMP% and the AppContainer's AC\\Temp hold no bpr-appearance-, bpr-queue-, bpr-ledger-, bpr-workbench- or bpr-syntax-canary-control- entry before or after. The contained rows' earlier failure was canonical refused inside the container for every path (BPR-0362 dos-volume-name), not a missing right. POSIX compiles only one new line, the #else branch of create_directory_only_if_absent (std::filesystem::create_directory), not yet built by Linux or macOS; the rows' POSIX code is otherwise unchanged.","path":"docs/issues/60-done/BPR-1126-four-sqlite-storage-contracts-remove-whatever-is-at-a-predictable-tmp-path-before-creating-their-scratch-root.md#windows-the-fix-bpr-main-win-2026-09-25-the-contained-rows-make-and-resolve-their-root"}},{"criterion":"AC6 Gates: clang-format 22, git diff --check, msvc_secure_crt_contract, DOCUMENTATION_OK, and no scoped tidy.py finding on a changed line","outcome":"pass","revision":"dbe1bcb5","evidence":{"kind":"command","detail":"Linux at ad117024: /usr/bin/clang-format 22 and git diff --check clean; architecture_boundaries, coding_format_check, msvc_secure_crt_contract pass; tidy findings only on untouched syntax lines; DOCUMENTATION_OK. Windows at dbe1bcb5: clang-format 22.1.8 --dry-run -Werror and git diff --check clean; coding_format_check, architecture_boundaries and msvc_secure_crt_contract pass; tidy.py on the appearance and syntax sources exit 0 with no finding on a changed line or in scratch_root.hpp; DOCUMENTATION_OK; gate-push passed.","path":"docs/issues/60-done/BPR-1126-four-sqlite-storage-contracts-remove-whatever-is-at-a-predictable-tmp-path-before-creating-their-scratch-root.md#gates"}}]
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

## Windows at `79a96104` (bpr-main-win, 2026-09-25): the helper compiles; the two rows inside the AppContainer are refused access to their own root

**MSVC compiles `scratch_root.hpp`'s Windows branch, and four of the six rows
pass. The two rows that run inside the AppContainer fail:** each is denied
access to the new root it just made there.

The run was in the station's main checkout at `79a96104`, which contains
`ad117024`, with bpr-main-win's card claims on top. The tree held bpr-main-win's uncommitted [[BPR-1123]],
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

## Windows, the fix (bpr-main-win, 2026-09-25): the contained rows make and resolve their root

**All six rows pass on Windows, and no row leaves a root.** The two contained
rows failed because `canonical` is refused inside an AppContainer. No
permission on the root was missing. The Windows branch now handles that
refusal. It also makes the root owner-private from creation, with the
[[BPR-0825]] platform descriptor. The syntax contract uses the same helper
on Windows.

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, `build/windows-dev`
(Debug), display scale 150%. The candidate was built on main at `1ec9157c`.
Every row ran through `ctest`, serially.

### The cause: `canonical` is refused inside a container, for every path

The earlier Windows entry could not say which permission was missing. A
trial, not committed, kept the appearance root when `canonical` refused it,
and read its list from outside:

```text
...\AC\Temp\bpr-appearance-73335154ec3b963e  JAEGAW\mikem:(OI)(CI)(F)
                                             S-1-15-2-2501375856-...-2136715157:(OI)(CI)(F)
                                             Mandatory Label\Low Mandatory Level:(I)(OI)(CI)(NW)
```

The owner and the container both have full control, so no permission on the
root was missing.

A probe, also not committed, ran inside the container through the row
launcher, and outside it as the control. It opened each directory and asked
for its final name in each form:

| Directory | Open | DOS form | GUID form | NT form | NONE form | `canonical` |
| --- | --- | --- | --- | --- | --- | --- |
| `AC\Temp` itself, which grants the container full control | opens | error 5 | error 5 | ok | ok | error 5 |
| a new directory that keeps the inherited list | opens | error 5 | error 5 | ok | ok | error 5 |
| a new directory narrowed to the platform descriptor (owner and container) | opens | error 5 | error 5 | ok | ok | error 5 |
| the same three, outside the container, under `%TEMP%` | open | ok | ok | ok | ok | ok |

MSVC's `canonical` asks for the DOS form. A container may not read which
drive letter a volume has, so it is refused for every path. This is
[[BPR-0362]]'s `dos-volume-name`. [[BPR-1108]] measured the same refusal for
`sqlite_release_maintenance_files`.

The earlier entry said that before `ad117024` this course "resolved a root
that inherited `AC\Temp`'s list, and the row passed". That is not so. The
old constructor called `canonical(Path, ignored)` and kept the unresolved
path when it failed. So inside the container it never resolved. The row
passed only because the error was ignored.

**The syntax contract** has a different cause. On Windows, `QTemporaryDir`
gives its root a list of its own: full control for the owner, and
read-control, synchronize and read-attributes for `Everyone` and the
low-integrity label. The list names no container. The probe made a directory
with an owner-only list from inside the container. The contained process
could not open it, create a child in it, open it with `WRITE_DAC` to
re-narrow it, or remove it. So narrowing Qt's root afterwards cannot work.
That is also why the failed syntax row left its root in `AC\Temp` at `HEAD`.

### The change

It is test support only. No product source, row registration or composition
pin changed.

- **`tests/sqlite_contract/scratch_root.hpp`:**
  - `create_directory_only_if_absent` makes the Windows root with
    `CreateDirectoryW` and the [[BPR-0825]] platform descriptor. That
    descriptor comes from `bpr/platform/windows_private_object.hpp`; there
    is no second descriptor builder.
    - The token user owns the root and has the one full-access grant. The
      container the process runs in, if any, has the second.
    - `ERROR_ALREADY_EXISTS` becomes `std::errc::file_exists`, and another
      name is drawn.
    - So the root never carries the temporary directory's inherited list.
      `make_storage_root_owner_private` still checks it before anything is
      written.
    - On POSIX the function is `std::filesystem::create_directory`, and it is
      not called there.
  - `resolved_root` runs on Windows only. It keeps the path as made only when
    `canonical` fails with `permission_denied` and the process runs in an
    AppContainer. Any other failure still fails the course.
- **The syntax contract** makes its control root with `scratch_root` on
  Windows. On POSIX its lines are byte for byte `ad117024`'s, inside
  `#else`.
- **CMake:**
  - The four SQLite targets link `BetterPullReview::Platform` on Windows.
  - The syntax target links `BetterPullReview::AdapterSQLite` and
    `BetterPullReview::Platform` on Windows.

On POSIX, the only code this change compiles is the one-line `#else` branch
of `create_directory_only_if_absent`. Linux and macOS have not built it. The
rows' POSIX behaviour is unchanged: `mkdtemp` still makes the root.

### Verification

| Check | `HEAD` (`3da31f2a`/`444f3e10`) | Candidate |
| --- | --- | --- |
| The card's `ctest` command, with `msvc_secure_crt_contract` | 4 of 6: appearance fails `canonical: Access is denied`, and syntax fails `create_directories: Access is denied` | **6 of 6 pass** (last run 05:33:12Z to 05:33:25Z) |
| Leftovers in `%TEMP%` and `AC\Temp` under the five prefixes | the syntax row leaves `bpr-syntax-canary-control-XXXXXX`, with Qt's list | none. The listings before and after are identical |
| Roots as made: a poller read each root's owner and list while the rows ran | 2 of 152 roots seen with `%TEMP%`'s inherited list, which gives two principals other than the owner, SYSTEM and Administrators modify rights. This was the first candidate, which still created the root the old way and narrowed it afterwards | all 152 seen are `O:user`, with a protected list: the user and the container (the 22 appearance roots and the syntax root, in `AC\Temp`), or the user only (the 129 ledger, queue and workbench roots, in `%TEMP%`) |

These trials used hooks that were not committed. The hook was reverted
afterwards, and the headers were restored to their recorded SHA-256:

- **Names that are taken.** A sentinel directory holding a sentinel file was
  planted at each root's first drawn name, 153 plants across the five rows.
  Every row passed, and all 153 plants were left intact. The same trial with
  a sentinel file at each name also left 153 of 153 intact.
  `CreateDirectoryW` refused each one, as `file_exists`, and another name was
  drawn.
- **Forced failures, with unwinding.** Nine runs: each SQLite row at a third
  and at two thirds of its expectations, and the syntax row at "the canary
  definition loads through an explicit search path". Five left no root:
  appearance at 502, queue at 337, ledger at 529 and 1059, and syntax. Four
  left their root, all while a raw `sqlite3` connection the course had
  opened was still open: queue at 168, appearance at 1005, and workbench at
  427 and 855. Windows cannot remove an open file, so `remove_all` fails
  silently. This is not caused by this change. It is reserved as
  [[BPR-1141]], whose card is not written yet.
- **Forced failures, without unwinding** (the negative control). All five
  rows left their root. Each root had the owner and the container (contained
  rows), or the owner only.

### Gates

| Gate | Result |
| --- | --- |
| clang-format 22.1.8 `--dry-run -Werror` on the changed C++ files; `git diff --check` | clean |
| `coding_format_check`, `architecture_boundaries` (windows-dev) | pass |
| `msvc_secure_crt_contract` | pass |
| `_coding-standard/scripts/tidy.py` on the appearance and syntax sources (`build/dev`) | exit 0, `clang-tidy passed`. None of the three findings is on a changed line: the syntax contract's `span` on lines 879 and 894 (Linux's 863 and 878, shifted by the added lines), and appearance line 753 (`bugprone-suspicious-missing-comma`), which this change does not touch. Nothing in `scratch_root.hpp` |
| `python scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The logs are station records and are not committed. They are under
`build/logs/bpr-1126-main-win/`, in `s12-*` and `final2-*`, and the probe and
trials are in `trial/`.
