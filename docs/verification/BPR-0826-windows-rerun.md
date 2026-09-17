# BPR-0826 Windows rerun

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-startup-roots`, step 1. [[BPR-0826]] is integrator-owned;
this file is evidence only.

**Base.** `559377700d52f37d6c094cd646200c67ab20eeb5` contains the course fix
`d9ea1cbc`. The windows-dev tree was fully rebuilt (Debug); the one other
change in that build is [[BPR-0831]]'s benchmark fix, which this course does
not compile.

**Host.** Windows 11 Home 10.0.26200; Visual Studio 18 Community, MSVC
14.51.36231; Qt 6.10.3 `msvc2022_64`. The shell token is not elevated.

## Result

`ctest --preset windows-dev -R "^application_agent_invocation_composition_contract$" --output-on-failure -V`

| Check | Result |
|---|---|
| `application_agent_invocation_composition_contract` | **Passed**, 26.85 s |
| Seed phase | `agent-invocation-composition-seed:pass` |
| Reopen phase | `agent-invocation-composition-reopen:pass` |
| Contract | `AGENT_INVOCATION_COMPOSITION_CONTRACT_OK` |

No `agent-exact-context-session` line was printed; the course prints it only
on failure.

Before `d9ea1cbc`, this course failed on Windows at `c94e02b1`: the expected
context's `ApplicationSession` was 2 where the course required 1
(`docs/verification/BPR-0622-windows-native-evidence.md`, slices (c) and
(d)).

Log: `build/logs/task-20260917-win-startup-roots/ctest-bpr0826.log`.

**Portable sources touched:** none. This step adds evidence only.
