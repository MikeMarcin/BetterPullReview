---
id: BPR-0332
title: Reconcile the Windows launch documentation with the shipped dispatch
status: Done
rfc: RFC-BPR-0011
depends_on: [BPR-0240]
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
| `docs/verification/RFC-BPR-0011.md:118` | "Windows starts no child." — a claim-matrix **Result** cell |
| `docs/verification/RFC-BPR-0011.md:124` | "Windows still proves pre-start refusal only." |
| `docs/verification/RFC-BPR-0011.md:225` | "The adapter refuses before `QProcess::start`" |
| `docs/architecture/ai-agent-boundary.md:140` | "refuses Windows launch before `QProcess::start`" |
| `docs/architecture/ai-agent-boundary.md:342` | "Windows still refuses before child start" |

Four of those line numbers moved when [[BPR-0401]] marked each site in place;
the claims themselves are untouched and this card's work is unchanged.

Line 118 is the one that matters most. A claim-matrix Result cell is what a
reader consults to decide whether a path is safe, and it currently certifies a
property the source does not have, for code nothing has ever compiled.

A seventh entry is wrong in a different way. `ai-agent-boundary.md:321` cites
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

## Why this is blocked rather than ready

Owner direction, 2026-08-17: this card requires the Windows implementation to be
tested, so it belongs in `50-blocked/` with the rest of the Windows work.

The reasoning holds against the lane rule. Whichever resolution is chosen, this
card's terminal criterion is a *true and final* statement about Windows launch
behaviour, and no such statement can be made from a host that cannot compile the
backend. Option 1 makes it worse, not better: the restored guard would live
inside `#if defined(Q_OS_WIN)`, so no macOS selector could ever exercise it, and
"a test fails without the guard" is unprovable here by construction.

The cost is recorded rather than hidden. Until a Windows host exists, six
statements — including the `RFC-BPR-0011.md:118` claim-matrix Result cell
reading "Windows starts no child" — continue to assert a property the source
does not have. That is a known-false documentation claim being carried
deliberately, with its falseness written down here, which is the honest form of
the trade. Anyone reading those six statements before this card closes should
read this card instead.

If the owner later chooses option 2, the documentation half is portable and this
card returns to `20-ready/` immediately, with no other change required.

## Required final state

- No document asserts a Windows pre-start refusal that the source does not
  perform, in either resolution.
- `ai-agent-boundary.md:321`'s evidence row names a test whose assertions
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

## Lane — 2026-08-18

Returned to `10-backlog/` by [[BPR-0358]]. The section above is retained because
it records a real decision, but its load-bearing premise has expired: it argued
that "no such statement can be made from a host that cannot compile the
backend", and this checkout now runs on a Windows 11 x64 host with MSVC 14.51
and the Windows SDK. The backend can be compiled here.

It has not been. That is why this card is in `10-backlog/` rather than
`20-ready/`, and why it now declares `depends_on: [BPR-0240]` instead of an
empty list: [[BPR-0240]] owns compiling and proving
`src/adapters/agent/src/windows_agent_process.cpp` for the first time, and this
card's terminal criterion is a true and final statement about Windows launch
behaviour, which still cannot be written before that.

The choice between the two resolutions is untouched and remains the product
owner's. If it is still unmade when [[BPR-0240]] is terminal, this card becomes
a `40-parked/` card carrying `awaiting-product-decision`, because at that point
every dependency is done and the only missing input is a person.

Until then the cost recorded above stands, with one correction: the six
statements are no longer being carried because nothing can compile the backend.
They are being carried because nothing has yet.

## The premise has moved again — 2026-08-28

`windows_agent_process.cpp` is no longer uncompiled and unproven. [[BPR-0240]]
records the MSVC compile (2026-08-22), the first failing target-native canary
(2026-08-23), and the full conversion — the canary and all five composed agent
courses green — on 2026-08-26/27, with Job Object pre-resume assignment and
descendant containment proven and assignment-before-resume pinned in source
order at `88a8c063`. The lifecycle-proof statements in the verification ledger,
boundary document, and fixture catalog are reconciled to that record; every
pre-start-refusal statement this card enumerates stays marked known-false and
unresolved. The choice between restoring the guard and documenting the
dispatch remains the product owner's, and option 1's original rationale —
fail closed until target-native proof exists — should now be weighed against
that proof existing. This card stays in `10-backlog/` behind open
[[BPR-0240]].

## Parked on the product decision — 2026-09-01

[[BPR-0240]] closed today with the Windows backend proven target-native, so
every dependency is done and the only missing input is the choice this card
has always carried: restore the pre-start refusal, or document the shipped
dispatch. It is presented as a choose-one subject in
`docs/verification/reviews/run-boundary-decisions-v001/offline-package/index.html`
(group `windows-launch-documentation`). Whichever alternative is chosen, the
documentation half is portable and this card returns to `20-ready/`; a restored
guard additionally needs its Windows-only failing-without-it test on the
Windows station.

## Decided and closed — 2026-09-01

The product owner chose option 2, documenting the shipped dispatch, on the
`run-boundary-decisions-v001` page (batch
`bpr:/attended-review/betterpullreview/c7e77830ef4fa92c6047c99b`, subject
`decision:/betterpullreview/bpr-0332/document-the-shipped-dispatch`
accepted, the restore-the-guard alternative recorded as not chosen; the
validated verdict and record are beside the page).

The six statements now say what is true. `docs/verification/RFC-BPR-0011.md`'s
status paragraph and its "Not delivered" bullet describe the suspended-create,
assign, resume backend and its 2026-08-26/27 target-native proof instead of a
refusal; `docs/architecture/ai-agent-boundary.md`'s lifecycle paragraph and
its "What is not claimed" clause do the same; and the evidence row that cited
a success-ending test as proof of refusal now names the two Job Object tests
for the claim they actually support. Every `Known false at HEAD` banner
[[BPR-0401]] placed is gone with the falsehoods it marked. No source changed,
so no Windows runtime property is claimed beyond BPR-0240's record.

| Claim | Command | Result | Commit |
|---|---|---|---|
| No document asserts a Windows pre-start refusal | `grep -rn 'refuses Windows launch\|refuses before .QProcess::start\|Known false at HEAD' docs/verification/RFC-BPR-0011.md docs/architecture/ai-agent-boundary.md` | no matches | this commit |
| The evidence row supports its claim | the row now names `test_windows_assignment_failure_has_no_pre_assignment_side_effect` and `test_timeout_terminates_the_descendant_process_tree` for the containment claim | by inspection | this commit |
| The documentation gate holds | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
