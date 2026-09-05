# RFC-BPR-0023 Verification Plan

Status: **imported plan; implementation deferred; no criterion satisfied**

RFC base: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

# Acceptance Traceability for RFC-BPR-0023

This matrix is imported planning source. Checks are planned and have not run.

| AC | Requirement summary | Primary live cards | Minimum direct evidence |
|---|---|---|---|
| AC-01 | Private thread/revision history survives restart | BPR-0578, BPR-0580, BPR-0582, BPR-0588 | Core replay + SQLite restart + public UI reopen |
| AC-02 | Side chat persists through existing invocation path and exact binding | BPR-0582, BPR-0583, BPR-0587 | Invocation trace, stale-head fixture, transcript restart |
| AC-03 | Every revision has typed actor | BPR-0578, BPR-0579, BPR-0580 | Core actor exhaustiveness and migration tests |
| AC-04 | Reviewer edit creates successor with immutable lineage | BPR-0578, BPR-0579, BPR-0580, BPR-0586 | Core history replay + store byte comparison + UI history |
| AC-05 | Requested/configured/reported/self-reported values stay separate | BPR-0579, BPR-0581, BPR-0586 | Protocol capability matrix and conflicting-value fixtures |
| AC-06 | Multiple runs per session do not collide; pruning is safe | BPR-0579, BPR-0580, BPR-0581, BPR-0582 | Multi-turn run tests, sequence tests, prune/restart test |
| AC-07 | Exact semantic subjects/targets; stale never guessed | BPR-0578, BPR-0579, BPR-0583, BPR-0586 | Core anchor fixtures and rendered stale/unplaceable states |
| AC-08 | Select sources and use reviewer/copy/agent composition modes | BPR-0579, BPR-0583, BPR-0586, BPR-0587 | Core/host/Qt mode tests through real window |
| AC-09 | Squash to Draft performs no mutation | BPR-0583, BPR-0584, BPR-0587, BPR-0588 | Mutation-port canary reports zero calls until Publish |
| AC-10 | Sources retained; multi-target selection explicit or split | BPR-0579, BPR-0583, BPR-0586 | Source digest checks, partition tests, UI target test |
| AC-11 | Accepted composition/draft survives restart | BPR-0580, BPR-0583, BPR-0587, BPR-0588 | Kill/restart exact draft and composition recovery |
| AC-12 | Publish remains separate RFC-BPR-0008 path | BPR-0584, BPR-0587, BPR-0588 | End-to-end proposal/coordinator/adapter trace |
| AC-13 | Server ID lineage, ordinary body, authenticated identity | BPR-0584, BPR-0587, BPR-0588 | Designated fixture mutation plus independent server read |
| AC-14 | Legacy notes migrate as `legacy_unknown` without dual authority | BPR-0580, BPR-0588 | v1-current fixtures, byte/digest comparison, schema inventory |
| AC-15 | Paging/quotas/refusals are bounded and non-truncating | BPR-0580, BPR-0588 | Boundary tests, benchmark, model statistics |
| AC-16 | Private context crosses only by exact opt-in/revalidation | BPR-0582, BPR-0583, BPR-0588 | Child-input canary and deleted/edited/cross-review refusals |
| AC-17 | Local deletion/crash does not change GitHub | BPR-0580, BPR-0584, BPR-0588 | Process-kill, purge, database-loss, independent GitHub read |
| AC-18 | Real composed UI, keyboard/a11y/scaling, no whole resets | BPR-0585, BPR-0586, BPR-0587, BPR-0589 | Qt automation and rendered evidence at 100/125/200% |

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

## Canonical criteria

| AC | Criterion | Result | Producing commit |
|---|---|---|---|
| AC-01 | A reviewer can create a PR-level or exact semantic-subject private thread, add reviewer turns, restart BetterPullReview, and recover the thread and its revision history exactly. | Not run | None |
| AC-02 | Side-chat prompts and complete assistant responses can be retained as turns without adding a second agent launch path, and the thread remains bound to the exact review revision it was created against. | Not run | None |
| AC-03 | Every turn revision has a typed actor. New reviewer text is `reviewer`, agent text names one exact `agent_run_id`, and migrated notes are `legacy_unknown`. | Not run | None |
| AC-04 | Editing agent-authored text creates a reviewer-authored successor with an immutable `edited_from` edge; the original bytes, actor, and run provenance remain inspectable. | Not run | None |
| AC-05 | Agent provenance retains requested, configured, protocol-reported, and content-self-reported model/effort facts separately. Missing facts remain unknown, disagreements are visible, and no value is inferred from a profile or executable name. | Not run | None |
| AC-06 | One agent session can produce multiple uniquely identified runs and finding sets without collision; retained provenance remains readable after session catalog pruning. | Not run | None |
| AC-07 | Thread subjects and composition targets use exact semantic identities. Head/epoch movement produces visible stale or unplaceable state and never a nearest-line or silent reanchor. | Not run | None |
| AC-08 | A reviewer can select exact revisions/findings, open a composition workspace, inspect sources and provenance, and produce a reviewer-authored, exact-copy, or explicitly agent-assisted candidate. | Not run | None |
| AC-09 | `Squash to Draft` creates only local composition/draft state. Tests prove zero GitHub mutation, zero resolve operation, and zero review submission at this step. | Not run | None |
| AC-10 | Sources remain present and traceable after composition. Multiple exact targets require explicit target selection or Split into Drafts; the product does not guess. | Not run | None |
| AC-11 | Accepting a candidate records the exact composition revision on a durable RFC-BPR-0008 draft and survives restart. | Not run | None |
| AC-12 | Publishing requires a separate explicit gesture and traverses the existing target/head/acting-identity/idempotency/safe-target mutation path. | Not run | None |
| AC-13 | A successful publication records local lineage to the returned server comment and reconciled server state. The GitHub body contains no hidden or mandatory BetterPullReview/AI marker and appears under the authenticated reviewer's identity. | Not run | None |
| AC-14 | Legacy local notes migrate byte-for-byte into one-turn private threads with unknown authorship, no invented provenance, no ID reuse, and no second private-note authority. | Not run | None |
| AC-15 | Private thread pages, provenance details, and composition inputs are bounded and cursor-loaded. Quota, oversize, and stale-source cases refuse whole and never silently truncate. | Not run | None |
| AC-16 | Private content reaches an agent only when exact source revisions are explicitly selected and successfully revalidated at dispatch. | Not run | None |
| AC-17 | Deleting the local ledger or an explicitly selected private thread changes no GitHub state; the remaining ledger recovers transactionally after abrupt termination. | Not run | None |
| AC-18 | The complete workflow is reachable through the real BetterPullReview composition root and is usable by keyboard and assistive technology at 100%, 125%, and 200% scale without clipping or whole-model resets for a bounded turn update. | Not run | None |

The public-path course also covers RFC-BPR-0022 native and file-permalink
representations, logical receipt recovery, and distinct approved-prose/public-body
digests. The quota course covers 1 MiB bodies through 512 KiB continuations.

## Import verification scope

Only pack integrity, mapping/dependency/status consistency, documentation checks,
and diff whitespace are verified by this import. No build, CTest, UI, migration,
agent, GitHub, or GHES feature evidence was run. See [import record](../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md).
