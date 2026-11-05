---
schema: "implement-rfc.card.v1"
id: "BPR-1869"
title: "The agent crash course reaches its checkpoint on Windows"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["windows", "mac", "linux"]
metadata: {"kind":"bug","found_by":"BPR-1857's bpr-main-win run, 2026-11-04: application_agent_crash_recovery timed out at crash-running on its branch and identically on base 33e1e088a","reservation":"docs/issues/_reservations/BPR-1869--BPR-1869--10a14d4437a2.json","related_cards":["BPR-1857","BPR-1753","BPR-0622","BPR-0240","BPR-1259"],"frontend_design":"loaded for the Conversations answer pacing; nothing visual changes, only how often a streaming answer re-renders"}
acceptance: [{"criterion":"AC1 application_agent_crash_recovery passes on Windows","outcome":"pass","revision":"fefa2b983","evidence":{"kind":"command","command":"windows-dev (JaegAW, MSVC, Debug, over OpenSSH): ctest --preset windows-dev -C Debug -R \"^application_agent_crash_recovery$\" --repeat until-fail:3","detail":"Base 4dc32922a fails as BPR-1857 recorded: crash-running exited before checkpoint (1) after 56 s, agent-and-descendant-running=false. At fefa2b983 it passes four times in a row, 30.2 to 30.6 s each (78 s at d67fbd686, where a repeat then failed at crash-reinvoke on the same cause)."}},{"criterion":"AC2 The Windows rows with the same cause pass","outcome":"pass","revision":"fefa2b983","evidence":{"kind":"command","command":"ctest --preset windows-dev -C Debug --output-on-failure -R \"agent_invocation|agent_launch_canary|agent_workspace_ownership|agent_materialization|agent_crash\"","detail":"cancellation, staleness and cleanup-retry contracts, which failed at base waiting for canary-complete (output stuck near 11 to 18 KB for 45 s), now pass in 8.4 to 8.7 s; host, durable canary course, workspace ownership, materialization crash recovery, construction, launcher, gesture and workspace-diagnostic rows pass. application_agent_invocation_composition_contract passes (22.7 s) with USERDOMAIN=JAEGAW set: over OpenSSH its CMake narrowing helper fails first (Open). qt_agent_launch_canary exits 0xc0000142 at start (0.04 s) under OpenSSH, before any course code; not judged here."}},{"criterion":"AC3 macOS still passes, with every agent row","outcome":"pass","revision":"fefa2b983","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev --output-on-failure -j4 -R \"agent|claude_process_wrapper|qt_workbench_window\"","detail":"macOS arm64 dev: 63 of 63 pass (four rows whose executables this slice does not touch and were not built are excluded by name: core_agent_comment_suggestion, core_agent_answer_findings, application_claude_process_wrapper, sqlite_agent_setup_storage). Before the fix the crash row failed here too, intermittently: output 16064 bytes, descendant-id seen, process-id not."}}]
---

# BPR-1869: The agent crash course reaches its checkpoint on Windows

## Report

[[BPR-1857]]'s station run: `application_agent_crash_recovery` fails on
Windows at `crash-running`. The course child printed
`agent-invocation-composition-failed:crash-running failed=agent-and-descendant-running`
and exited 1 after about a minute, before the kill checkpoint; the agent's
panel never showed both `process-id` and `descendant-id`. Base 33e1e088a
failed identically. Reproduced at 4dc32922a on windows-dev.

## Root cause

Not process, kill, handle or path semantics: the agent, its supervisor and its
descendant were all running. The course had not been shown their identities
yet, because the application drew the agent's output too slowly.

1. **The product (Qt).** Every output line calls `agent_tab_view::progress`,
   which re-rendered the running turn's *whole* answer as Markdown with line
   links ([[BPR-1753]]'s live answer). A turn therefore cost lines times
   answer size. The canary echoes the review context it receives on standard
   input, about 300 lines, so the drawer re-rendered a growing 15 KB answer
   300 times. Measured with a probe of the panel's output over time: on
   macOS the output stood at about 2 KB for 6 to 31 s, then arrived at once;
   on Windows (MSVC Debug) it stood at 3.6 KB for 46 s. Any agent writing many
   lines pays the same.
2. **The fixture.** The canary wrote `supervisor-id` and `process-id` last,
   after that echo, so the crash checkpoint waited for all of it, and it
   wrote every frame in three-byte pieces a millisecond apart: a 45 KB echo
   was 15,000 sleeps, 25 s on macOS on its own.

Together they put the checkpoint past the course's 30 s wait on Windows every
time and on macOS sometimes. The same cause failed the running, staleness,
cleanup-retry and crash-reinvoke courses on Windows (waiting for
`canary-complete` or the finished status), which BPR-1857's selection did not
run.

A third fault is environment-only: over OpenSSH on a workgroup machine
`USERDOMAIN` is `WORKGROUP`, so the course's `make_private` grant to
`USERDOMAIN\USERNAME` failed in `icacls` with 1332 before anything ran. The
station's interactive session has `JAEGAW` there.

## Built

- **Qt** (`agent_tab_view`): `AnswerPacer`, a 100 ms single-shot timer. The
  first output line renders the answer at once; lines while it runs mark it
  owed, and it renders once more when it fires. A phase change renders through
  `refresh` as before and owes nothing, so the finished answer is exact. The
  banner and refusal checks stay per line.
- **Canary** (`tests/fixtures/agent/agent_canary_main.cpp`):
  `report_process_identity`; in `crash-descendant-wait` mode it runs right
  after `descendant-id`, before the echo (the `--report-process-id` path keeps
  its place). `write_partial` still splits every frame, in three-byte pieces up
  to 192 bytes and in 64 pieces beyond.
- **Course** (`tests/end_to_end/agent_invocation_composition_public_path.cpp`):
  the crash wait requires the identities and `origin`, which follows HEAD, so
  each line it reads is whole; it no longer waits for the echo. A miss records
  the output size, which ids were seen and the panel status.
- **Fixture helper** (`tests/scripts/agent_workspace_ownership_tests.py`):
  `current_user_sid` (System32's `whoami`, by path, since Git for Windows'
  POSIX `whoami` is on PATH for perl); `make_private` grants `*SID`.

The functional checks and their waits are unchanged.

## Verification

Windows: `C:\code\git\bpr-1869`, a plain clone (no alternates) on branch
`bpr-1869-windows-agent-crash-recovery`, configured with
`-DCPM_SOURCE_CACHE=C:/code/git/BetterPullReview/build/.cpm-cache` (the
station's locked shared cache, so Qt was not acquired again), `vcvars64.bat`
loaded and Git for Windows' `usr\bin` on PATH, per
`docs/development/remote-stations.md`; the session was OpenSSH, not the
station's interactive one.

```powershell
cmake --build --preset windows-dev --target test_better_pull_review_agent_invocation_composition_public_path test_better_pull_review_agent_canary tool_better_pull_review_network_sandbox tool_better_pull_review_agent_supervisor
ctest --preset windows-dev -C Debug --output-on-failure -R "agent_invocation|agent_launch_canary|agent_workspace_ownership|agent_materialization|agent_crash"
ctest --preset windows-dev -C Debug -R "^application_agent_crash_recovery$" --repeat until-fail:3
```

macOS arm64, dev preset, the 63 rows in AC3. clang-format 22 is clean on every
touched C++ file. No live GitHub call was made.

## Open

- Over OpenSSH, `tests/end_to_end/bpr_narrow_owner_private.cmake` and
  `tests/end_to_end/bpr_narrow_owner_private.py` still grant
  `USERDOMAIN\USERNAME` and fail with 1332; the station's interactive session
  is unaffected. The same SID grant would fix them.
- `qt_agent_launch_canary` exits 0xc0000142 at start under OpenSSH; whether it
  passes in the station's interactive session was not checked here.
- [[BPR-1857]]'s "Follow-up: Windows crash-running course" section can point
  here once this lands.
