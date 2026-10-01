---
schema: "implement-rfc.card.v1"
id: "BPR-1259"
title: "Windows: agent courses never report finished when the canary loses its timer resolution"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win BPR-0905 Windows full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1259--BPR-1259--b28391b17369.json","related_cards":["BPR-0905","BPR-0240","BPR-1138","BPR-1229","BPR-1245"]}
acceptance: [{"criterion":"The eight rows pass on Windows with the fix, run serially","outcome":"pass","revision":"bb88d12eb","evidence":{"kind":"command","detail":"ctest --preset windows-dev -j1 --output-on-failure -R \"^(agent_|application_agent_|qt_agent_)\" on bpr-main-win (Windows 11 10.0.26200, MSVC 14.51, Qt 6.11.2, build/windows-dev Debug) with this commit's canary change: agent_parent_death, application_agent_crash_recovery, application_agent_finding_composition_contract, application_agent_finding_dif_context_contract and the four application_agent_invocation_{composition,cancellation,staleness,cleanup_retry}_contract rows all pass; without it, agent_parent_death and the invocation composition contract fail alone at the same HEAD."}},{"criterion":"No other agent row changes result","outcome":"pass","revision":"bb88d12eb","evidence":{"kind":"command","detail":"Same run: 33 of 35 pass in 488.43 s. The two failures are qt_agent_launch_canary (open durable child course, BPR-1229) and application_agent_finding_workshop_press_contract (run-quiescent, BPR-1245), as in the BPR-0905 Windows full suite."}}]
---

# BPR-1259: Windows: agent courses never report finished when the canary loses its timer resolution

Found in the [BPR-0905 Windows full suite](../../verification/BPR-0905-full-suite-2026-10-01.md)
on bpr-main-win at `db021262a`. Eight rows failed there. In each, the agent
never reports finished:

- `application_agent_invocation_composition_contract`,
  `_cancellation_contract`, `_staleness_contract` and
  `_cleanup_retry_contract`: `failed=canary-complete-seen`, or
  `typed-success agent-status=Running … reached=status-is-finished`;
- `application_agent_crash_recovery`: `crash-running …
  agent-and-descendant-running=false`;
- `agent_parent_death`: `guarded launch outcome 7` (`timeout`), `guarded
  launch succeeds through the Windows supervisor`;
- `application_agent_finding_composition_contract`:
  `positive-evidence failed=draft-body-is-suggestion`;
- `application_agent_finding_dif_context_contract`:
  `dif-context-evidence failed=dif-saw-consider`.

`agent_parent_death` and the invocation composition contract failed the same
way when run alone. These rows passed on this station earlier the same day, under
[[BPR-1138]] Part A at `aba89bae1`.

## Not a commit

The rows fail at `aba89bae1` too, rebuilt and run on the same afternoon. They
also fail with the canary's pre-BPR-1138 marker rebuilt in. So no commit in
`aba89bae1..db021262a` caused this, and the bisect the coordinator suggested
has nothing to find.

## What the canary was doing

While `agent_parent_death` ran, a stack dump of the live
`better_pull_review_agent_canary.exe`, the child of
`better_pull_review_agent_supervisor.exe`, showed its main thread in
`NtDelayExecution`. The frames were `std::this_thread::sleep_for` and
`write_partial` (`agent_canary_main.cpp:159`), called from `emit`, in all
three samples taken a second apart. Its total CPU time was 0.05 s.

`write_partial` writes three bytes and then sleeps 1 ms. At the default
15.6 ms tick, a 10 KB exchange takes about 53 s. That is the 54 s each failing
invocation contract took. The canary calls `timeBeginPeriod(1)` for this
([[BPR-0240]]). But Windows 11 may ignore that request from a process it
treats as unseen, and a canary under the supervisor has no window. On this
host, a Python process started by the station measured `Sleep(1)` as follows:

| Case | `Sleep(1)` |
|---|---:|
| `timeBeginPeriod(1)` at start | 1.9 ms |
| `timeBeginPeriod(1)` after 3 s of default-tick sleeps | 15.5 ms |
| No request | 15.5 ms |

The canary run directly from the station, with no supervisor, paced
`--findings-transcript` at 2.1 ms per three bytes. What changed on the host
between the morning pass and the afternoon failures was not identified. The station's console
window being hidden or occluded fits Windows' rule, but was not proven.

## Fix

Right after `timeBeginPeriod(1)`, the canary asks Windows not to ignore its
timer resolution:

```cpp
auto throttling        = PROCESS_POWER_THROTTLING_STATE{ };
throttling.Version     = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
throttling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
throttling.StateMask   = 0U;
static_cast<void>(::SetProcessInformation(::GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling)));
```

This is fixture code only. The product's agent launcher, supervisor and
pacing are unchanged. The call is best-effort, like `timeBeginPeriod`.

## Acceptance

- The eight rows pass on Windows with the fix, run serially.
- No other agent row changes result.

## Windows evidence (bpr-main-win, 2026-10-01)

Windows 11 Home 10.0.26200, Visual Studio 18 (MSVC 14.51), Qt 6.11.2,
`build/windows-dev` Debug, at `70fd63a49` plus this change.
`test_better_pull_review_agent_canary` built with no warning, and
`coding-format-check` passed.

```powershell
ctest --preset windows-dev -j1 --output-on-failure -R "^(agent_|application_agent_|qt_agent_)"
```

33 of 35 pass, in 488.43 s. All eight of this card's rows pass. "Before" is the
full suite, except where it says alone:

| Row | Before | After (serial) |
|---|---|---|
| `agent_parent_death` | Fail, 20.16 s alone | Pass, 50.29 s |
| `application_agent_crash_recovery` | Fail | Pass, 58.91 s |
| `application_agent_finding_composition_contract` | Fail | Pass, 8.66 s |
| `application_agent_finding_dif_context_contract` | Fail | Pass, 15.27 s |
| `application_agent_invocation_composition_contract` | Fail, 53.86 s alone | Pass, 29.23 s |
| `application_agent_invocation_cancellation_contract` | Fail | Pass, 21.30 s |
| `application_agent_invocation_staleness_contract` | Fail | Pass, 22.38 s |
| `application_agent_invocation_cleanup_retry_contract` | Fail | Pass, 21.24 s |

The two failures have owners and fail as they did in the full suite:

- `qt_agent_launch_canary`, `open durable child course`, 0.05 s
  ([[BPR-1229]]);
- `application_agent_finding_workshop_press_contract`, `run-quiescent`
  ([[BPR-1245]]).

`clang-tidy` was not run: the Visual Studio `windows-dev` graph has no
compilation database.
