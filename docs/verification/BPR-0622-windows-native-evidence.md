# BPR-0622 Windows native evidence

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-workspace-recovery`, run `run-20260916-stations`, dispatch
`dsp-001-r2`, mandate r2. Base `743c59d79b09f18d4178f0702a0f377f53082ccd`.

**Host.** Windows 11 Home 10.0.26200; Visual Studio 18 Community, MSVC
14.51.36231; Qt 6.10.3 `msvc2022_64`. Smart App Control is off. The shell token
is not elevated, and its owner SID equals its user SID.

This file grows one section per verified slice. The card
[[BPR-0622]] stays integrator-owned: nothing here moves or edits it.

## Job and lock semantics measured before choosing a design

The measurements used a throwaway native probe, kept outside the repository.
Each run involved these processes:

- an owner, which created a job with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, a
  completion port associated with that job, and a guard file;
- an agent, created suspended, assigned to the job, then resumed;
- a grandchild of the agent, which inherited nothing and held a workspace file
  open without delete sharing;
- in some runs, a holder process outside the job.

A driver killed the owner with `TerminateProcess` and polled the results at
about 1 ms resolution.

| Scenario | Observation (ms after the owner's death) |
|---|---|
| Only the owner holds the job handle | The agent and grandchild exited at 1–3 ms, and the workspace became renamable at the same moment. Kill-on-close fires. |
| The owner takes a shared `LockFileEx`; the holder keeps a duplicated handle | An independent exclusive lock succeeded at **0 ms**, while the holder still held the handle (it released at 2009 ms). **The lock belongs to the process that took it, not to the handle.** |
| The child inherits the owner's locked handle | Exclusive succeeded at 1 ms. The inherited handle does not keep the lock. |
| The holder opens the guard and takes its own shared lock | Exclusive failed until the holder released, at 2006 ms. |
| Share-mode guard: open without `FILE_SHARE_WRITE`; exclusivity means an open with share mode 0 | Exclusive failed until the holder's duplicate closed, at 2005 ms. Share-mode exclusion follows handles. |
| The job handle is duplicated into a holder that never acts | The agent was still alive at 1500 ms. **Duplication defeats last-handle kill-on-close.** |
| The holder watches the owner, then calls `TerminateJobObject` | The completion port delivered `NEW_PROCESS` twice, then `ACTIVE_PROCESS_ZERO`, and `ActiveProcesses` was then 0. From owner death to confirmed empty took at most 1 ms. The port handle duplicates into the holder. |
| Owner and holder both killed | Kill-on-close fires. The guard release and the agent's exit coincide at 1–2 ms resolution. |

**Design consequence.** Every holder takes its own lock, and the supervisor sits
outside the child's job. That supervisor holds the job, its completion port and
its own shared guard lock. It confirms the job is empty before releasing the
guard.

**Double-kill window.** If the owner and the supervisor are both killed, there
is **no exclusion guarantee**. Termination is asynchronous, so the guard can be
released while job processes are still being torn down. This is the Windows
analogue of SIGKILLing the Unix supervisor, which is also unguarded, and this
file claims no exclusion there.

## Slice (a): Windows supervisor, launcher guard, parent-death course

### Source

- **`src/adapters/agent/src/windows_agent_supervisor.cpp`** (new). This is the
  bundled `better_pull_review_agent_supervisor.exe`, built without Qt. It
  performs these steps in order:
  1. Reads a bounded launch specification from an inherited owner pipe: magic,
     version, flags, executable, command line, guard path and guard identity.
  2. Opens the guard itself and takes its own shared lock.
  3. Creates a kill-on-close job with an associated completion port. It never
     sets breakaway flags.
  4. Creates the child suspended, with a handle list limited to its three
     standard handles, then assigns it to the job and resumes it.
  5. Reports the start result.
  6. Waits for the child to exit or the owner pipe to reach end-of-file.
  7. Terminates the job and confirms it empty.
  8. Cancels its watcher's synchronous read and joins it, and only then releases
     the guard.
- **`src/adapters/agent/include/bpr/adapters/agent/windows_workspace_guard.hpp`** (new, header-only; moved from `src/` in slice (b)).
  It provides:
  - the regular-file identity `{1, volume serial, file index high, file index
    low}`, the same kind-1 encoding the Git adapter's opaque identities use
    (slice (a) used kind 2 until slice (b) aligned it), which requires a
    single-link regular file that is not a reparse point;
  - the principals, the token user plus the AppContainer SID when contained;
  - the protected private descriptor, whose owner is set explicitly to the token
    user so an elevated token's default owner cannot make an object fail its own
    check;
  - the current-user ACL check: owner equals user, the DACL is protected, and it
    contains only full-access allow entries for the user (and container);
  - the shared or exclusive lock on the one byte at offset
    `0x4000'0000'0000'0000`, far above the 64 KiB journal cap.
- **`src/adapters/agent/src/windows_agent_process.cpp`.** A guarded request runs
  through the supervisor:
  1. The launcher takes its own shared lock.
  2. It starts the supervisor with the agent's standard pipes plus the owner and
     report pipes.
  3. It writes the specification with the command line's terminating NUL
     removed.
  4. It waits for the start report within the deadline and cancellation.

  Teardown closes the owner pipe and waits up to 5 s. If the supervisor has not
  exited by then, it terminates it, which fires that supervisor's own
  kill-on-close. Unguarded launches are unchanged.
- **`src/adapters/agent/src/agent_process_launcher.cpp`.** A requested guard is
  now refused only where no supervisor exists, which is neither Unix nor
  Windows.
- **Build and architecture.**
  - `src/adapters/agent/CMakeLists.txt` adds the Windows supervisor target and
    its tests-build path.
  - `src/app/CMakeLists.txt` copies the supervisor beside the application image
    on Windows too.
  - `cmake/VerifyArchitectureBoundaries.cmake` names
    `windows_agent_supervisor.cpp` as a designated owner of process creation and
    containment APIs.

### Tests

- **`qt_agent_launch_canary`** gains a breakaway course.
  1. The owner first joins a plain outer job, so the supervisor's job is
     nested.
  2. A descendant asks for `CREATE_BREAKAWAY_FROM_JOB`. The request is refused
     with error 5, and the descendant is created inside the job.
  3. Teardown at the 1.5 s timeout stops the descendant's heartbeat and releases
     the guard.
- **The canary's `--workspace-guard` course** has a Windows implementation. It
  checks these cases:
  - **Guarded success**, with a child-side handle census. The agent counts its
    own `Job` handles, `IoCompletion` handles and pipes other than its standard
    handles, and checks its disk handles by final path against the guard.
    Counts must equal those of an unsupervised launch of the same fixture, which
    measured job 0, port 2 and extra pipe 0 on both. The two ports are the
    process's own thread-pool ports. No guard handle may be open.
  - **Exclusivity.** A held exclusive lock refuses a launch before any child
    side effect.
  - **Refusals.** Same-path substitution, a second hard link, a symbolic link
    (created unprivileged here) and a DACL widened to Everyone read are all
    refused.
- **`agent_parent_death`** is registered on Windows as `WINDOWS_UNCONTAINED`,
  pinned `dos-volume-name`. Contained, the canary exits 3 with no output at
  `std::filesystem::canonical` of its root, the lowbox refusal measured under
  BPR-0806. It runs the guard course, then two courses:
  - **`owner-pid`:**
    1. The driver suspends the supervisor (`NtSuspendProcess`, test code only)
       and kills the owner.
    2. The guard must still be held, and the agent must still be running.
    3. On resume, the agent and its descendant must exit, observed through
       process handles opened while they were alive.
    4. The guard must become available, the heartbeat must stop, and a fresh
       launch must succeed.
  - **`owner-live-supervisor`:** the same course without suspending the
    supervisor. Windows has no process groups, so this replaces Unix's
    owner-group course.

  A launch without an owner channel exits 125 with no side effect.

### Results at the slice (a) commit

Commands ran from a `vcvars64` shell with Git's `usr/bin` appended. The
rows below were rerun after a clean rebuild of the committed tree; the
first working-tree run passed identically.

| Check | Result |
|---|---|
| `ctest --preset windows-dev -R "^agent_parent_death$" -V` | Passed, 48.60 s (51.56 s in the first run): `agent-parent-death:owner-pid:pass`, `agent-parent-death:owner-live-supervisor:pass` |
| `ctest --preset windows-dev -R "^qt_agent_launch_canary$" -V` | Passed, 105.22 s (110.33 s in the first run): `agent-breakaway:owner-already-in-job 0`, `owner-in-job 1`, `answer refused 5`, `agent-breakaway:pass`, `agent-launch-canary-tests:pass` |
| The canary's `--workspace-guard` course, run directly | `agent-workspace-guard:pass`, 21.34 s; census `job 0/0`, `port 2/2`, `extra-pipe 0/0`; `symlink-refused` |
| **Trial, reverted:** the supervisor does not take its own lock | `agent_parent_death` **fails**: `application death released the live supervisor's workspace guard` |
| `architecture_boundaries` | Passed, 15.30 s |
| `test_network_isolation_manifest` | Fails with the same 25 sorted findings before and after this slice's registration; zero delta |
| `cmake --build --preset windows-app-release` | Builds. The built supervisor (`src/adapters/agent`) and the bundled copy (`src/app`) both hash `ecd2deed2c2019b68972cb0c4ef02b36bde77078af0b0e0560c53a3a12961077`. `dumpbin /dependents` lists only KERNEL32, ADVAPI32, the MSVC runtime (MSVCP140, VCRUNTIME140, VCRUNTIME140_1) and UCRT `api-ms-win-crt-*`; no Qt. |

### Defects found while building the slice

Both were fixed before the results above.

- **Duplicate handle-list entries.** With stdout and stderr on the same handle,
  the handle list named it twice and `CreateProcessW` failed with error 87. The
  supervisor now de-duplicates the list.
- **A synchronous read blocked exit.** Closing the owner pipe while the watcher
  thread was blocked in a synchronous `ReadFile` on it hung the supervisor's
  exit. The child's output pipe then never reached end-of-file. The supervisor
  now cancels that read and joins the thread first.

### Constraints the integrator set

| Constraint | State at slice (a) |
|---|---|
| The child gets no guard, job, port or owner handle; asserted child-side | Met by the handle list and the census above. |
| No breakaway flags; a breakaway grandchild dies; nested jobs work | Met by the breakaway course, which runs from inside an outer job. |
| A fixed lock range far above the journal cap, and recovery locks the same range | The range is fixed. Recovery arrives with the journal in slice (b). |
| An elevated token's default owner must not refuse its own journal | The descriptor sets the owner explicitly to the token user. No elevated run is possible here, because the shell is not elevated and elevation needs UAC. Recorded as a limitation. |
| The owner-plus-supervisor double kill is recorded as unguarded | Recorded above; no exclusion is claimed. |
| `NtSuspendProcess` in test code only | Only `tests/scripts/agent_parent_death_tests.py` calls it. The product uses documented APIs only; the canary fixture uses `NtQueryObject`, also test code. |
| The supervisor is packaged with `windows-app-release`, with built and bundled SHA-256 recorded | Met: the two hashes above are equal. |

### Portable sources touched in slice (a)

These are the files the Mac and Linux courses should cover at merge:

- `src/adapters/agent/src/agent_process_launcher.cpp`: one preprocessor
  condition.
- `src/adapters/agent/CMakeLists.txt`: Windows-only block.
- `src/app/CMakeLists.txt`: the bundling condition changes from `UNIX` to
  `UNIX OR WIN32`.
- `cmake/VerifyArchitectureBoundaries.cmake`
- `scripts/verify_test_network_isolation.py`: pin only.
- `tests/adapters_qt/CMakeLists.txt`: registration condition, a Windows link
  and the fixture's `ntdll` link.
- `tests/adapters_qt/agent_launch_canary_tests.cpp`: `#if` conditions now
  include `Q_OS_WIN`, plus new Windows-only blocks.
- `tests/fixtures/agent/agent_canary_main.cpp`: three standard includes plus
  Windows-only blocks.
- `tests/scripts/agent_parent_death_tests.py`: the POSIX path is unchanged
  except the guard-course run timeout, which goes from 25 s to 60 s.

## Slice (b): the ownership journal on Windows

### Source

- **`src/adapters/git/src/installed_git_process.cpp`.** The Unix-only journal
  block now builds under `Q_OS_UNIX || Q_OS_WIN`. Its POSIX calls sit behind a
  thin platform layer, and the owner, load, prepare, recover and cleanup logic
  above that layer is shared. Windows primitives:
  - **Handles.** The journal opens with `GENERIC_READ|GENERIC_WRITE|DELETE`,
    share read/write/delete and `FILE_FLAG_OPEN_REPARSE_POINT`. Identity is
    `{1, volume serial, index high, index low}` from the handle, and the
    handle must be a regular file with exactly one link.
  - **Privacy.** Every journal and directory check reads the descriptor
    through the handle (`windows_private_to_current_user`). The owner must be
    the token user, the DACL protected, and the only grants full access to that
    user or to the AppContainer SID. New directories and journals are created
    with that descriptor and the owner set explicitly.
  - **Lock.** `LockFileEx` on one byte at `0x4000'0000'0000'0000`, shared for a
    live owner and exclusive for recovery, fail-immediately. Every journal read,
    append and truncate works inside the 64 KiB cap, far below that offset.
  - **Create and delete.** Creation is `CREATE_NEW`, so it never replaces.
    Deletion is `FileDispositionInfoEx` with
    `DELETE|POSIX_SEMANTICS` on the verified handle. A directory sync is
    `FlushFileBuffers` on a directory handle.
  - **The parent.** A neighbourhood is created only under a parent the user
    owns and no other principal can write. Only the user, the AppContainer SID,
    SYSTEM and Administrators may hold write-type grants.
  - **Unchanged.** The limits stay at 64 records, 64 KiB and a 4096-entry
    inventory. The flow of mkdir, identity receipt, quarantine rename and
    recursive removal is the same.
- **`src/adapters/agent/include/bpr/adapters/agent/windows_workspace_guard.hpp`.**
  Moved from `src/` so the Git adapter can include it. The identity kind changes
  from 2 to 1 to match the Git adapter's opaque directory identities. The guard
  opens with `FILE_SHARE_DELETE` as well, so a live owner does not block the
  journal's own delete.
- **CMake.** On Windows, `better_pull_review_adapter_git` links
  `BetterPullReview::AdapterAgent` (for the header include path) and
  `Advapi32`. The three Git contract targets that compile
  `installed_git_process.cpp` directly get the same include and link.

### Tests

- **`agent_workspace_ownership` now runs on Windows** with the same course. It
  covers:
  - forced death (`TerminateProcess`) at every committed phase, including both
    renames;
  - the ambiguous mkdir, where recovery and abandon both refuse;
  - the live owner: `recovered=0 live=1`, then recovery after its death;
  - busy cleanup, cancellation and the partial tail;
  - substitution, symlink and corruption negatives that preserve the exact
    fixture;
  - replaced quarantines and the inventory bound;
  - capacity, with 64 live journals held by the test's own `LockFileEx`
    shared locks: `live=64`, the 65th admission refused, then `live=63` and
    `recovered=63`;
  - removal that does not follow a symlink into an external checkout.
- **Two Windows-only negatives:**
  - `negative-acl`: an `Everyone:(R)` grant on the journal makes recover and
    abandon both return `failure=1`, and the fixture is byte-identical before
    and after.
  - `untrusted-parent`: `Everyone` modify rights on the root make abandon return
    `failure=1`, and no neighbourhood is created.
- **Fixture changes.** Fixture directories are made private with `icacls`,
  because Python's `mkdir(mode)` leaves SYSTEM, Administrators and OWNER RIGHTS
  grants. The marker is written in binary mode, and the busy-cleanup course
  takes an independent shared guard through the product header.
- **Registration.** `agent_workspace_ownership` is pinned `WINDOWS_UNCONTAINED`
  with cause `dos-volume-name`. Contained, CPython `Path.resolve(strict=True)` of
  the fixture executable fails with `[WinError 5] Access is denied` before any
  course step.

### Results

These ran on the committed source tree after a rebuild, from a `vcvars64` shell
with Git's `usr/bin` appended (`ctest --preset windows-dev -V`).

| Check | Result |
|---|---|
| `agent_workspace_ownership` | Passed, 9.07 s (8.88 s in the first working-tree run): `AGENT_WORKSPACE_OWNERSHIP_OK` |
| `git_process_contract` | Passed, 38.13 s (38.36 s) |
| `agent_parent_death` | Passed, 48.76 s (48.60 s) |
| `qt_agent_launch_canary` | Passed, 105.00 s (105.31 s) |
| `architecture_boundaries` | Passed, 15.20 s (15.27 s) |
| `test_network_isolation_manifest` | The same 25 findings as slice (a), compared line by line; zero delta, and none name this slice's tests |
| **Trial, reverted:** recovery ignores a busy lock | `agent_workspace_ownership` **fails**: `recover: expected 'recovered=0 live=1', got 'recovered=1 live=0'` |
| **Trial, reverted:** the handle privacy check always passes | **Fails** at `negative-acl`: `expected 'failure=1', got 'recovered=1 live=0'` |
| **Trial, reverted:** only the path-level privacy check in the header is disabled | Still passes, because the journal's handle check catches the widened DACL on its own. The handle check is the one that carries the negative. |
| `cmake --build --preset windows-app-release` | Builds. The built and bundled supervisors both hash `b882ec449529b3535161f8bce9bac5558c8a44be60cfa6211831deb8d146d33a`. |

### Defects found while building the slice

- **Identity kind.** The guard encoded kind 2 while the Git adapter's opaque
  identities use kind 1, so every prepare refused its own journal. The kinds are
  now aligned.
- **CRLF marker.** A text-mode `ofstream` wrote the ownership marker with CRLF,
  so its bytes did not match. The marker is now written in binary mode.
- **Fixture ACLs.** Directories from Python's `mkdir` inherit SYSTEM,
  Administrators and OWNER RIGHTS grants and correctly failed the privacy rule.
  The fixture now strips them.

### Constraint (3) now met

Recovery takes the exclusive lock on exactly the fixed range that live owners
and the supervisor hold shared. No journal read or append reaches that offset.
The busy-lock trial above shows the lock carries the live-owner negative.

### Limits recorded

- **The elevated shell.** Not run, as in slice (a). The descriptor sets the
  owner explicitly.
- **The POSIX branches.** Not built on this host. Review found them
  behaviour-preserving. One check is new: journal creation also checks privacy
  on the new file, which always holds for a new 0600 file.

### Portable sources touched in slice (b)

The Mac and Linux courses should cover these at merge:

- `src/adapters/git/src/installed_git_process.cpp`: POSIX calls move behind
  the platform layer; the preprocessor conditions widen to `Q_OS_WIN`.
- `src/adapters/git/CMakeLists.txt`: Windows-only block.
- `tests/git_contract/CMakeLists.txt`: the registration condition becomes
  `UNIX OR WIN32`, plus `WINDOWS_UNCONTAINED` and Windows-only blocks.
- `tests/git_contract/agent_workspace_ownership_tests.cpp`: POSIX includes go
  under `#if`; the marker is written in binary mode (no effect on POSIX).
- `tests/scripts/agent_workspace_ownership_tests.py`: `fcntl` is imported only
  on POSIX. `create_root` now calls `make_private`, which on POSIX is an
  explicit `chmod 0700` after the existing `mkdir(mode=0o700)`. The capacity
  course holds its locks through `hold_shared`, which uses the same `flock` on
  POSIX.
- `scripts/verify_test_network_isolation.py`: pin only.
