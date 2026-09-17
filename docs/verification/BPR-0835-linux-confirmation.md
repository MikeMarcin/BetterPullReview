# BPR-0835 Linux confirmation

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-003` (station
  dispatch `bpr-linux/int-6`), mandate r2, task
  `task-20260917-lnx-portable-p3`. Station `bpr-linux` (`stn-3f7fcc92`,
  `wsl2`), branch `linux-lane`, authority `lane-commit`. Base
  `94f6f8859993be4fea63c52855bab4f0144b185e`. RFC-BPR-0029 base
  `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.
- **Scope of this file.** Step 1 only: [[BPR-0835]] belongs to the
  integrator. This file is evidence. The card is not edited or moved.
- **State.** Step 1 is complete. All 14 runs pass. No run printed a failure
  line.
- **Commands.** None is active. No child agent ran for this step.
- **Next.** Steps 2 and 3 of the task, [[BPR-0842]] and [[BPR-0838]], are
  recorded on their cards.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-portable-p3/`.
  None is committed.

## Station, revision and build

Station `bpr-linux` ran these on JaegAW (WSL2) on 2026-09-17, on branch
`linux-lane` at `94f6f8859993be4fea63c52855bab4f0144b185e`, a clean tree.
That revision contains `3cc2ac482dc75591acaf07109c11727c0af4e494`, the
course change for this card (BPR-0705's per-window snapshot path, the setup
destination's submit, and a named line for every failed term). The host,
compiler, Qt and tool versions are the ones in the
[dsp-003 Linux confirmation](dsp3-linux-confirmation.md#station-revision-and-build).
No `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `CMAKE_*`, `QT_*` or
`CTEST_*` variable was set.

- **Configure.** `cmake --preset dev --fresh`: rc 0, 7.3 s, ending
  12:56:51Z. `CMAKE_CXX_FLAGS` is empty.
- **Build.** `cmake --build --preset dev`: rc 0, 263 steps, 15.1 s, ending
  12:57:06Z. The log has 0 `warning:` and 0 `error:` lines. It compiled
  `agent_finding_composition_public_path.cpp`, `catalog_database.cpp`,
  `device_binding_store.cpp` and `linux_network_sandbox.cpp`, and relinked.
  `ninja -n` then reported no work.
- **The binary is the changed course.** The built course contains the string
  `agent-finding-composition-failure`, which only `3cc2ac48` adds.

## How the rows ran

`ctest --preset dev -N -R 'application_agent_finding_'` lists 10 rows. The
two contracts ran 3 times each, alternating, and the other 8 rows ran once.
Every run was its own serial `ctest --preset dev -j1 -V -R '^<name>$'`
invocation with its own log (`s1-<label>.ctest.log`). The Secret Service
`login` collection read `(<false>,)` immediately before every run. Exit codes,
wall times and load averages are in `groups.summary`, and ctest's result lines
are in `groups.results`. The 1-minute load average was 0.85 to 1.07.

## Results

| Run | Row | Result | ctest time | Printed |
|---|---|---|---|---|
| 1 | `application_agent_finding_composition_contract` | Pass | 1.29 s | `agent-finding-composition:pass` |
| 1 | `application_agent_finding_full_agent_contract` | Pass | 1.30 s | `agent-finding-full-agent:pass` |
| 2 | `application_agent_finding_composition_contract` | Pass | 1.25 s | `agent-finding-composition:pass` |
| 2 | `application_agent_finding_full_agent_contract` | Pass | 1.30 s | `agent-finding-full-agent:pass` |
| 3 | `application_agent_finding_composition_contract` | Pass | 1.28 s | `agent-finding-composition:pass` |
| 3 | `application_agent_finding_full_agent_contract` | Pass | 1.32 s | `agent-finding-full-agent:pass` |
| 1 | `application_agent_finding_protocol` | Pass | 1.44 s | `agent-finding-protocol-tests:pass` |
| 1 | `application_agent_finding_finding_host_construction_cut_contract` | Pass | 1.26 s | `agent-finding-finding-host-construction-cut:pass` |
| 1 | `application_agent_finding_workshop_press_contract` | Pass | 1.58 s | `agent-finding-workshop-press:pass` |
| 1 | `application_agent_finding_workshop_construction_cut_contract` | Pass | 1.25 s | `agent-finding-workshop-construction-cut:pass` |
| 1 | `application_agent_finding_mcp_construction_cut_contract` | Pass | 1.25 s | `agent-finding-mcp-construction-cut:pass` |
| 1 | `application_agent_finding_chat_panel_construction_cut_contract` | Pass | 1.26 s | `agent-finding-chat-panel-construction-cut:pass` |
| 1 | `application_agent_finding_queue_construction_cut_contract` | Pass | 1.26 s | `agent-finding-queue-construction-cut:pass` |
| 1 | `application_agent_finding_dif_context_contract` | Pass | 7.57 s | `agent-finding-dif-context:pass` |

- **No failure line.** No log contains `agent-finding-composition-failure`,
  `failed=`, `agent-finding-positive-evidence` or an `agent-finding-full-agent`
  line other than the pass term.
- **Why the evidence lines are absent.** At this revision the course prints
  `agent-finding-positive-evidence:…` and `panel-status:…` only when
  `positive-evidence` has a false term, and `agent-finding-full-agent:<exact>:…`
  only on the full-agent failure path
  (`tests/end_to_end/agent_finding_composition_public_path.cpp` ~985 and
  ~1088). A passing run prints only its pass term. The other output is Qt's
  `This plugin does not support propagateSizeHints()`, four times per
  composition run.
- **Before the change.** At `84ac5e0c` this station printed
  `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0` and
  `agent-finding-full-agent:0:0:802982:0:0:0`, and both rows failed twice
  ([dsp-003 step 6](dsp3-linux-confirmation.md#step-6-bpr-0835-the-agent-finding-composition-contracts)).
  With `3cc2ac48` both pass 3 of 3 on Linux.
- **Load.** These runs were serial on a quiet host. They say nothing about
  the load-dependent waits recorded on [[BPR-0840]].
- **Offscreen by registration.** The 9 composition-course rows run
  `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.
  `application_agent_finding_protocol` sets no Qt platform.

## What remains on the card

Linux is not one of the card's platforms. The card still needs its Windows
rerun with the fix and one quiet macOS rerun of both contracts.
