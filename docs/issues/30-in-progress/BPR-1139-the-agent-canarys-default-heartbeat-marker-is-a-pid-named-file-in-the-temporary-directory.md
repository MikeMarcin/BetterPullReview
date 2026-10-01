---
schema: "implement-rfc.card.v1"
id: "BPR-1139"
title: "The agent canary's default heartbeat marker is a pid-named file in the temporary directory, written through whatever is at that name"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux", "mac", "win"]
metadata: {"found_by":"BPR-1128's 'Seen, not changed' item","reservation":"docs/issues/_reservations/BPR-1138--BPR-1140--96a55e982e06.json","related_cards":["BPR-1128","BPR-1121","BPR-1138"]}
---

# BPR-1139: The agent canary's default heartbeat marker is a pid-named file in the temporary directory, written through whatever is at that name

## Finding

[[BPR-1128]] found this and left it, in its "Seen, not changed" section.
Rechecked at `81d8da35`.

**The fixture.** `tests/fixtures/agent/agent_canary_main.cpp`:

- Lines 711 to 728. Run as `…-descendant-wait` without
  `--spawn-descendant`, the canary names its descendant's heartbeat marker
  `bpr-agent-descendant-<pid>` in `std::filesystem::temp_directory_path()`
  (lines 715 to 723). Nothing checks what is at that name. The canary prints
  it as `descendant-marker` (line 727) and starts the descendant (line 730).
  As `…-crash-descendant-wait` the marker is `.bpr-crash-heartbeat` in its
  working directory instead (lines 724 to 726). That name is not this card's.
- `heartbeat`, lines 379 to 387. The descendant opens the marker with
  `std::ofstream` in append mode every 10 ms. That follows a link, and
  creates a missing link target.
- `spawn_descendant`, lines 505 to 537. On POSIX the descendant is the
  canary's own `argv[0]`, run by `execl` (line 526).

**The course that runs it that way.**
`tests/end_to_end/agent_invocation_composition_public_path.cpp`, run by
`application_agent_invocation_cancellation_contract`
(`tests/end_to_end/CMakeLists.txt`, lines 1450 to 1513, `WINDOWS_UNCONTAINED`):

- Lines 629 to 661. The course copies the canary into its data root as
  `better_pull_review_agent_canary-descendant-wait` (lines 632 to 636, 641
  to 643). The data root is the `data` child of a `QTemporaryDir` the course
  made and narrowed to owner-private (lines 558 to 583).
- The application launches that copy. The external terminal protocol gets
  no arguments (`src/application/src/agent_launch.cpp`, lines 1815 and
  1816), and its profile passes only `HOME`, `PATH`, `LANG`, `LC_ALL`,
  `TMPDIR`, `TEMP` and `TMP` (`src/core/src/agent_profile.cpp`, lines 165
  to 171). So the course cannot pass `--spawn-descendant`, and the canary's
  temporary directory is the course's.
- On POSIX the supervisor runs the copy by the path the application was
  given (`src/app/src/github_application.cpp`, lines 6742 to 6749;
  `src/application/src/agent_launch.cpp`, line 1821;
  `src/adapters/agent/src/agent_process_launcher.cpp`, line 386;
  `src/adapters/agent/src/unix_agent_supervisor.cpp`, line 129). That is
  the copy's absolute path in the data root, and it is the canary's
  `argv[0]`.
- Lines 1396 to 1403. The course reads `descendant-marker`, checks the name
  exists, and cancels. Lines 1459 to 1471: it checks the marker stops
  growing, then removes whatever is at the name (line 1470).

So whatever is at `<temporary directory>/bpr-agent-descendant-<pid>` is
used. A file there is appended to, then removed. A link there is followed:
the heartbeat goes into its target, and the course removes the link. Every
failure the course reports before line 1470 (lines 1394, 1401, 1456, 1461
and 1466) returns through `fail` (lines 100 to 108) and leaves the marker in
the temporary directory. To plant, someone has to know the canary's process
id before it starts.

## Scope

- On POSIX the canary never names a marker in the temporary directory.
  - With `--spawn-descendant PATH` it writes `PATH`, as now.
  - As `…-descendant-wait` without it, the marker is
    `.bpr-descendant-heartbeat` in the directory that holds the canary's own
    executable. The caller chose that directory by putting the executable
    there. The canary refuses to start the descendant (exit 31, as for a
    missing marker now) unless its executable path is absolute.
  - The cancellation course's copy is in its owner-private data root, so
    its marker is in a directory the course made, owns and removes, on a
    failure as on a pass.
- `.bpr-crash-heartbeat` does not change.
- The course does not change. It reads the marker from `descendant-marker`,
  checks it exists, cancels, checks it stops growing, and removes it.
- Why not a `mkdtemp` directory made by the canary: the cancellation kills
  the canary and its descendant, so neither can remove it. The course would
  have to remove a directory it did not make, and a failure before it read
  `descendant-marker` would leave it.
- On Windows the canary keeps `HEAD`'s marker, in an `#if defined(_WIN32)`
  branch marked `[[BPR-1139]]`. Whether the canary there can write in the
  course's data root depends on the access the Windows station grants
  ([[BPR-1138]]), and is not guessed here. The Windows change is owed to
  bpr-main-win.

## Acceptance

- AC1: The hazard at `HEAD`. A trial hook, never committed, pins the
  canary's process id in the marker's name to a value no process can have.
  A sentinel file planted at that name is appended to by the heartbeat and
  removed at the course's end. A link planted there, to a sentinel file in
  a sentinel directory the trial owns, is followed: the heartbeat is
  appended to the sentinel file, and the course removes the link. A forced
  failure of the course after the descendant started leaves the marker in
  the temporary directory.
- AC2: The change, as scoped. With it, the AC1 plants are left exactly as
  they were: same inode, bytes and link target, and the sentinel
  directory's listing is unchanged. A trial watching the run sees the
  marker only inside the course's root, in a directory mode `0700` that
  this user owns, and nothing named `bpr-agent-descendant-` in the
  temporary directory.
- AC3: The Linux rows that run the canary pass and print the same lines as
  at `HEAD`, apart from random names and timings:
  `application_agent_invocation_cancellation_contract`, the other seven
  `application_agent_invocation_*_contract` rows,
  `application_agent_crash_recovery`,
  `application_agent_materialization_crash_recovery`,
  `application_agent_finding_protocol`, `qt_agent_launch_canary` and
  `agent_parent_death`. A passing run adds no top-level entry to the
  temporary directory.
- AC4: Forced failures and negative controls, with trial hooks never
  committed and each run in its own private `TMPDIR`. The cancellation
  course, failed at each check after the descendant started, reports its
  reason and leaves nothing ([[BPR-1121]]'s trial shape). The same failures
  ended with `std::exit`, which runs no destructor, leave the course's root,
  with the marker in it: the trial can see a leak. A descendant that leaves
  the process group and outlives the cancellation fails the course at
  `descendant-stop`, and a marker nobody writes fails it at
  `descendant-start`: the checks still discriminate at the marker's new
  place.
- AC5: The change is portable. The canary builds, and the AC3 rows pass,
  on macOS and Windows. On Windows the canary compiles `HEAD`'s marker.
- AC6: Gates: `/usr/bin/clang-format` 22, `git diff --check`,
  `architecture_boundaries`, `msvc_secure_crt_contract`, `DOCUMENTATION_OK`,
  and no scoped `tidy.py` finding on a changed line.

## Boundaries

- Test fixture and test documentation only. No product source, no row
  registration and no composition pin.
- `tests/sqlite_contract/scratch_root.hpp` is not edited, and no
  owner-only Windows access list is added anywhere.
- Every run goes through `ctest`, serially and offscreen. `DISPLAY`,
  `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` are unset, and `HOME` and
  the XDG roots are fresh absolute paths. Nothing is created in
  `/run/user/1000`.
- Trial plants are made only at paths the trial checked were absent. They
  hold only sentinel data the trial wrote. A link's target is in a sentinel
  directory the trial made. Every plant is removed by its exact name
  afterwards.

## Fixed on POSIX at `c11929e2`, verified on Linux — bpr-main-linux, 2026-09-25

Claimed at `e449fa9f`. The change is one commit, `c11929e2`, rebased over
commits that change only `docs/`. It touches
`tests/fixtures/agent/agent_canary_main.cpp` and `tests/AGENTS.md`. The
course, product sources, row registrations and composition pins are
unchanged.

### The change

- `default_descendant_marker` in the canary. As `…-crash-descendant-wait` it
  returns `.bpr-crash-heartbeat` in the working directory, as before.
  Otherwise, on POSIX, it returns `.bpr-descendant-heartbeat` beside the
  canary's own executable, or nothing when that path is relative. Then the
  canary prints no marker and exits 31. On Windows it returns `HEAD`'s
  `bpr-agent-descendant-<pid>` in the temporary directory, in an
  `#if defined(_WIN32)` branch marked `[[BPR-1139]]`.
- `tests/AGENTS.md`: a fixture that the application launches, and so cannot
  be given a path, writes beside its own executable, which the course copies
  into its root.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev` Debug with warnings as
errors. No build had a `warning:` line. Every run went through `ctest`,
serially and offscreen. `DISPLAY`, `WAYLAND_DISPLAY` and
`DBUS_SESSION_BUS_ADDRESS` were unset, `HOME` and the XDG roots were fresh
absolute paths, and no run added a `bpr-` name to `/run/user/1000`.

**AC1, `HEAD` (`e449fa9f`).** A trial hook, not committed, replaced the pid
in the marker's name with `4242424242`. No process can have that id
(`pid_max` is 4194304). `TMPDIR` was unset.

| Plant at `/tmp/bpr-agent-descendant-4242424242`, seen absent first | `HEAD` | Candidate |
| --- | --- | --- |
| A 35-byte sentinel file | Appended to: the poll saw 695 bytes, the sentinel text and 660 heartbeat bytes. Removed at the course's end. The row passes | Left: same inode and bytes. The poll never saw it grow |
| A link to a sentinel file in a sentinel directory the trial made | Followed: 665 heartbeat bytes appended to the sentinel file. The link removed. The row passes | Left: same inode and target. The sentinel file is unchanged |
| A link to an absent name in that directory | Followed: the descendant created `written-through` there and wrote 662 bytes. The link removed. The row passes | Left. Nothing created |

Other runs, each with a private `TMPDIR`:
- A watch at `HEAD` saw the marker at the top of `TMPDIR`, mode `0644`.
- The same watch with the candidate saw only the course's root at the top.
  The marker was `<root>/data/.bpr-descendant-heartbeat`. `data` and the
  root are both mode `0700`, owned by this user. Nothing was left.

**AC4, forced failures.** A trial hook in the course, not committed, failed
one check by name (`BPR_TRIAL_FAIL_AT`). With `BPR_TRIAL_NO_UNWIND`, `fail`
ended with `std::exit` instead of returning: the negative control, as in
[[BPR-1121]]'s trial. There were four points: `running-course`,
`descendant-start`, `typed-terminal-cleanup` and `descendant-stop`.

| | Returning (as committed) | `std::exit` (control) |
| --- | --- | --- |
| `HEAD` | 4 of 4 fail with their reason and leave `bpr-agent-descendant-4242424242` | 4 of 4 leave that and the course's root (28 entries) |
| Candidate | 4 of 4 fail with their reason and leave nothing | 4 of 4 leave the course's root (28 entries), with the marker in it |

Two negative controls used canary hooks, not committed:
- A descendant that calls `setpgid(0, 0)` escapes the supervisor's group
  kill. It beats for 25 s. The course fails at `descendant-stop`. The
  course's root was still removed. Nothing was left after 30 s, and no
  heartbeat process remained.
  - A first try beat for only 5 s. That ran out before the course
    cancelled, at about 6.6 s, so the row passed. That run is a
    calibration, not evidence.
- A descendant that writes `<marker>.elsewhere` fails the course at
  `descendant-start`, and nothing is left.

**AC3, the rows.** Two runs at `HEAD` were compared, 13 of 13, to find what
varies between runs: only timings. Then the candidate was compared with
`HEAD`. All 13 rows pass, and each prints the same lines as at `HEAD` once
timings are normalized:
- the eight `application_agent_invocation_*_contract` rows;
- `application_agent_crash_recovery` and
  `application_agent_materialization_crash_recovery`;
- `application_agent_finding_protocol`, `qt_agent_launch_canary` and
  `agent_parent_death`.

No run added a top-level `/tmp` entry.

Every hook was restored to its recorded SHA-256. The committed diff is
byte-identical to the tested candidate. The rebuilt canary and course carry
no `BPR_TRIAL`, `TRIAL_FORCING` or `setpgid` symbol. The cancellation row
was run once more on that build, and passed with the same output.

**AC6, the gates.**
- Pass: `/usr/bin/clang-format` 22 `--dry-run -Werror` and
  `git diff --check`.
- Pass: `architecture_boundaries`, `architecture_boundary_contract`,
  `coding_format_check` and `msvc_secure_crt_contract`.
- `_coding-standard/scripts/tidy.py --paths
  tests/fixtures/agent/agent_canary_main.cpp`: `clang-tidy passed`.
- `DOCUMENTATION_OK` after the rebase.

These station records are not committed. They are in
`build/logs/bpr-1139/`:
- `candidate.patch`, which is the commit;
- the build logs, `gates.log` and `tidy-candidate.log`;
- `head-baseline/`, `head-pinned/`, `candidate/`, `candidate-forced/`,
  `candidate-nc/` and `candidate-final/`, holding the per-run logs and
  JSON;
- `trial/`, holding `trial.py`, `compare.py`, `apply_hooks.py` and
  `bpr_trial_course_hook.hpp`, and the sentinel directories.

### Evidence owed (AC5)

On the macOS station:

```sh
cmake --build --preset dev
ctest --preset dev -j1 --output-on-failure -R '^(application_agent_invocation_(composition|construction_cut|launcher_construction_cut|gesture_cut|cancellation|staleness|cleanup_retry|workspace_diagnostic)_contract|application_agent_(materialization_)?crash_recovery|application_agent_finding_protocol|qt_agent_launch_canary|agent_parent_death)$'
ls "${TMPDIR:-/tmp}" | grep '^bpr-agent-descendant-'
```

Every row should pass, and the `ls` should print nothing.

On the Windows station, which still compiles `HEAD`'s marker:

```powershell
cmake --build --preset windows-dev
ctest --preset windows-dev -j1 --output-on-failure -R "^(application_agent_invocation_(composition|construction_cut|launcher_construction_cut|gesture_cut|cancellation|staleness|cleanup_retry|workspace_diagnostic)_contract|application_agent_(materialization_)?crash_recovery|application_agent_finding_protocol|qt_agent_launch_canary|agent_parent_death|msvc_secure_crt_contract)$"
```

Every row should pass. The card stays In Progress on this station's claim
until both runs are supplied.

### Owed to bpr-main-win

On Windows the marker is still `bpr-agent-descendant-<pid>` in the
temporary directory (`grep -rn 'BPR-1139\]\]. Windows keeps'
tests/fixtures`). The fix belongs with [[BPR-1138]]'s Windows access work,
or with a card of its own; the title was given to the coordinator. The
POSIX design, a marker beside the executable in the course's data root,
needs the canary to have access to that root on Windows.

The coordinator decided on 2026-09-25 that the Windows half folds into
[[BPR-1138]]. That card was not yet written when this note was added.

## macOS, AC5 (bpr-main-mac, 2026-09-25)

The station runs macOS 26.6.2 (25G83) on Apple Silicon, with Apple clang
21.0.0, Qt 6.11.2, and `build/dev` Debug with warnings as errors. This entry
supplies the macOS half of AC5. It does not move the card, which stays on
bpr-main-linux's claim. The Windows run is still owed.

**The build.** At `0f33ad29`, which contains `c11929e2`,
`cmake --build --preset dev` exited 0 after 305 steps and printed no
`warning:` line. It recompiled `agent_canary_main.cpp` and the invocation
course.

**The rows**, run with the card's macOS command from 05:50:38Z to 05:55:35Z:

| Rows | Result |
| --- | --- |
| The eight `application_agent_invocation_*_contract` rows | 8 of 8 pass. `application_agent_invocation_cancellation_contract` took 19.96 s |
| `application_agent_crash_recovery`, `application_agent_materialization_crash_recovery` | pass, 45.14 and 42.04 s |
| `application_agent_finding_protocol`, `qt_agent_launch_canary`, `agent_parent_death` | pass, 2.15, 72.51 and 24.35 s |
| In all | **13 of 13 pass**, 296.34 s |
| `ls "${TMPDIR:-/tmp}" \| grep '^bpr-agent-descendant-'`, and the same on `/private/tmp` | nothing printed. `/private/tmp` gained no entry. The user's `$TMPDIR` gained only a desktop indexer's and the editor's files |

That `ls` cannot see a row's marker on macOS ([[BPR-1047]]). Under ctest,
each row gets its own `/private/tmp/bpr-net-*/fixture` as `TMPDIR`, and the
runner removes it when the row ends. So the check that matters is a direct
run.

**The direct sandboxed run.** It used [[BPR-1128]]'s macOS driver, which
runs a row's command as `scripts/run_network_isolated_test.py` does, but
keeps the root and makes `fixture/` (mode `0700`) the `TMPDIR`. The driver
first checked each name below was absent, then planted at it in `fixture/`:

- sentinel directories at `bpr-agent-descendant-PLNTDR` and
  `bpr-agent-descendant-4242420000`, each holding a `heartbeat` file;
- a link at `bpr-agent-descendant-PLNTLK` to a sentinel directory outside
  `fixture/`;
- a sentinel file at `bpr-agent-descendant-4242420001`;
- a link at `bpr-agent-descendant-4242420002` to a sentinel file outside
  `fixture/`.

A thread listed `fixture/` every 2 ms while the row ran. Afterwards, every
entry under `fixture/` and under the link targets was compared with its
state before the run.

| Row | `TMPDIR` | Result | Top-level entries made during the run | Plants | Left |
| --- | --- | --- | --- | --- | --- |
| `application_agent_invocation_cancellation_contract` | resolved, under `/private/tmp` and `/private/var/folders` | pass, both, 19.9 s | only the course's own root, `better_pull_review_agent_invocation_composition_public_path-XXXXXX`, mode `0700`. **No `bpr-agent-descendant-` entry** | all 9 unchanged | nothing |
| The same | through the `/tmp` and `/var` links | pass, both | the same | all 9 unchanged | nothing |
| `qt_agent_launch_canary` | resolved, under `/private/tmp` | pass, 72.2 s | `bpr-agent-cwd-<clock>`, and `bpr-agent-descendant-XXXXXX`, mode `0700` | all 9 unchanged | nothing |

In the canary's row, `bpr-agent-descendant-XXXXXX` is [[BPR-1128]]'s
directory. The course makes it with `mkdtemp` for the path it passes as
`--spawn-descendant`, and removes it. It is not this card's default marker.
Only the cancellation course uses the default marker. Its course passes, so
the descendant wrote the marker and the course read it back. The marker was
never at the top of `TMPDIR`, and nothing was written through a plant. Each
driver root was removed by its exact name after its comparison. The logs,
the driver and the results are station records in
`build/logs/bpr-1126-1139-mac/`, not committed.

**AC5 on macOS: met.** The canary builds with no warning, and the 13 rows
pass. The Windows run is still owed.

## Windows, AC5 (bpr-main-win, 2026-09-29)

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, `build/windows-dev`
Debug with warnings as errors. This entry supplies the Windows half of AC5.
It does not move the card, which stays on bpr-main-linux's claim.

**The build.** `main` at `22d18416` did not build on Windows, for five
reasons outside this card. They are fixed in `537f718d`, which changes no
line of the canary or the course. With it, the whole graph builds with no
`warning` line. The canary compiles `HEAD`'s marker: the `#if
defined(_WIN32)` branch at `agent_canary_main.cpp:567` returns
`bpr-agent-descendant-<pid>` in the temporary directory, as scoped.

**The rows**, run with the card's Windows command, serially:

| Rows | Result |
| --- | --- |
| `application_agent_invocation_cancellation_contract`, the one course that uses the default marker | pass, 19.49 s. Again at `f9870baa`: pass, 21.12 s |
| `…_construction_cut`, `…_launcher_construction_cut`, `…_gesture_cut`, `…_staleness`, `…_cleanup_retry` contracts | 5 of 5 pass |
| `application_agent_crash_recovery`, `application_agent_materialization_crash_recovery`, `application_agent_finding_protocol`, `agent_parent_death` | 4 of 4 pass |
| `msvc_secure_crt_contract` | failed at `22d18416`: `tour_owner_channel_tests.cpp:170`, a POSIX-only `getenv` the text scan cannot see is in `#if !defined(_WIN32)`. Given the scoped C4996 spelling in `537f718d`, it passes |
| `application_agent_invocation_composition_contract`, `application_agent_invocation_workspace_diagnostic_contract` | **fail**: `agent-status=Not started: Choose immutable revisions or findings in Private history, then Ask agent…`. This is the private-run refusal that [[BPR-1211]] recorded failing at its baseline on macOS. It comes from RFC-BPR-0023's admission, not from the marker |
| `qt_agent_launch_canary` | **fails** contained in 0.05 s at `open durable child course`, `agent_launch_canary_tests.cpp:281`. That course came with RFC-BPR-0023 (`8e20e9e8`, 2026-09-27). It opens a private SQLite store under `scratch_root` inside the AppContainer, the shape that fails for the other new store rows on this station. It runs before any canary launch |
| In all | 11 of 14 pass |

The temporary directory and the AppContainer's own temporary directory were
listed before and after. Neither gained a `bpr-agent-descendant-` entry.

**AC5 on Windows: not met, for causes outside this card.** The canary
builds and compiles `HEAD`'s marker. The course that uses the marker passes,
and so do nine of the other twelve rows. The three that fail are red for
reasons this card does not touch. Two fail with the same refusal that
[[BPR-1211]] recorded at `main` on macOS. One is an RFC-BPR-0023 course that
fails contained on Windows. The logs are
station records in `C:\Users\mikem\bpr-main-helpers\run\station-0929\`
(`1139-1128.log` and `.xml`, `pass2.log`), and are not committed.

## Finished on bpr-main-mac (2026-10-01)

bpr-main-linux's claim was taken over by the Mac integrator, on the
coordinator's direction. No source changed in this step.

**The integration pass at `4aa6f544d`.** macOS, `build/dev` Debug, current
with no `warning:` line. The card's 13 rows ran serially inside a 103-row
pass with [[BPR-1121]]'s and [[BPR-1128]]'s selections, from 15:24:56Z to
16:04:10Z:

| Rows | Result |
| --- | --- |
| 12 of the 13, among them `application_agent_invocation_cancellation_contract` (20.15 s), the composition and workspace-diagnostic contracts that [[BPR-1245]] has since repaired on macOS, and `qt_agent_launch_canary` (76.42 s) | pass |
| `application_agent_invocation_staleness_contract` | failed once, after 64.62 s: `agent-terminal-status=Not started: agent_session_catalog_unavailable`. The application refused to start the agent, because the review ledger answered its session-catalog read with `unavailable`. The course runs the canary as `stale-wait`, which never uses the default marker. Run alone straight afterwards it passed twice, in 22.11 and 21.31 s. Filed as [[BPR-1254]] |
| `/private/tmp`, listed before and after | no `bpr-` entry added |

**The judgment.**

- **AC5 is met for this card's change.** The canary builds on macOS and
  Windows, and on Windows it compiles `HEAD`'s marker, as AC5 requires. The
  one course that uses the default marker, the cancellation contract,
  passes on all three platforms. The other reds are owned elsewhere:
  - Windows, `qt_agent_launch_canary`: [[BPR-1229]]. The row stops at an
    RFC-BPR-0023 course before any canary launch.
  - Windows, the composition and workspace-diagnostic contracts: the private-run
    refusal that [[BPR-1245]] records and has repaired on macOS. Their
    Windows rerun is part of [[BPR-1138]]'s AC3, which runs all eight
    invocation contracts there.
  - macOS, the staleness contract, once: [[BPR-1254]].
- **The Windows marker moves to [[BPR-1138]]**, Part A, as the coordinator
  decided on 2026-09-25. Its user, the cancellation contract, is
  `WINDOWS_UNCONTAINED`, so it does not wait on [[BPR-1229]].
- AC1 to AC4 and AC6 stand on the Linux entry.

The logs are station records in `build/logs/bpr-1121-1128-1139-takeover/`
(`integration.log`, `staleness-rerun.log`), not committed.
