---
schema: "implement-rfc.card.v1"
id: "BPR-0620"
title: "Stop agent process groups after application death"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0619"]
acceptance: [{"criterion":"forced-owner-death","outcome":"pass","revision":"bdcd3837b541a5fee608976822af0f27bc7df907","evidence":{"kind":"observation","detail":"Both owner-PID and isolated owner-process-group SIGKILL courses stop the stubborn agent and heartbeat descendant within five seconds before test cleanup; both fresh launches succeed. agent_parent_death passed 19.25s."}},{"criterion":"normal-contract-and-descriptors","outcome":"pass","revision":"bdcd3837b541a5fee608976822af0f27bc7df907","evidence":{"kind":"observation","detail":"qt_agent_launch_canary passed 70.80s, including normal protocol/environment, failed start, cancellation/timeout, descendants, and both unrelated high-FD and owner-channel closure."}},{"criterion":"exact-supervisor-authority","outcome":"pass","revision":"bdcd3837b541a5fee608976822af0f27bc7df907","evidence":{"kind":"observation","detail":"Missing owner channel refuses before side effect. Architecture scan plus three focused mutation tests pass; helper retains child PID until group signal and keeps distinct helper/agent groups. Windows backend unchanged."}},{"criterion":"release-and-gates","outcome":"pass","revision":"bdcd3837b541a5fee608976822af0f27bc7df907","evidence":{"kind":"observation","detail":"Tests-off app-release build passes; bundled and built helper hashes match, otool shows only libc++/libSystem. Four focused CTests pass, documentation gate and 62 contract tests pass; clang-format and diff checks pass. Local Mac only."}}]
---

# Stop agent process groups after application death

RFC-0013's recovery matrix requires forced death during an AI session. The
current Unix launcher creates an isolated process group and kills it during
cooperative cancellation/timeout, but those paths require the application to
remain alive. Windows already uses a kill-on-close Job Object. Existing
`qt_agent_launch_canary` exercises timeout and descendants, not parent death.

## Acceptance

- Use the concrete production agent launcher and the local, stubborn agent
  canary. Force-kill its owning application process after the agent and its
  descendant are observably running. A child that voluntarily exits on stdin
  EOF is insufficient proof.
- The agent and its descendant must stop within a bounded local observation
  window without relying on application destructors, timers or the test's
  cleanup. Detect failure before cleaning up exact fixture-created processes.
- A fresh launcher process must complete an ordinary canary invocation after
  the killed run. The normal protocol, environment filtering, cancellation,
  timeout and descendant-cleanup contract must remain passing.
- If a Unix lifetime guard is needed, prepare it before agent execution, keep
  its process-group authority exact, close unrelated inherited descriptors,
  and make setup failure refuse launch. It must not expand agent argv,
  environment or credential authority or require an independently installed
  executable. Windows Job Object behavior remains unchanged.
- Verify focused local process/protocol checks and a tests-off adapter build.
  Run network-isolated fake-agent fixtures on the Mac. No GitHub Actions,
  pushes, credentials, paid agent invocation or remote mutation.

## Scope and ownership

Own the agent process adapter and narrow Unix lifetime support, the existing
canary/test executable and its CMake registration, a bounded parent-death
runner, network-isolation registration if needed, and the owning architecture
paragraph. Reuse existing process-kill and contained-temp support when their
contracts fit; do not falsely treat heartbeat stability alone as termination.

This card covers child lifetime across forced application death. Complete
Git/SQLite/workspace restart during a real agent-host session remains a
separate RFC-0013 requirement to scope next, along with target-native evidence
outside this Mac. Do not close the parent RFC on this card's evidence.

## Reproduction

At `eaae8586` plus the regression fixture, `agent_parent_death` fails in
10.41 seconds: the concrete Unix launcher's owner is forcibly killed after a
stubborn agent and its heartbeat descendant start; at least one remains alive
after the five-second termination window. Cleanup then kills the exact two
fixture-reported PIDs and verifies their absence. An earlier harness attempt
used `ps`, which the test sandbox denies; its exact leaked descendant was
identified and killed, and the retained runner now uses direct process checks.
The failure is no longer an observation-tool failure. Network isolation
manifest passed in 3.58 seconds. No live agent, credential or service was used.

The repair will need a small bundled Unix supervisor with a parent-owned
lifetime channel. Forking a permanent watchdog from the full application would
retain its address space and inherited descriptors, so it is unsuitable for
this resource-limits slice. A bundled helper can start with only the selected
agent environment and protocol descriptors, keep the child PID unreaped until
group cleanup, and preserve the agent's launch/error/exit semantics. This is
part of the application package, not a separately acquired dependency. Include
its CMake/bundle placement and tests-off compile in this card's owned scope.

## Implementation

The Unix adapter now launches a bundled, Qt-free supervisor through QProcess.
A close-on-exec socket carries only startup errno and owner lifetime; unrelated
inherited descriptors are closed before helper execution, and the helper closes
the lifetime descriptor before executing the unchanged agent argv/environment.
The supervisor and agent have separate process groups. The supervisor retains
its direct child unreaped until signaling the exact agent group, then forwards
its exit status. Owner socket closure covers normal teardown and forced owner
PID/group death without relying on application cleanup. Setup failures refuse
agent execution; a missing lifetime channel is exercised directly. Windows
continues through its existing kill-on-close Job Object backend.

The release target copies the helper beside the application executable and
tracks helper changes as a link dependency. The architecture verifier names
this exact native process owner, allowing its fork and exact execv call while
retaining the other process/shell bans. Focused verifier mutations check the
helper target and reject other APIs/owners.

This owns descendants that remain in the assigned agent process group, not a
program deliberately escaping that group. A supervisor that is itself forcibly
killed cannot provide cleanup; the application-group course verifies the
supervisor is isolated from that particular failure. Target-native Linux and
Windows execution, and complete application workspace recovery, remain outside
this Mac-only slice.

## Verification — 2026-09-05

Production repair: `bdcd3837b541a5fee608976822af0f27bc7df907`; failing regression: `be4d99b7`.

- Local dev target `test_better_pull_review_qt_agent_launch_canary_tests`
  builds. Final network-isolated CTest selection
  `^(agent_parent_death|qt_agent_launch_canary|test_network_isolation_manifest|architecture_boundaries)$`
  passes 4/4 in 101.52 seconds: architecture 8.03s, isolation manifest 3.42s,
  complete protocol/environment/cancel/timeout/descriptor canary 70.80s, and
  two parent-death/relaunch courses 19.25s. Each crash course verifies both
  reported processes are absent before test cleanup, then checks a new launch.
- Three focused `architecture_boundary_tests.py` methods pass in 2.331s
  under `run_network_isolated_test.py`: exact Unix supervisor authority,
  required supervisor target, and alternate process API rejection everywhere.
- Tests-off `cmake --build --preset app-release --target app_better_pull_review -j4`
  passes. The built helper and `BetterPullReview.app/Contents/MacOS/` copy
  both have SHA-256 `07d3a2379bd0d0b3968d5a665cb7510d01d0d92b0ab1c1cd6441fd00756aeee6`.
  `otool -L` lists only libc++ and libSystem, with no Qt dependency.
- Documentation gate passes; all 62 documentation contract tests pass.
  Changed C++ files pass clang-format dry-run/Werror, and `git diff --check`
  passes. No whole-repository matrix, GitHub Actions, push, credential use,
  paid agent or remote mutation was run.

Task-created build logs are consumed and removed after recording these results;
the contained test roots and fixture processes are cleaned by their runners.
The normal release application was built but not launched or signed out.
