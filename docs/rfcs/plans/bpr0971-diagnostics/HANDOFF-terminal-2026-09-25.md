# BPR-0971 terminal bounds handoff — 2026-09-25

## Status

Role: rfc-implementer | Model: gpt-6-astra | Effort: xhigh

Bounded assignment `r4-terminal-20260925`, baseline `f6de40b6`, branch
`codex/bpr0971-terminal-bounds`, source/tracker root
`/Users/jaeger/.codex/worktrees/bpr0971-terminal-bounds/BetterPullReview`.
The existing In Progress claim and `privacy-and-backpressure` milestone remain
unaccepted. Codex only, no delegated agents, no primary-checkout writes or push.

## Milestone result

- `86072651`: bound this continuation to the existing plan and claim.
- `e1dbf44b`: cap developer exception capture before storage/rendering at 2 KiB,
  flatten ASCII controls and mark truncation; retain first-failure ownership.
  Adds direct composition and installed-Git limit tests plus an opt-in terminal
  counterexample probe.
- `2b58ce29`: cap each Git diagnostic URL-replacement append before expansion.
  Returned diagnostic bytes, useful local detail and typed results are unchanged.

Changed product paths: `src/app/main.cpp`, `src/app/src/github_application.cpp`,
`src/app/src/startup_diagnostic.hpp`, the startup declaration comment in
`src/app/include/bpr/app/github_application.hpp`, and
`src/adapters/git/src/installed_git_process.cpp`. Tests own the new startup
course, extended Git course, opt-in MCP audit, registration and app-source pins.
The [terminal audit](../../../verification/bpr0971-terminal-bounds.md) is the
complete retained field/bound/failure record. RFC0027's content policy and the
parallel classifier candidate are unchanged.

## Verification

Dev configure/build pass. Final focused CTest 3/3 passes in 110.18 seconds:
`git_process_contract`, `application_startup_diagnostic_bounds`, and
`application_startup_failure_source`. Prior startup/Git run passed 3/3 in
107.57 seconds; the final run follows the Git append repair. There is no
cross-platform claim.

Architecture/composition integration passes 3/3 in 244.92 seconds, including
the actual tests-disabled Release application build and checks inside
`application_composition_contract`. Both app-source pins pass, and the mutation
branch pin is unchanged. The command is:

```sh
ctest --preset dev --output-on-failure -R '^(application_composition_contract|application_review_round_trip_composition_contract|architecture_boundaries)$'
```

Strict tidy completed on the changed translation units. The new test TUs and
main are clean; `github_application.cpp` retains baseline `exact_current`
naming, and `installed_git_process.cpp` retains baseline signed `flock` flags.
Both exact lines exist at `f6de40b6`; no rule was disabled. The first run caught
the new helper's `#pragma once`; it was replaced with an include guard and the
final source check no longer reports it. Formatter is Homebrew LLVM 23.1.1,
compiler AppleClang 21, Python 3.14.7, Qt 6.11.2, macOS 26.6.2 arm64.

Documentation gate (`PYTHONPATH` names the installed MASK scripts because this
isolated checkout has no local ignored skill deployment) and BPR0971 board
validation pass. Scoped formatting, Python compilation and `git diff --check`
pass. Raw strict tidy remains nonzero only for the two named baseline findings.

The opt-in MCP audit exits 1: 32,886 stderr bytes, synthetic name content and
injected line retained, credential value absent; a filled 65,536-byte stderr
pipe blocks for 1.001 seconds, and draining it lets the exact child exit 1.
That failure is the pending requirement, not an expected-passing regression.

## Review handoff

No human criterion is ready, and no new attended session was created. App-source
self-review and the unchanged production mutation-branch digest are recorded
on BPR0971. Existing RFC0052 session/candidate protections are untouched.

## Ownership and command state

Every task-owned configure, build, CTest, audit, tidy and documentation command
has finished. No server, watch, native app, fixture, pipe or repository writer
is handed off. Ignored output remains at `build/bpr0971-terminal-*.log` for
reproduction; no runtime output or synthetic child text is committed. The
isolated worktree/build are retained for coordinator integration. Writer
ownership is released after this checkpoint commit. No push was performed.

## Continuation and uncertainties

AC1 is partial. AC2/AC3 have reproduced MCP environment-name privacy/line
injection and full-pipe failures; application synchronous stderr and external
native/crash paths remain unqualified. Windows/Linux are owner-deferred.
The next engineering round must reconcile complete environment-observation
diagnostics with bounded, content-safe observations and an explicitly owned
nonblocking terminal path, then run the pending composed pathological courses.
Preserve useful unknown causes and drop/availability facts. This patch supplies
no privacy waiver, independent verdict or BPR0971 finish decision.
