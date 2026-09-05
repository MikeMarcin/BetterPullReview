---
schema: "implement-rfc.card.v1"
id: "BPR-0621"
title: "Verify composed application recovery after an agent-session kill"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0620"]
acceptance: [{"criterion":"composed-forced-death","outcome":"pass","revision":"430b7c518c52b700129c6bba2b35c4bf3587051b","evidence":{"kind":"observation","detail":"The ordinary app launches its local canary in the exact-head workspace, reports agent/descendant PIDs, then receives SIGKILL. Both processes are absent before cleanup; final isolated crash lane passes 43.68s."}},{"criterion":"durable-state-and-no-replay","outcome":"pass","revision":"430b7c518c52b700129c6bba2b35c4bf3587051b","evidence":{"kind":"observation","detail":"Independent read-only SQLite integrity/FK/session/sequence/epoch checks and Git fsck/exact refs pass initially and after two ordinary reopens. Both reopens retain session #1/next #2 with zero launcher calls through shutdown."}},{"criterion":"explicit-new-session","outcome":"pass","revision":"430b7c518c52b700129c6bba2b35c4bf3587051b","evidence":{"kind":"observation","detail":"A real panel gesture admits session #2/next #3, completes successfully and removes the new workspace on normal shutdown. Prior session and all epoch/ref identities remain exact; the prior read receipt persists."}},{"criterion":"workspace-gap-tracked","outcome":"pass","revision":"430b7c518c52b700129c6bba2b35c4bf3587051b","evidence":{"kind":"observation","detail":"The interrupted root retains all 18 native/content entries through reopens and reinvocation. BPR-0622 is committed and linked to the open RFC closeout for durable exact reclamation; no complete resource recovery claimed."}},{"criterion":"focused-validation","outcome":"pass","revision":"430b7c518c52b700129c6bba2b35c4bf3587051b","evidence":{"kind":"observation","detail":"Dev composition target builds; new isolated lane passes 43.68s, ordinary seed/reopen contract 25.33s, isolation manifest 4.95s, docs gate/format/Python compile/diff checks pass. Reused inputs on ordinary/manifest paths remain unchanged. Local Mac only."}}]
---

# Verify composed application recovery after an agent-session kill

RFC-0013 requires forced death during an AI session. BPR-0620 proves the
concrete launcher stops its children, but does not open the real application,
Git cache, SQLite session catalog or disposable workspace on restart.

## Acceptance

- Extend the existing ordinary application composition fixture with a real
  local canary that reports its PID and descendant PID only while running in
  the materialized exact-head workspace. Kill the application with SIGKILL
  after session admission and observable descendant activity, without normal
  shutdown. Assert both processes stop before fixture cleanup.
- Independently inspect all fixture SQLite databases for integrity/foreign-key
  consistency and the exact committed agent-session/sequence identity. Inspect
  the retained bare Git cache with fsck and exact refs. Reopen the application
  twice without starting an agent: both opens retain session #1/next #2 and
  start zero agent processes. No remote publication or automatic replay.
- A new explicit invocation through the real UI/host/process composition uses
  session #2, succeeds in a fresh workspace and cleans that new workspace at
  shutdown. Independent persisted-state checks retain both exact session
  bindings and next sequence, without duplicate epochs or changed cache refs.
- Observe the killed workspace through each restart. Preserve its exact native
  identity/content if it remains; record any missing reclamation as a separate
  bounded recovery card, not as full resource recovery. Do not acquire deletion
  authority from a matching directory name. Test-owned artifacts are removed
  only after all assertions and exact fixture process cleanup.
- Run the new network-isolated composed kill lane, existing ordinary composed
  seed/reopen lane and isolation manifest on the Mac, plus the documentation
  gate and changed-source format/diff checks. No GitHub Actions, push, live
  credentials, paid agent, network service or remote mutation.

## Ownership and closeout

Own the existing agent composition fixture, local agent canary, end-to-end
CMake registration, bounded Python kill/restart runner and narrow shared kill
runner argument/timeout support. Production fixes require an observed failure
and explicit ownership refinement. Anchor: BPR-0617, RFC base
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`. RFC-0013 remains open for any workspace
reclamation gap and the remaining performance/resource criteria.

## Implementation and observations

Source: `430b7c518c52b700129c6bba2b35c4bf3587051b`. The new `application_agent_crash_recovery` lane runs
`crash-running` through the existing application fixture and local canary,
waits for reported agent/descendant identities and the exact materialized head,
and forcibly kills the application's process. Both fixture processes must be
absent within five seconds before cleanup can contribute. The runner opens
all fixture SQLite databases read-only, checks integrity and foreign keys,
compares exact session bindings/sequence and epoch rows, and runs independent
Git fsck/ref reads. Two ordinary `positive-reopen` runs preserve session #1 and
next #2 with zero agent starts through shutdown. `crash-reinvoke` preserves the
old read receipt, dispatches an explicit gesture, verifies successful session
#2/next #3, then verifies normal new-workspace removal. No production source
or runtime credential state was changed by this card.

The first reinvocation attempt failed at the fixture's receipt check: reusing
its fresh-start gesture toggled an already-persisted read receipt off. Fixed
the fixture to retain and assert that receipt on reinvocation. This was a
harness error, not an application recovery defect.

The actual resource defect is separate: the interrupted workspace's 18 entries
retain exact native identity, mode, size and content through both reopens and
the new invocation. BPR-0622 (`5718607f`) records durable exact ownership and
reclamation, including live-owner/substitution negatives and bounded admission.
This observation intentionally does not claim completed workspace cleanup.

## Verification — 2026-09-05

- Dev target `test_better_pull_review_agent_invocation_composition_public_path`
  builds. Final `ctest --preset dev -R '^application_agent_crash_recovery$'
  --output-on-failure` passes in 43.68s (43.69s total), including exact initial
  head, child absence, both reopens, explicit #2, independent state checks and
  the reported 18-entry orphan preservation. The preceding run passed 44.22s;
  the final assertion strengthens the initial exact-head evidence.
- Existing `application_agent_invocation_composition_contract` passes in 25.33s,
  including the strengthened zero-replay checks on its ordinary seed/reopen
  path. Its selected behavior is unchanged by the final crash-only fixture fix.
  `test_network_isolation_manifest` passes in 4.95s; registration did not change
  afterward. No live service, paid agent, GitHub Actions or push was used.
- Documentation gate reports `DOCUMENTATION_OK`; changed C++ files pass
  clang-format dry-run/Werror, Python compilation passes, and `git diff --check`
  passes. Full matrices and tests-off application rebuild are not needed for
  this fixture/documentation-only slice.
- Contained runners remove their roots after consuming assertions. Exact
  fixture PID cleanup is only a failure fallback and cannot turn a failed
  termination check into a pass. The build log, acceptance JSON and new
  runner's compiled Python cache are removed after bookkeeping.

BPR-0621 is complete as composed state-recovery evidence. BPR-0622 and RFC-0013
remain open; neither cross-platform recovery nor complete resource reclamation
is inferred from this Mac result.
