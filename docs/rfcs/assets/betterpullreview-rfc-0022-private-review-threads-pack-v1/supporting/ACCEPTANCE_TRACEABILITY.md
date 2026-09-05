# Acceptance Traceability for RFC-BPR-0022

This matrix is pack planning source. Live issue IDs replace provisional card
IDs during import.

| AC | Requirement summary | Primary provisional cards | Minimum direct evidence |
|---|---|---|---|
| AC-01 | Private thread/revision history survives restart | 03, 05, 07, 13 | Core replay + SQLite restart + public UI reopen |
| AC-02 | Side chat persists through existing invocation path and exact binding | 07, 08, 12 | Invocation trace, stale-head fixture, transcript restart |
| AC-03 | Every revision has typed actor | 03, 04, 05 | Core actor exhaustiveness and migration tests |
| AC-04 | Reviewer edit creates successor with immutable lineage | 03, 04, 05, 11 | Core history replay + store byte comparison + UI history |
| AC-05 | Requested/configured/reported/self-reported values stay separate | 04, 06, 11 | Protocol capability matrix and conflicting-value fixtures |
| AC-06 | Multiple runs per session do not collide; pruning is safe | 04, 05, 06, 07 | Multi-turn run tests, sequence tests, prune/restart test |
| AC-07 | Exact semantic subjects/targets; stale never guessed | 03, 04, 08, 11 | Core anchor fixtures and rendered stale/unplaceable states |
| AC-08 | Select sources and use reviewer/copy/agent composition modes | 04, 08, 11, 12 | Core/host/Qt mode tests through real window |
| AC-09 | Squash to Draft performs no mutation | 08, 09, 12, 13 | Mutation-port canary reports zero calls until Publish |
| AC-10 | Sources retained; multi-target selection explicit or split | 04, 08, 11 | Source digest checks, partition tests, UI target test |
| AC-11 | Accepted composition/draft survives restart | 05, 08, 12, 13 | Kill/restart exact draft and composition recovery |
| AC-12 | Publish remains separate RFC-BPR-0008 path | 09, 12, 13 | End-to-end proposal/coordinator/adapter trace |
| AC-13 | Server ID lineage, ordinary body, authenticated identity | 09, 12, 13 | Designated fixture mutation plus independent server read |
| AC-14 | Legacy notes migrate as `legacy_unknown` without dual authority | 05, 13 | v1-current fixtures, byte/digest comparison, schema inventory |
| AC-15 | Paging/quotas/refusals are bounded and non-truncating | 05, 13 | Boundary tests, benchmark, model statistics |
| AC-16 | Private context crosses only by exact opt-in/revalidation | 07, 08, 13 | Child-input canary and deleted/edited/cross-review refusals |
| AC-17 | Local deletion/crash does not change GitHub | 05, 09, 13 | Process-kill, purge, database-loss, independent GitHub read |
| AC-18 | Real composed UI, keyboard/a11y/scaling, no whole resets | 10, 11, 12, 14 | Qt automation and rendered evidence at 100/125/200% |

## Evidence rules

- A helper or unit-test-only composition does not prove public reachability.
- A static screenshot does not prove a command path, focus order, or mutation
  boundary.
- A fixture-authored model/effort value proves parsing and presentation, not
  that an external provider always returns the value.
- A local mock receipt does not prove ordinary GitHub body/identity semantics;
  use a designated target or a signed, independently readable recorded
  response as allowed by repository policy.
- A test that sees “no mutation” must instrument every mutation capability, not
  merely one adapter method.
- A migration test must inspect both data and the canonical schema; preserving
  text while leaving dual authorities is failure.
- Performance evidence must report active-set/changed-set work, page bounds,
  model reset counts, peak memory, and database growth for the tested fixture.

## Required architecture-conformance claims

| Claim | Evidence |
|---|---|
| Core has no forbidden dependency | Architecture-boundary gate plus include/link inventory |
| Host owns effects and serialized completions | Fake ports/manual executor tests with reordering, cancellation, duplicate completion |
| Qt emits intents and applies incremental deltas | Qt tests, model statistics, no direct adapter/store includes |
| Agent path is singular | Composition trace proves side chat and squash dispatch through existing invocation host |
| GitHub path is singular | Publish trace proves RFC-BPR-0008 coordinator and adapter only |
| Private/shared authority remains distinct | Database-loss test and independent server read |
| Provenance never exceeds evidence | Full/partial/conflicting/unknown capability fixtures and UI assertions |
| Source lineage is immutable | Digest and predecessor comparison before/after edits/composition/publication |
