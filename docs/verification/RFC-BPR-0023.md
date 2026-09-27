# RFC-BPR-0023 Verification Plan

Status: **implementation authorized under mandate r1; M1 complete; M2 architecture candidate awaits independent review; feature criteria pending**

Import baseline: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

Immutable implementation RFC_BASE_SHA: `899090f39304dca5a2c0b034c4684a50ca73605a`.

Owner mandate r1 (2026-09-26) authorizes implementation, live-plan revision and
coordinator integration of completed milestones to main. This supersedes the
import-only deferral; it does not satisfy acceptance. The
[live audit and evidence allocation](RFC-BPR-0023-live-audit.md) and
[adopted plan](../rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json) govern
execution. The protected imported issue-plan bytes remain unchanged.

# Acceptance Traceability for RFC-BPR-0023

This matrix retains all imported feature obligations. The live audit reconciles
their current direct surfaces and evidence owners; feature checks have not run.

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

## M1 — live audit and execution plan

Assignment: BPR-0576, `m1-live-audit`, implementer `/root/rfc0023_m1`, mandate
r1, branch `codex/rfc-bpr-0023`. Readiness commit `ff134d74`; MASK claim commit
`02537bcaf836186c9c74dd14f7e58a0d699a9c02`. The planning candidate is the commit
introducing the live audit and adopted plan; the completion/handoff below names
its exact revision. The coordinator owns landing this milestone on main.

| Check | Result and scope |
|---|---|
| `python3 scripts/verify_documentation.py --root .` | PASS, `DOCUMENTATION_OK`; live links, RFC/index and board structure. |
| `python3 tests/scripts/verify_documentation_tests.py` | PASS, 64 tests; expected negative fixtures print errors. |
| MASK `issue_board.py ... validate --rfc RFC-BPR-0023 --json` | PASS, valid across 1,208 indexed cards, scoped workflow errors empty. |
| MASK plan prepare/adopt/inspect and checkpoint bind | PASS, ten milestones, sixteen whole-card obligations covered exactly once; M1 has BPR-0576 and four explicit exits. Semantic acceptance remains engineering judgment. |
| Retained manifest / baseline comparison | PASS, all 26 listed payload hashes; retained pack and imported source-only issue plan unchanged against implementation base. |
| Canonical acceptance comparison | PASS, all eighteen RFC criterion texts byte-identical to implementation base. |
| `git diff --check` | PASS. |
| Production public-path audit | Source trace completed for current chat, findings, revisioned shared drafts and logical publication. No runtime execution or feature acceptance claimed. |

Self-review reconciled the ledger v6 and recovery v5 frontiers, volatile profile
shared drafts, updater salvage, temporary finding-claim chat replies, existing
Comments/Notes/Tour navigation and dedicated composition pages. It also exposed
the current all-note expansion in chat and the missing named total encoded-input
bound as explicit BPR-0577/0581/0582/0583 design obligations.

The initial `move --to ready` was refused because the helper requires the exact
lane `20-ready`; it changed nothing. The first claim after a successful move
refused its already-dirty LIVE_IMPORT link. Committing only that readiness move
and editable live link repaired the bookkeeping precondition; the repeated
claim succeeded. Neither refusal weakened the protected-path guard.

No product source, build, CTest, external agent, vault, GitHub/GHES mutation,
push, PR or installed package action was performed. No configured build tree
was inherited. BPR-0576 is planning acceptance only. All eighteen feature rows
remain Not run, and BPR-0577/BPR-0591 independent review, later designated
publication and rendered/native/human evidence remain pending where required.

The new plan JSON/source receipt is a fresh adopted plan under the helper's
required `docs/rfcs/plans/` location. Existing protected plan/pack bytes were not
edited. Live audit, receipt, selection and checkpoint are retained in
`docs/verification/`. Card amendments only activate the owner mandate, link the
plan and correct stale schema facts; future card acceptance and dependencies
remain intact.

## M1 durable handoff

Accepted planning candidate: `920bb62b526d900c049415b8ea88910c033fe1f3`.
This candidate contains all audit/plan/card/RFC changes; the succeeding MASK
finish commit changes BPR-0576 to Done, repairs its editable LIVE_IMPORT link
and stores this completion record. Source behavior is unchanged from
`899090f3`. No product acceptance is attached to this candidate.

The [bound checkpoint](RFC-BPR-0023.m1-checkpoint.json) and
[completion observation](RFC-BPR-0023.m1-observation.json) bind mandate r1,
segment `m1-initial`, BPR-0576 and all four M1 exits. The observation's candidate
is the accepted planning commit above, before completion bookkeeping.

Changed paths: sixteen RFC-0023 cards; live RFC/index; the existing verification
ledger; new live audit, adopted plan/derived view/source receipt, normalization
receipt, resolved strategy, checkpoint and observation; editable LIVE_IMPORT
link repair only. Original imported issue plan and retained pack are unchanged.
No source, tests, build configuration, binary or unrelated work is included.

Verification is complete for this planning scope. All task-owned commands have
finished; there are no background builds, child agents, review servers or
external writers owned by this milestone. The sole transient preparation root
is removed before return. No native/human candidate or protection lease is held.
The branch is released to the coordinator after the completion commit.

Next action: integrate accepted M1 into main, then dispatch a fresh implementer
for `m2-architecture` / BPR-0577. That card becomes dependency-eligible after
BPR-0576 is Done and remains Backlog until explicitly readied/claimed. Required
independent architecture review stays pending; later external publication,
rendered/native/human evidence and the final independent review remain owned by
their existing cards. No extra acceptance waiver accompanies this handoff.


## M2 — architecture candidate

Assignment: BPR-0577 / `m2-architecture` / `/root/rfc0023_m2` / mandate r1,
branch `codex/rfc-bpr-0023`. Starting M1/main candidate:
`cb36559911d6cd4def1582a0121b527ed2f9edce`. Readiness commit `afb0507d`;
claim commit `20ff2732e4e6d11df333ccc2fffbf3069029f7e3`.
The [live architecture contract](RFC-BPR-0023-live-audit.md#m2--live-architecture-contract)
is the review artifact on the existing audit, linked from the live RFC and card.
The [bound checkpoint](RFC-BPR-0023.m2-checkpoint.json) records the adopted M2
obligations. The candidate identity and checks are recorded in the durable handoff below.

The allocation extends existing shared draft and mutation owners, preserves
unknown legacy binding and provenance, retires flat-note authority atomically,
and gives concrete request/terminal, CAS, pagination, source-admission,
publication repair and Qt contracts. Corrected M1 observations distinguish
Review modality from Tour tab, App Server thread/turn from Claude session, and
selected code-text bounds from total private-input bounds. No product behavior
or external protocol capability is certified by these corrections.

### M2 acceptance and documentation impact

| Bound obligation | Result |
|---|---|
| BPR-0577 required final states / M2 exit 1 | Architecture candidate written: concrete authority/state/requests/ports/identities/deltas/projections at inspected current symbols. |
| M2 exit 2 | Architecture candidate written: migration/draft/run/paging/input/navigation choices and failure cases settled in the live contract; future implementation proof remains on its assigned cards. |
| M2 exit 3 | Eighteen-row evidence ownership retained and refined; **independent authority-boundary review pending**. M2 and whole card are not accepted. |

Changed live documents: this ledger, the existing live audit, the live RFC
allocation pointer, BPR-0577, its helper-managed LIVE_IMPORT link, and a new M2
checkpoint/observation. The authored/adopted plans, retained pack, RFC criteria,
explicit dependencies, main branch, product source and tests remain unchanged.
The M1 documentation-impact table remains applicable: these are future contracts
for ledger/drafts/agent/Qt ownership, with no new operator-visible behavior.
Owning shipped architecture/operator documents must be updated with their actual
implementation evidence in BPR-0580–0590. GitHub/GHES authority, actor, review
semantics, Git cache, credentials, network isolation, packaging, native/platform,
performance and accessibility acceptance are not changed or waived here.

### M2 verification and pending review

Focused verification for this documentation candidate consists of the configured
documentation gate, scoped board validation, checkpoint bind/check, criterion/
dependency/import integrity comparisons, source-symbol trace and whitespace.
The documentation regression suite was also rerun: all 64 tests passed, including
its expected failing-fixture diagnostics. No product build or CTest matrix was
run for these documentation-only changes.

The independent review must examine the exact committed allocation and source
baseline, then try counterexamples around source dispatch/edit/delete ordering,
lineage retention and migration, draft single authority, account/window
retirement, qualified metadata, total-input bounds and publication recovery.
The coordinator must obtain a separately authorized independent review before
accepting BPR-0577. No nested reviewer, worker or alternative model was launched.
This is an independent engineering gate, with no human UX/native candidate ready;
no attended session, server, URL, event cursor or protection lease is created.

### M2 durable handoff

| Check | Result / evidence scope |
|---|---|
| `python3 scripts/verify_documentation.py --root .` | PASS, `DOCUMENTATION_OK`; live links, status and board structure. |
| `python3 tests/scripts/verify_documentation_tests.py` | PASS, 64 tests; expected negative fixtures printed their errors. |
| MASK `issue_board.py --repo-root ROOT validate --rfc RFC-BPR-0023 --json` | PASS, 1,208 indexed cards, no errors. |
| MASK plan discovery and M2 checkpoint bind | PASS, adopted plan found, M1 accepted prerequisite, exact BPR-0577 obligation and three exits bound. |
| Python exact section/dependency/hash/diff comparison against `cb365599` | PASS, all 18 criterion texts and RFC dependencies unchanged; 26 retained payload hashes; plans, product source, tests and unrelated cards unchanged. |
| Body-continuation contract arithmetic | PASS, `0 → 524288 → 1048576` advances in two bounded reads. This checks the written allocation only; M4/M9 runtime/store proof is pending. |
| Current-symbol/public-composition trace | Inspected single chat/invocation/process and shared-draft/mutation routes; no runtime feature acceptance inferred. |
| `git diff --check` | PASS. |

Self-review checked every BPR-0577 final state against the allocation and the
original RFC. It explicitly distinguished durable scope from runtime authority,
unknown legacy fields, referenced deletion refusal, stale versus incomplete
output, whole-body versus displayed chunks, and ledger versus remote-journal
commit points. No independent review is claimed.

No task-owned command remains running. No process/build/render was started,
no descendant agent or review server exists, and no review protection is held.
The only test temporaries were owned and removed by the regression harness;
no task-owned temporary directory was retained. The sole repository writer was
this implementer; the coordinator owns subsequent integration. No live service,
credential, push, PR, installed-package or main-branch mutation was performed.
The sole next action is independent authority-boundary review of this candidate,
followed by any scoped correction and BPR-0577 acceptance. Only then may the
coordinator land M2 on main and start M3. All eighteen feature rows remain Not run;
later native/human/designated publication and final independent review gates
remain unchanged.
