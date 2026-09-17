# BPR-0835 Windows observation

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-private-roots`, step 4. [[BPR-0835]] is owned by the Mac
station; this file is evidence only and does not claim the card.

**Base.** `6c44b6eaedfc7a752f0ab78255c2403bc4f406e4` plus this task's commits
up to `bf930d12c74c959e49b81ffdf2a1297f01abcb17`. None of those commits
touches the finding composition course or its application paths. The
windows-dev tree was fully rebuilt (Debug).

**Host.** Windows 11 Home 10.0.26200; Visual Studio 18 Community, MSVC
14.51.36231; Qt 6.10.3 `msvc2022_64`. The shell token is not elevated.

**Command.** Each row ran once:
`ctest --preset windows-dev -R "^(application_agent_finding_composition_contract|application_agent_finding_full_agent_contract)$" --output-on-failure -V`.
Log: `build/logs/task-20260917-win-private-roots/ctest-bpr0835.log`.

## Results

| Row | Result | Evidence lines |
|---|---|---|
| `application_agent_finding_composition_contract` | **Failed**: `agent-finding-composition:pass` not printed | `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0` |
| `application_agent_finding_full_agent_contract` | **Failed**: `agent-finding-full-agent:pass` not printed | `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0`, then `agent-finding-full-agent:0:0:802982:0:0:0` |

## Reading the lines

The field order comes from
`tests/end_to_end/agent_finding_composition_public_path.cpp`.

`agent-finding-positive-evidence` fields:

| Position | Field | Value |
|---|---|---|
| 1 | Deliveries | 1 |
| 2 | Queued findings | 1 |
| 3 | Frozen file count | 1 |
| 4 | Frozen snapshot writes | 1 |
| 5 | Workshop proposals | 1 |
| 6 | Draft commands | 1 |
| 7 | Helper refusal | 0 |
| 8 | MCP completions | 1 |
| 9 | Mutation submissions | 0 |
| 10 | Mutation target observations | 0 |

`agent-finding-full-agent` fields:

| Position | Field | Value |
|---|---|---|
| 1 | exact | 0 |
| 2 | no second edge | 0 |
| 3 | source size | 802982 |
| 4 | mutation submissions | 0 |
| 5 | mutation target observations | 0 |
| 6 | stored comments | 0 |

Every printed count matches a positive course: one delivery, one queued
finding, one frozen file, one workshop proposal, one draft command, one MCP
completion, no helper refusal and no mutation. The failing condition is
therefore one the line does not print. In the full-agent course, `exact` and
`no second edge` are both 0.

This observation adds no diagnosis beyond these lines; the card's owner
diagnoses the failure.

## Rerun after the course fix `3cc2ac48` (task-20260917-win-creation-descriptor)

The base is `31fff50675a71bc2c5ce55857c72e57dc36933df`, which contains
`3cc2ac48`. The windows-dev tree was fully rebuilt; the only other change on
top is [[BPR-0833]]'s Windows-only change to storage-root creation (`aa3c6ef1`).
Each row ran twice. Logs:
`build/logs/task-20260917-win-creation-descriptor/ctest-bpr0835-1.log` and
`-2.log`.

| Row | Run | Result | Lines |
|---|---|---|---|
| `application_agent_finding_composition_contract` | 1 | **Failed** | `agent-finding-composition-failure:positive-evidence failed=helper-directory-is-snapshot-root`, `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0` |
| `application_agent_finding_composition_contract` | 2 | **Failed** | identical |
| `application_agent_finding_full_agent_contract` | 1 | **Failed** | `agent-finding-composition-failure:positive-evidence failed=helper-directory-is-snapshot-root`, `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0`, `agent-finding-full-agent:0:1:802982:0:0:0` |
| `application_agent_finding_full_agent_contract` | 2 | **Failed** | identical |

The two runs of each row are identical.

With the course fix the failure is now named: `helper-directory-is-snapshot-root`.
In `tests/end_to_end/agent_finding_composition_public_path.cpp` that term is
the exact string comparison `launch.WorkingDirectory == aSnapshotRoot.string()`.

In the full-agent line, `no second edge` changed from 0 to 1; `exact` is still
0.

No diagnosis beyond these lines is attempted here; the card's owner
diagnoses the failure.
