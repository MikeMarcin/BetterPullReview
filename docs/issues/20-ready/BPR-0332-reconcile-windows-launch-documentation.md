---
id: BPR-0332
title: Reconcile the Windows launch documentation with the shipped dispatch
status: Ready
rfc: RFC-BPR-0011
depends_on: []
---

# Reconcile the Windows launch documentation with the shipped dispatch

Six statements across the RFC-BPR-0011 verification ledger and the agent
architecture document assert, in the present tense, that the adapter refuses a
Windows agent launch before `QProcess::start`. The adapter stopped doing that at
`c6f3d96`, which replaced the refusal with a dispatch into the private WIN32
backend:

```cpp
#if defined(Q_OS_WIN)
		return detail::run_windows_agent_process(
			aRequest, aTimeoutMilliseconds, aSink, cancelled);
```

The deleted branch had set `failed_to_start` with "agent launch is disabled on
Windows until pre-start process-tree containment has target-native proof". No
such proof exists yet — `windows_agent_process.cpp` has never been compiled by
any toolchain this project has access to.

The stale statements:

| Location | Claim |
|---|---|
| `docs/verification/RFC-BPR-0011.md:13` | "Windows launch remains disabled before process start" |
| `docs/verification/RFC-BPR-0011.md:109` | "Windows starts no child." — a claim-matrix **Result** cell |
| `docs/verification/RFC-BPR-0011.md:115` | "Windows still proves pre-start refusal only." |
| `docs/verification/RFC-BPR-0011.md:216` | "The adapter refuses before `QProcess::start`" |
| `docs/architecture/ai-agent-boundary.md:140` | "refuses Windows launch before `QProcess::start`" |
| `docs/architecture/ai-agent-boundary.md:335` | "Windows still refuses before child start" |

Line 109 is the one that matters most. A claim-matrix Result cell is what a
reader consults to decide whether a path is safe, and it currently certifies a
property the source does not have, for code nothing has ever compiled.

A seventh entry is wrong in a different way. `ai-agent-boundary.md:314` cites
`qt_agent_launch_canary` (Windows-conditional) as the evidence for "Windows
launch is refused before start". The test it points at is now
`test_windows_assignment_failure_has_no_pre_assignment_side_effect`, whose final
assertion is `recovered.Outcome == success`. The evidence row names a test that
would contradict its own stated claim if it ever ran.

## The decision this card carries

Two resolutions are defensible, and they are not equivalent:

1. **Restore the guard.** Put the pre-dispatch refusal back so the documented
   property becomes true again, and let the WIN32 backend stay unreachable until
   target-native proof exists. Conservative, and consistent with how this
   project treats every other unproven path — fail-closed CLI, fail-closed
   redirects, default-deny live mutation.
2. **Document the real state.** Leave the dispatch and rewrite the six
   statements to say what is actually true: the Windows path dispatches to a
   backend that has never been compiled or executed, [[BPR-0240]] owns proving
   it, and no Windows behavioural claim holds until then.

Option 1 changes shipped behaviour. Option 2 does not, and does not pre-empt
option 1 later. Neither may be chosen by rewriting a Result cell to sound
satisfied.

## Required final state

- No document asserts a Windows pre-start refusal that the source does not
  perform, in either resolution.
- `ai-agent-boundary.md:314`'s evidence row names a test whose assertions
  actually support the claim in the row, or the row is removed.
- If the guard is restored, a test fails without it.
- The documentation gate and the two documentation contract tests pass.

## Non-goals

- Do not claim any Windows runtime property. Nothing here has been compiled or
  run on Windows, and this card does not change that.
- Do not close or reclassify [[BPR-0240]]. This card exists because that card's
  independent review found portable work still owed on this host.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
ctest --test-dir build/dev --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

If the guard is restored, additionally the agent launch and canary selectors
that cover the dispatch path.
