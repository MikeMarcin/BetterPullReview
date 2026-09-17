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
- **`src/adapters/agent/src/windows_workspace_guard.hpp`** (new, header-only).
  It provides:
  - the kind-2 identity (volume serial, file index), which requires a
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
