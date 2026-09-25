---
schema: "implement-rfc.card.v1"
id: "BPR-1139"
title: "The agent canary's default heartbeat marker is a pid-named file in the temporary directory, written through whatever is at that name"
status: "Ready"
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
