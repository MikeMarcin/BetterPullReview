# RFC-BPR-0023 Verification Plan

Status: **implementation authorized under mandate r2; M1–M5 complete; final independent review and integrated feature criteria pending**

Import baseline: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

Immutable implementation RFC_BASE_SHA: `899090f39304dca5a2c0b034c4684a50ca73605a`.

Owner mandate r1 (2026-09-26) authorizes implementation, live-plan revision and
coordinator integration of completed milestones to main. This supersedes the
import-only deferral; it does not satisfy acceptance. Owner amendment r2
(2026-09-26) directs implementation to continue and moves M2's independent
architecture review into one capability-critical review of the final integrated
candidate at M10/BPR-0591. That final review is authorized and required before
RFC closeout; it has not been supplied. Milestone completions under r2 are recorded chronologically below. Earlier r1 handoffs remain historical observations. The
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

**Historical r1 record.** This section describes candidate `b98ddfd4` and handoff
`ba8cb88e`; its review wait is superseded only by the explicit r2 amendment below.
The prior checkpoint/observation remain retrievable at commit
`ba8cb88e942981ee925ef77a2d05d689fe8ae13f` under their recorded paths. Current
checkpoint/observation links now carry the renewed r2 binding and completion.

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

No task-owned command remains running. No product process, build or render was started,
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


Candidate-ready architecture revision:
`b98ddfd477962c4768ab291e646a3fecd4e1600e`.
Its review range starts at M1/main
`cb36559911d6cd4def1582a0121b527ed2f9edce` and includes readiness/claim plus the
allocation commit. This final handoff commit adds only the observation and this
completion record; it does not revise the architecture artifact or claim M2
acceptance. Review the allocation and live RFC/card at the candidate above.

The [M2 observation](RFC-BPR-0023.m2-observation.json) binds the exact candidate,
mandate r1, segment `m2-initial`, the whole-card obligation and all three exits.
`milestone_checkpoint.py check --repo-root ROOT --checkpoint
docs/verification/RFC-BPR-0023.m2-checkpoint.json --observation
docs/verification/RFC-BPR-0023.m2-observation.json --execution quiescent
--accepted-milestone m1-live-audit --json` returned **pending** on that exact
HEAD, with engineering acceptance explicitly not evaluated. A later handoff-only
HEAD differs from its candidate field by design; any renewed engineering round
must reconcile/rebind rather than treating that identity mismatch as acceptance.

Changed paths across M2 are the BPR-0577 move from Backlog through Ready to
In Progress; editable LIVE_IMPORT link; live RFC; existing audit and ledger;
new M2 checkpoint and observation. No other card or code path changed.
The leaf releases repository writer ownership on return. There are no handed-off
commands or retained temporary files. Review server/session/cursor: none.
Required decision: independent authority-boundary review only. No user acceptance,
permission or waiver is inferred from this candidate-ready result.

Architecture artifact SHA-256:
`4ca9348710eeff300f5ebd5c8fffb268294b2fc65b6296b4b90bb670cfe66255`.


## M2 — completed architecture under amendment r2

Assignment remains BPR-0577 / `m2-architecture` / `/root/rfc0023_m2` on
`codex/rfc-bpr-0023`. The owner amendment received 2026-09-26 is **r2**:
continue implementation now and perform the independent review at the end of
RFC-BPR-0023. This is an explicit change to review timing, not review acceptance.
The original architecture candidate is `b98ddfd477962c4768ab291e646a3fecd4e1600e`;
its pending handoff is `ba8cb88e942981ee925ef77a2d05d689fe8ae13f`.

The adopted plan now gives M2 three architecture exits and carries its exact
former review exit into M10. M10's existing capability-critical review exit and
that carried exit are fulfilled by **one independent review of the final
integrated candidate**, with every original review obligation preserved.
BPR-0591 names the complete scope: creation, agent dispatch, composition,
draft acceptance, separate publication, reconciliation, migration, cleanup and
crash paths; source dispatch/edit/delete ordering; immutable lineage; single
draft authority; account/window retirement; qualified metadata; input/body
limits; and the ledger/remote-journal join. Material findings require correction
and verification before closeout. M2 self-review cannot satisfy this final gate.
No nested reviewer is dispatched now.

The [amendment check receipt](RFC-BPR-0023.m2-amendment-r2.json) records the exact
before/after plan fingerprints and user source basis. Its affected milestones
are only M2 and M10. Every original work selector, dependency and exit text is
preserved; the old M2 review exit moved rather than disappeared. The generated
MILESTONES view was regenerated from the amended JSON. The original source-only
issue plan, retained pack and adoption source receipt remain unchanged.

### M2 r2 architecture acceptance

| BPR-0577 final state | Evidence and judgment |
|---|---|
| Authoritative state and compact typed core/host/Qt contracts | PASS — live allocation names state ownership, intent/request/terminal algebra, exact identities, bounded staging and semantic deltas. |
| One existing agent launch and GitHub mutation path | PASS — current-symbol/public source trace identifies chat/invocation/process and draft/mutation/coordinator routes; contracts add no second launcher or publisher. |
| Current draft authority, targets and durable linkage | PASS — existing shared revisioned draft host owns policy/fan-out; same-ledger acceptance/CAS and exact composition/draft linkage are specified, with separate RFC-0008/0022 publication. |
| Migration frontier, origins, sequences and one authority | PASS — ledger v6/recovery v5 distinguished; v1–v6 conversion, exact legacy/CAS/high-water preservation, no-dual-authority and reference-aware cleanup specified. |
| Private/shared navigation and bounded query/projection | PASS — current modalities/tray/centre pages, stable models, focus/accessibility, cursor/body bounds and total input refusal specified. |
| Every criterion has direct surface and evidence owner | PASS — all eighteen canonical criteria retain their matrix rows/cards and refined falsifiers; no feature row is marked passed. |
| Review disposition under r2 | PASS — independent architecture review carried intact to BPR-0591/M10's one final review; final review itself remains pending. |

M2 acceptance is architecture-only. Product source is unchanged, all eighteen
feature criteria remain Not run, and no native/human/designated publication
criterion is waived. The plan/mandate refinement does not certify feature
behavior, external protocol support or independent review.

### M2 r2 verification and continuation

The affected documentation gate, scoped board validation, plan discovery,
M2 rebind/check and exact requirement/integrity comparisons are rerun on the
amended candidate. The documentation regression suite passed all 64 tests again. Unchanged
source/public-path inspection remains evidence for the same implementation;
the final candidate identity is recorded in the handoff.
The bound checkpoint uses `mandate_revision: r2`, segment
`m2-review-timing-r2`; the observation covers BPR-0577 and all amended M2 exits.
Historical r1 fingerprints are preserved in Git and the amendment receipt.

This same implementer resumed from a clean `ba8cb88e` after all previous commands
and writers had stopped. No review protection/session/server existed. Only the
explicitly assigned M2 documents, adopted plan/view, BPR-0591 carry-forward and
current RFC/index narrative were edited. The r2 proposal/check used one
TemporaryDirectory, removed when the command completed. No product process,
build, external agent, live service, push, PR or main-branch action was performed.
All task-owned commands must be quiescent at handoff; no command transfers.

Next: coordinator integrates completed M2 to main and starts a fresh M3
implementer for BPR-0578/BPR-0579. The final capability-critical independent
review remains a required M10/BPR-0591 obligation under r2; the coordinator has
authorization to obtain it at the end after implementation. Unavailable native,
human and designated publication evidence remains on the existing later cards.


| Amendment verification | Result |
|---|---|
| `milestone_plan.py check-amendment` | PASS; only M2/M10 affected, same scope/work/dependencies, all 26 original exit texts retained. Receipt linked above. |
| `milestone_plan.py discover` and `milestone_checkpoint.py bind` | PASS; amended generated view matches the live plan; renewed M2/r2 binding names all three exits and BPR-0577. |
| `python3 scripts/verify_documentation.py --root .` | PASS, `DOCUMENTATION_OK`. |
| `python3 tests/scripts/verify_documentation_tests.py` | PASS, 64 tests; expected negative-fixture diagnostics. |
| Scoped MASK board validation | PASS, 1,208 cards, no errors. |
| Python exact comparison against `ba8cb88e` | PASS; 18 RFC criteria and dependencies unchanged; all original plan exits/work/dependencies/boundaries retained; 26 pack hashes; product source/tests and source-only plan/provenance unchanged. |
| `git diff --check` | PASS. |

The technical allocation remains the previously checked M2 design. Amendment
edits change review timing and records only. There are no unresolved M2
architecture obligations under r2; BPR-0591 retains the unsatisfied final review.


### M2 r2 completed handoff

Accepted architecture/amendment candidate:
`8a11272be9eb629165113c58b4ab1e7d6cadb029`. The technical design originated at `b98ddfd4`; this candidate
adds owner amendment r2, the revised plan/view, BPR-0577/BPR-0591 review
allocation, current RFC/index narrative, and renewed M2 checkpoint. The finish
commit changes BPR-0577 to Done, repairs its editable LIVE_IMPORT link, and
stores this completion plus the [r2 observation](RFC-BPR-0023.m2-observation.json).
It does not certify product behavior or the final independent review.

The [r2 checkpoint](RFC-BPR-0023.m2-checkpoint.json) and observation bind run
`rfc-bpr-0023-2026-09-26`, segment `m2-review-timing-r2`, mandate r2 and the exact
candidate above. BPR-0577 and all three M2 exits pass; `required_pending` is
empty for M2. The final independent review is a distinct pending M10/BPR-0591
obligation, not an omitted RFC gate. After finish, the completion-only HEAD
will differ from the observation's candidate; exact acceptance remains bound
to the candidate above and the relevant artifact bytes are unchanged.

All task-owned commands are complete at return. The amendment proposal and
acceptance decision use automatically removed temporary directories; the test
harness removed its own fixtures. No child agent, product process, server,
protection lease or retained temporary work exists. The implementer releases
writer ownership to the coordinator. Main integration is the coordinator's next
action, followed by fresh M3 work; there is no pending M2 decision.

Architecture artifact SHA-256 under r2:
`59d617cf08de116ab81ad8bc2c0b7f64a8b0acc42a4538494a98b029e5d4c169`.


The candidate-bound `milestone_checkpoint.py check` command (same flags as the
r1 check, current r2 files, execution quiescent and accepted M1) returned
`fresh-context` for M3 on `8a11272be9eb629165113c58b4ab1e7d6cadb029`.
The helper reports engineering acceptance as not evaluated; the explicit
architecture judgments and owner timing amendment above supply that decision.

## M3 — core policy implementation

Assignment: `m3-core`, BPR-0578 then BPR-0579, `/root/rfc0023_m3`, mandate r2,
branch `codex/rfc-bpr-0023`, integrated M2 baseline
`1f964cfa86f1b69cf2233f024e2ba9c975c19b21`. The
[M3 checkpoint](RFC-BPR-0023.m3-checkpoint.json) binds both whole-card
obligations and the two milestone exits. The owner moved independent review
to M10/BPR-0591; no review acceptance is inferred here.

### BPR-0578 implementation and transition contract

`private_review.hpp/.cpp` defines independent positive INT64-bounded IDs,
profile/review/ledger-incarnation scope, immutable exact revision bindings and
an explicit legacy binding with no invented OIDs. Subjects reuse RFC-0004
identities. Diff subjects require the validated immutable semantic index;
PR, shared-conversation and finding subjects are distinct alternatives.
Shared/finding existence remains a store fact to revalidate in M4/M6.

| Public core operation | Accepted change | Refusal examples |
|---|---|---|
| `create_private_thread` | Exact subject and first reviewer revision | Invalid IDs/binding, missing index, wrong comparison/epoch, invalid actor, oversize |
| `migrate_private_thread` | Exact legacy bytes and unknown actor | Invented actor, invalid retained attachment, oversize |
| `append_private_turn` | New revision and header version; optional exact reply | Archived/header CAS, wrong run/binding/completion, foreign source, backwards order |
| `edit_private_revision` | Reviewer successor, exact predecessor and edit/adoption edge | Tip/header CAS, source digest, actor, missing explicit incomplete adoption, backwards order |
| `set_private_lifecycle` | Open/resolved-locally/archived, idempotent same-state | Invalid enum/ID, old CAS, version exhaustion |
| `observe_private_subject` | Current/stale/unplaceable fact only | Unknown legacy or unavailable subject stays unplaceable; no replacement anchor |
| Request lane begin/finish | Checked generation; matching terminal consumed once | Wrong scope/incarnation/window/session/request/generation; duplicates and exhaustion |

Bodies are bounded values in operation observations; the compact four-lane
state holds identities only. SHA-256 covers exact body bytes. Complete output
is eligible; incomplete/failed/cancelled agent output requires a new explicit
reviewer adoption. The host remains responsible for transactional allocation,
source existence, exact latest CAS observations and persisted publication of
the returned proposal. No new persistence or user-visible application path is
claimed by this core slice.

Core tests instantiate no QApplication and directly compose the public create,
reply, agent-result, edit, archive/reopen and stale-observation APIs with a real
computed diff semantic index. They cover a 3-by-3 lifecycle table, repeated
successor replay, invalid identities, byte limits, unknown migration,
source/digest/CAS refusal and stale/duplicate terminals. Source and tests are
linked exclusively to `BetterPullReview::Core` in the core-only preset.

BPR-0578 verification: `cmake --preset core-only` and focused build
`cmake --build --preset core-only --target test_private_review_core -j 6`
passed on AppleClang 21 / arm64 macOS. `core_private_review`,
`architecture_boundaries` and `architecture_boundary_contract` passed (3/3).
The final high-water self-review correction reran the core test successfully;
`python3 scripts/verify_documentation.py --root .` returned DOCUMENTATION_OK,
scoped board validation passed (1,208 cards), and whitespace passed.

Documentation impact for this slice: the owning domain/state document records
the implemented core API. No user-visible UI, GitHub/GHES authority, acting
identity, review semantics, local cache, credential, network isolation,
packaging, accessibility or native behavior changes. Limits are core admission
bounds only; storage quotas, paging, performance and full RFC criteria still
require the assigned later milestones. No product-wide suite or live service
was exercised. Build outputs remain ignored and no transient evidence is committed.

### BPR-0579 core policy and integrated result

`review_composition.hpp/.cpp` provides qualified runtime facts with bounded
UTF-8 scalar validation, conflict locations, coalesced identical reports and
retained conflicting reports. A run snapshots its exact session/profile/
protocol/entry-point/binding independently of any session catalogue. Complete
conversation output now also checks the exact run output digest in
`private_review.cpp`; merely matching run ID cannot claim different bytes.

Source references distinguish turns, run+ordinal findings and composition
revisions. All carry scope/incarnation, common vertex order, digest and byte
count. Versioned, length-framed canonical encodings bind full finding content
and selected identities. Finding observations retain verdict/analyzed revision
facts. Source admission rechecks exact digest/tip, same review and completion;
historical selection is explicit. Derivation admission refuses later edge
attachment, duplicate/missing/cross-review sources, self edges and non-increasing
orders. No transitive history load is required.

Reviewer-authored, exact-copy and agent-assisted proposals retain immutable
sources. Exact copy preserves source actor and bytes; an agent proposal requires
its exact complete run, selected set, binding and output digest. Editing every
mode yields a reviewer successor. Target partitioning returns all assigned
partitions and unassigned source indexes. A mixed set has no implicit target.
Acceptance revalidates source/tip/body/target and produces a local request for
`revisioned_review_draft`; a separate durable acknowledgement changes lifecycle.
There is no composition publish/resolve/submit variant.

The existing revisioned draft now has optional scope/composition/revision/body
correlation. Linked-body edits require a matching immutable successor;
backwards, changed-same-ID, corrupt-body and cross-composition links refuse.
All accepted ordinary draft transitions preserve correlation. The existing
Publish command remains a separate operation. Duplicate exact source-set,
body and target evidence is an advisory and cannot trigger or block an action.

#### Acceptance coverage

| Card final state | Direct M3 evidence |
|---|---|
| BPR-0578 IDs/subjects/lifecycle/actor/immutability/completion/replay | `core_private_review`: real diff index, create/reply/run/edit/adoption, exact bytes, 3x3 lifecycle table, 98 successor steps, stale/unplaceable and request identity refusals. |
| BPR-0579 run identity and provenance categories | `core_review_composition`: same-session distinct runs, unknown/partial/full/conflicting metadata, report repetition/count/byte limits, invalid UTF-8/control strings, profile-name non-inference. |
| Finding identity and immutable lineage | Same-ordinal distinct-run findings, complete structured snapshot/verdict retention, digest changes on field changes, stale finding, missing verdict refusal, same-scope order proof and backwards/self/cross-review/missing/late-edge refusals. |
| Modes, source admission and exact partitions | All three modes, immutable reviewer successors, copy mismatch, wrong run, incomplete/missing/duplicate/digest/tip/history/scope/count cases; explicit target and multi-target/unassigned split. |
| Local draft acceptance and advisory | Real `core::apply` on the existing draft type, exact local correlation/CAS, immutable edit link, independent Publish command, duplicate advisory without blocking, abandoned/accepted terminal refusal. |
| Boundary and boundedness | Core-only build, core/architecture boundary gates, constant-sized state assertions, 16 KiB candidate, 1 MiB source descriptor, 2 MiB selected-private run, 256 source and 16-report/16 KiB metadata limits. |

M3 proves these core decisions. The eighteen whole-feature RFC rows above stay
pending for their storage/protocol/host/UI/native/publication owners. It does
not prove restart durability, process opt-in delivery, actual agent reporting,
zero-mutation host traces, rendered accessibility or a live GitHub round trip.
No independent review is performed here; the r2 M10/BPR-0591 gate is retained.

#### Diagnosis and simplification

The first BPR-0579 build command named a nonexistent
`test_revisioned_review_draft_core` target. It made no build claim; the registered
`test_better_pull_review_revisioned_review_draft_tests` target then built and
passed. Self-review added header `LastOrder` advancement for edits and tested
its replay, output-digest correlation for agent turns, finding-verdict admission,
and backwards draft-link refusal. Adding finding-verdict admission made the
old omitted-verdict copy fixture fail; the fixture now first proves that refusal,
then supplies its real retained verdict and passes. The incidental edit script
that missed its formatted test locator was corrected; no source was discarded.

The code-simplifier pass extracted draft-link validity/successor checks and
bounded run-source admission. New source/test files follow the repository
formatter. Final evidence is attached to the candidate named in the handoff;
earlier failed attempts do not count as acceptance.

#### Documentation and main integration

Changed semantic documentation: domain/state model and this existing ledger;
the live audit records current-main reconciliation at `75102233`. Main's
review-tour work adds three core sources and a test registration; integration
must keep these beside both M3 registrations. Ledger schema remains 6 and
mutation-recovery schema 5; the staged-tour payload codec is now 3. No milestone
plan, original RFC criterion or user amendment changes are warranted by these
additive paths. Main integration remains the coordinator's responsibility.

No current user UI, GitHub/GHES authentication/acting identity/review semantics,
Git cache, credentials, network policy, packaging, native platform or accessibility
behavior is added. New core admission limits are documented; store quotas,
paging, performance measurement, secret-boundary and public workflow qualification
remain later acceptance. Source/test changes are confined to core and its tests.

### M3 verification and candidate checkpoint

| Final check | Result |
|---|---|
| `cmake --preset core-only` | PASS; Qt-free graph, AppleClang 21, macOS arm64. |
| Focused build of `test_private_review_core`, `test_review_composition_core`, `test_better_pull_review_revisioned_review_draft_tests`, `test_better_pull_review_diff_semantics_core_tests`, `test_better_pull_review_agent_session_core_tests`, `test_better_pull_review_agent_finding_core_tests` | PASS with the configured warnings. |
| `ctest --preset core-only --output-on-failure -R '^(core_private_review\|core_review_composition\|core_revisioned_review_draft\|core_diff_semantics\|core_agent_session\|core_agent_finding\|core_dependency_boundary\|core_boundary_contract\|architecture_boundaries\|architecture_boundary_contract)$'` | PASS, 10/10, 106.81 seconds; each test ran under the registered network-isolation wrapper. Escaped table separators represent ordinary regex alternatives in the command. |
| `clang-format --dry-run --Werror` on all nine touched C++ headers/sources/tests | PASS. |
| `otool -L build/core-only/tests/core/review_composition_core` | Only libc++ and libSystem; no Qt, provider or database library. |
| `python3 scripts/verify_documentation.py --root .` | PASS, DOCUMENTATION_OK. |
| MASK scoped board validation | PASS, 1,208 cards, no errors. |
| M3 checkpoint rebind | PASS; definition, obligations and prerequisite fingerprints unchanged after BPR-0578 completion. |
| `git diff --check` | PASS. |

The integrated pass includes the final high-water, output-digest, finding-verdict
and draft-link corrections. It is one combined core pass, not an application
suite or native release qualification. The existing M2/r2 architecture and all
later obligations remain intact.

### M3 durable handoff

Accepted integrated M3 core candidate:
`1acd8821187e77321862df7e26e1f4013d633b4f`.
BPR-0578 candidate was `5bbb2563`, completed by `3ab8df0b`; BPR-0579 was claimed
at `df069591`. The integrated candidate includes both cards and the later
run-body correlation correction. The finish commit `e6e902083c06f365e56eac7c55c5cd71bd6e1d45` changes
BPR-0579 to Done and records the observation. This following documentation-only
commit records the handoff without changing code.

The [M3 observation](RFC-BPR-0023.m3-observation.json) binds both whole-card
obligations and both milestone exits to that candidate, mandate r2 and segment
`m3-core-r2`. The candidate-bound checkpoint check returned `fresh-context`
for M4 after accepted M1/M2. Its `engineering_acceptance: not-evaluated` means
the helper checked consistency only; the criterion judgments and direct tests
above provide the engineering decision. An initial observation used the wrong
`acceptance` field; the helper refused it without action. The corrected
`acceptance_records` observation passed.

Changed M3 paths: new `private_review.hpp/.cpp`, `review_composition.hpp/.cpp`,
private/composition core tests and shared synthetic fixture; existing
revisioned-draft header/implementation; core and test CMake registration;
domain/state document; live RFC-0023 audit/ledger; M3 checkpoint/observation;
BPR-0578/BPR-0579 card moves and editable LIVE_IMPORT link repairs. No SQLite,
application, agent adapter, Qt, production composition, RFC criterion, retained
pack or authored plan source was edited.

All task-owned configure, build, CTest and helper commands have finished.
No child agent, review server, human session, event cursor, protection lease,
external service operation or pending background writer is handed off. The
acceptance helper's task-owned temporary directory is removed on return; ignored
core-only build/cache outputs remain reusable. The sole writer in this assigned
checkout was `/root/rfc0023_m3`; unrelated worktrees and main's independent tour
work were observed read-only and preserved. Writer ownership is released to
the coordinator after the finish commit. No push, PR or main-branch mutation
was performed.

### M3 integration on current main

The coordinator integrated M3 with current main, preserving the independently
landed review-tour work and both additive core/test CMake registration sets.
Merge commit: `bd9faef5` (`Merge branch 'codex/rfc-bpr-0023'`), on top of the
current-main evolution checkpoint `75102233`.

| Combined-tree check | Result |
|---|---|
| `cmake --preset core-only` | PASS. |
| Build `test_private_review_core` and `test_review_composition_core` | PASS alongside the newer review-tour core. |
| Focused CTest for private review, composition, review-tour update, architecture boundaries and architecture contract | PASS, 5/5 after building the existing review-tour update executable. Its first selection preceded that target build and did not launch; the target was then built and its row rerun successfully. |
| `git diff --cached --check` before integration commit | PASS. |

Start a fresh M4/BPR-0580 implementer from this integrated main candidate;
BPR-0581/M5 is also dependency-eligible after M3 but follows M4 in the adopted
order. Storage must enforce high-water allocation, source existence/latest CAS
and transaction atomicity using these pure proposals. M5 must capture actual
protocol facts and full encoded-input bounds; M6/M7 must prove dispatch,
durability and separate mutation composition. All later human/native/designated
publication obligations and the single final M10 independent review remain
pending at their assigned boundaries. There is no pending M3 acceptance or
review session to interpret.

## M4 — BPR-0580 private ledger storage (mandate r2)

M4 implements the one durable private authority through the public
`make_private_review_store` factory, core-created typed proposals and the new
application store port. The entry course opens a real owner-private SQLite
ledger, creates a thread through core policy, commits it, observes it through
another connection and restarts. The legacy note host facade writes the same
thread/content authority. No application window or remote capability is needed
for this storage course; M6–M8 own their public wiring.

### Schema, migration and allocation

Ledger schema **7** contains canonical STRICT tables for scope/incarnation/high
waters, thread headers and turn tips, immutable content vertices, runs and
source pins, derivation edges, compositions and source sets, the existing shared
draft value, publication joins and the bodyless legacy note map. Exact canonical
validation includes new constraint/index definitions. The migration participates
in the existing single all-version transaction; `local_note` and
`local_note_sequence` no longer own text after success.

Historical v1–v6 fixtures traverse to 7. v5/v6 note tests preserve exact embedded
NUL, Unicode and line-ending bytes, SHA-256 digest and optional attachment with
`legacy_unknown` actor. v6 revision/CAS and a deleted-highest high water of 99
survive, with the next facade edit becoming revision 100. v5 creates the expected
initial revision before the next edit. The fixture helper reconstructs canonical
historical schemas only inside tests; no production downgrade path exists.

A universal positive signed-64-bit high-water frontier supplies new object IDs
and vertex order. Batch allocation, mutable version CAS, references, payloads
and checked counters share `BEGIN IMMEDIATE`. Purge retains high waters and
incarnation; database replacement creates a new incarnation and refuses old
references. Two simultaneous connections racing one frontier have exactly one
winner and no partially allocated object. No production retained-maximum scan
is used for the new identity allocation.

### Whole-card acceptance evidence

| BPR-0580 required state | Direct evidence and practical limit |
|---|---|
| Next canonical STRICT schema | `sqlite_review_ledger_storage`: every canonical origin, schema substitution/refusal, current v7 inventory, application/future identity refusal; `sqlite_private_store_open` retains hardened open/path behavior. |
| Durable high waters | New public-port tests: restart, purge/recreate, two-connection CAS and actual concurrent race, signed maximum exhaustion; legacy note/session courses preserve their independent sequences. |
| Atomic exact legacy migration | Existing v1–v6 migration course plus new v5/v6 exact body/digest/attachment/actor/high-water checks; actual killed v6 migration rolls back schema and note conversion before retry. |
| One note authority | `local_note` tables absent; facade and new port share catalog and immutable revisions; thread expansion advances facade CAS and keeps the original note turn coherent. |
| Bounded pages and provenance | 65-header and 68-revision traversal; stale scope/generation/header/digest/offset refusal; exact 1 MiB assistant in two progressing 512 KiB chunks; query-plan assertion proves scope/thread/order index seeking without transcript scan or sort. Run details survive session pruning/restart; source/finding/receipt/dependency reads are bounded. |
| Existing durable draft linkage | Real core composition/acceptance plus atomic candidate/source/draft commit; restart recovers exact body/target/link without runtime scope. Exact copy preserves a conversation agent actor. Late run/draft terminals preserve admitted/accepted immutable sources after a tip change. No second draft reducer or fan-out owner is introduced. |
| Whole-write quotas | All ten persisted count/byte counter boundaries, finding quota and maximum identity refusal; real 4,096-note/thread facade saturation and real 64 MiB thread-body fill plus one-byte refusal; reviewer 16 KiB+1, 257-record/source, 17-run limits; no partial terminal/body/header or lost frontier on refusal. Large count cases use designated counter fixtures instead of allocating 65,536 objects per kind. |
| Purge/loss/WAL/concurrency/kill | Exact dependency refusal and ordered explicit cleanup; whole scope purge; new incarnation after file loss; simultaneous allocator race; real POSIX SIGKILL before and after thread, composition/draft and publication COMMIT; killed migration rollback/retry; existing WAL and wrong-app/future refusal course. |

The real 64 MiB body fixture measured 67,473,408 bytes in the main database
(including schema/index/metadata overhead at observation time). This is a local
fixture measurement, not a production performance claim or a worst-case bound
for all retained kinds. Counts, row sizes and both aggregate byte budgets provide
finite storage bounds; M9 still owns broader public-path performance evidence.

Database-loss evidence independently rereads an unchanged neighboring synthetic
shared-authority file and the adapter exposes no GitHub capability. It proves
local isolation only. Actual GitHub unchanged-state readback and remote-success/
local-repair crash joins remain the designated-target or signed-recording work
of M7/M9. POSIX kill evidence ran on macOS arm64; the Windows native equivalent
is not claimed. The original eighteen feature rows remain pending at their
assigned whole-feature owners, including native, accessibility and publication
acceptance. The final independent review remains M10/BPR-0591 under r2.

### Diagnosis, refinement and documentation impact

The earliest smoke refused purge because existing connection hardening sets
trigger depth to zero; foreign-key cascading would violate that policy. New
private deletion now follows explicit dependency order within one transaction.
The first exact 1 MiB assistant write hit SQLite's existing 1 MiB row ceiling;
the ledger alone now admits 2 MiB SQLite rows while schema body quotas retain
the specified 1 MiB limit. These concrete refinements are recorded in the live
audit and architecture document; no milestone exit or RFC criterion was weakened.

Self-review strengthened revision/dependency indexes, snapshot consistency,
legacy-facade CAS after thread expansion, run terminal retention after source
edits, exact-copy actor preservation and unchanged linked-draft terminal
persistence. Focused regression cases pass for each changed path. Initial
compile/fixture attempts (unfinished source, void-result wrapper, unused local,
omitted finding anchor) were corrected before acceptance. No failed attempt is
used as acceptance evidence.

Changed product paths: new application/private-store contract; new SQLite public
factory, canonical private schema, bounded binary codec and implementation;
existing ledger migration and facade implementation/header; one core retained-
finding validity helper; SQLite build registration. Tests add the public private
store course and historical-schema fixture, adjust existing ledger fixtures and
register the new course. Documentation changes are this ledger, the live audit,
review-ledger architecture, checkpoint/observation and the card/link transitions.
The imported source pack and all eighteen original criteria are unchanged.

| Documentation concern | M4 impact |
|---|---|
| GitHub/GHES, acting identity, review semantics | No transport or mutation added. Publication rows are local receipt joins, never authorization. Later hosts must use the existing mutation authority. |
| Cache, private data and credentials | v7 retains explicit reviewer/assistant/finding/candidate/draft content and bounded provenance; no source-file duplication, credentials, argv, environment or runtime grants. Owner-private path and WAL policy are retained. |
| Limits and performance | Fixed keyset/body pages, count/byte quotas, universal frontier, explicit reference cleanup and ledger-only 2 MiB row ceiling are documented. |
| Packaging and platforms | No package or release policy changes; portable core/SQLite code compiled and exercised on macOS arm64 only. |
| Qt, accessibility and visible behavior | Existing note facade remains usable; new transcript/workshop UI and host persistence/fan-out remain later milestones. No rendered or native acceptance is claimed. |

### M4 verification

The combined core-only build targets are `test_private_review_storage`,
`test_better_pull_review_sqlite_review_ledger_tests`, `test_private_review_core`,
`test_review_composition_core`, `test_review_tour_storage`,
`test_better_pull_review_review_mutation_recovery_storage_tests`,
`test_better_pull_review_review_submission_recovery_storage_tests` and
`test_private_store_open`. The selected CTest names are
`sqlite_private_review_storage`, `sqlite_review_ledger_storage`,
`sqlite_review_tour_storage`, `sqlite_review_mutation_recovery_storage`,
`sqlite_review_submission_recovery_storage`, `sqlite_private_store_open`,
`core_private_review`, `core_review_composition`, `architecture_boundaries` and
`architecture_boundary_contract`. Tests use the registered network-isolation
wrapper. Recovery schema 5 and staged-tour codec 3 production code is unchanged.

| Final command/check | Result |
|---|---|
| `cmake --build --preset core-only --target` with the eight targets above, `-j 6` | PASS with configured warnings. |
| `ctest --preset core-only -R` selecting the ten names above, `--output-on-failure` | PASS, 10/10, 122.28 seconds. |
| Final relink and CTest of the eight core/storage names after the exhaustion-terminal and existing GCC nested-value refinements | PASS, 8/8, 18.43 seconds; unchanged architecture results reused. |
| `/opt/homebrew/opt/llvm/bin/clang-format --dry-run --Werror` on all twelve touched C++ files | PASS. |
| `python3 scripts/verify_documentation.py --root .` | PASS, DOCUMENTATION_OK. |
| `python3 tests/scripts/verify_documentation_tests.py` | PASS, 64 tests; printed rejection diagnostics are expected negative fixtures. |
| MASK board validation `--rfc RFC-BPR-0023 --json` | PASS, 1,208 cards, no errors. |
| M4 checkpoint rebind | PASS; unchanged definition, whole-card and prerequisite fingerprints. |
| `git diff --check`; compare authored plan and live RFC to baseline | PASS; no plan/RFC criterion source changes. |

Final code includes the preservation of already allocated terminal updates at
identity exhaustion; new allocation still refuses without wrapping. The existing
GCC aggregate-construction workaround is retained in the note facade. No command
requires network access or contacts a real review target.

Final candidate and ownership are recorded in the durable handoff below.

### M4 durable handoff

Accepted M4/BPR-0580 code and evidence candidate:
`25fd32efd8ca316ca3473f9a6a928265eb0d6000`.
Baseline integrated main was `2a254e638d364d249e79148db82b13d0fceccca5`;
BPR-0580 was readied at `8f8f4ca7` and claimed at
`1442faf60e8fe26a8c8f3ebeb00250b8c284b6ae`.
Finish commit `18964711caf463376ee311d406990f193c9280e4` moves the whole card
to Done, records all eight required final states and adds the candidate-bound
[M4 observation](RFC-BPR-0023.m4-observation.json). This following documentation
commit records the final handoff without changing product code.

The checkpoint check for segment `m4-storage-r2`, mandate r2, accepted M1–M3
and candidate `25fd32ef` returned `fresh-context` for M5 before bookkeeping.
The post-finish recheck returned `reconcile` because the current commit had
advanced past that exact candidate; `git diff --exit-code 25fd32ef HEAD -- src tests`
confirmed no product/test change. This is the expected documentation/board
handoff distinction, not a changed engineering candidate. Its
`engineering_acceptance: not-evaluated` is a consistency result; the whole-card
judgment rests on the direct tests and limitations above. M4 has no required
pending acceptance. Preserve ledger schema 7, mutation-recovery schema 5 and
staged-tour codec 3, including any independent main work.

All task-owned build, CTest, formatter, documentation, board and checkpoint
commands have completed. No background writer, child agent, review server,
human session, event cursor or protected candidate lease is handed off. The
acceptance helper's temporary directory was deleted on return. Ignored build
outputs remain reusable; test scratch roots and crash children were reaped by
the courses. `/root/rfc0023_m4` was the only writer in this assigned checkout;
writer ownership is released to the coordinator with this handoff. No main
mutation, push, PR or live GitHub/GHES operation was performed.

### M4 integration on current main

The coordinator integrated the accepted branch without conflicts on current
main. Merge commit: `54060ba4` (`Merge branch 'codex/rfc-bpr-0023'`), on top of
M3 integration commit `2a254e63`. The M4 candidate was already built and tested
against that exact code baseline; the integration merge changed ancestry only.
The ten-test and final eight-test M4 passes above therefore apply unchanged to
the integrated tree.

Start a fresh M5/BPR-0581 implementer from this integrated main tip. Preserve
ledger v7, mutation-recovery v5 and staged-tour codec v3. M5 covers the
protocol facts/input course; later milestones cover host lifecycle/dispatch,
draft fan-out, production Qt wiring, native/accessibility and designated-
publication acceptance, followed by the one final independent review at
M10/BPR-0591. Windows process-kill parity and actual unchanged-GitHub readback
are not inferred from macOS storage fixtures. None is silently marked complete
by M4, and the eighteen-row feature matrix is unchanged.

## M5 runtime provenance and input admission

Segment `m5-protocol-r2`, mandate r2, owns BPR-0581 as one whole card. Baseline
is integrated main `fa2eefe9`; readiness commit `a51bd6fd` and claim commit
`8d69e75eac9803fc6e392f78bffbcbd8731043e8` precede implementation. The
[M5 checkpoint](RFC-BPR-0023.m5-checkpoint.json) binds the adopted obligation
and both exits. No original criterion, milestone exit, dependency or protected
source-pack byte changes. Ledger 7, mutation recovery 5 and staged-tour codec 3
remain intact. Independent review remains assigned to M10/BPR-0591 under r2.

### Whole-card evidence and limits

| BPR-0581 required state | M5 result and direct evidence |
|---|---|
| Bounded optional requested settings without profile-name inference | PASS at the existing invocation entry point: `application_agent_invocation_host` observes the explicit settings on the one dispatched request. The launch course separates explicit profile defaults and request overrides; JSONL, Claude and terminal requested/default settings refuse before any process call. |
| One dispatch identity and immutable profile/protocol/binding/entry-point/source snapshot | PASS for the allocated M5 contract: `agent_launch_options::AdmittedRun` and ordered full bodies are validated; two runs on one session remain distinct. Missing/tampered/unadmitted bodies, changed settings/entry point and snapshot mismatches refuse. Request, stream and result preserve run identity. M6 owns store-before-dispatch production wiring, as detailed below. |
| Protocol-reported model/effort/version/remote IDs only when reported | PASS at each implemented protocol field: [capability matrix](../architecture/ai-agent-boundary.md#tested-protocol-capability-boundary), decoder and real child canary. App Server provides thread/turn facts and thread settings; it supplies no invented version/session/run. Claude init and assistant model retain separate bases. |
| Unsupported/external facts remain unknown | PASS: JSONL retains only its reported remote thread; external text that claims a model/effort/version yields no report; absent optional fields stay absent. All content-self-report fields remain absent. |
| Requested and reported disagreement persists as facts and typed conflict | PASS: launch/protocol-loop/canary courses inspect both facts and conflict values. The SQLite course decodes, stores, reopens and compares the exact metadata and three derived conflicts. |
| No prose/executable/profile scraping or provider credential capture | PASS: only named protocol fields are admitted. Real child canary compares exact allowlisted environment/argv/stdin while fake credential values exist in the parent. SQLite scans every file in its owned root, including any WAL, for ignored credential-like envelope canaries and finds none. |
| Full/partial/conflicting/malformed/oversized/stale/cancelled/unknown fixtures | PASS: launch, loop, finding-protocol, storage and child courses cover each class; remote-ID mismatch/oversize cannot release App Server context, and foreign run/session results cannot attach facts to another run. Same-run staleness/cancellation retain previously admitted facts. |

The per-field runtime limits remain 256 bytes, remote IDs 1,024 bytes, 16
reports and 16 KiB aggregate metadata. Input courses exercise 2 MiB raw selected
bodies and one byte past, exact 8 MiB direct wire admission and one byte past,
JSON expansion beyond the aggregate budget, fixed wrapper/handshake costs and
late remote-ID overhead. Full source text is digest checked and transmitted
without truncation. The pre-encoding counter avoids allocating the expanded
context before admission; the real process adapter refuses before child start;
the shared loop counts every actual stdin write. The child side-effect file
stays absent for refused input and exists for the small-input negative control
using the same child. Invalid launch facts are refused without copying them
into returned metadata.

The production host currently accepts explicit requested settings, but legacy
callers still have no `AdmittedRun` and retain zero run. That compatibility path
cannot send `PrivateSources`. The adopted M5 exit is the identity **contract**;
M6/BPR-0582 must allocate and persist each run/prompt before process dispatch,
load/revalidate exact selected sources, pass the snapshot, retain complete
assistant text, finalize against the exact expected run and commit terminal
facts. This record does not mark AC-02/06/16 or the complete public UI flow
accepted. The eighteen-row canonical matrix remains unchanged for integrated
feature acceptance.

### Changed surfaces and operator behavior

- `src/core/include/bpr/core/agent_profile.hpp` carries explicit profile default
  settings; `review_composition.hpp` reuses the setting type and gives the
  immutable provenance value exact comparison. No core effect dependency is
  introduced.
- `src/application/include/bpr/application/agent_launch.hpp` and
  `src/application/src/agent_launch.cpp` own validated snapshot input, field-level
  report extraction, total encoded input admission and result correlation.
  `agent_invocation_host.hpp/.cpp` forwards requested settings through the
  existing launch path.
- `src/adapters/agent/src/agent_protocol_loop.cpp`, `agent_process_launcher.cpp`
  and `windows_agent_process.cpp` retain run/facts through the one process path,
  independently refuse oversized input and preserve existing lifecycle and
  credential isolation. Windows-specific fallback returns carry the metadata;
  Windows native execution was not performed here.
- Focused application launch/loop/finding/invocation-host tests, the child
  fixture and `tests/adapters_qt/agent_launch_canary_tests.cpp` exercise the wire
  and process boundary. `tests/sqlite_contract/private_review_storage_tests.cpp`
  links the application decoder for the restart/secret-scan course; its CMake
  target adds that dependency. No store codec or schema is edited.
- [Agent boundary](../architecture/ai-agent-boundary.md) and
  [live audit](RFC-BPR-0023-live-audit.md#m5-protocol-and-input-reconciliation)
  describe actual capability limits. No UI setting editor or provenance
  presentation is claimed by this milestone.

The canaries are sanitized local children on macOS with Qt 6.11.2, not installed
Codex/Claude providers. They prove transport handling and existing process
isolation on this host. They do not establish Windows/Linux native parity,
model catalog support, vendor execution attestation, native accessibility,
complete installed UI reachability or live GitHub body/identity semantics.
Those outstanding acceptance owners remain M6–M10. No real provider or GitHub
mutation was performed.

### Verification and self-review

The shortest public composition was the existing invocation host into its
process port, followed by the real process adapter into the hostile child.
The metadata persistence course then traversed decoder → public private-store
transaction → close/reopen → exact run load. Secret scanning uses only its
owned temporary files. Scratch roots for touched process courses are now
owner-private `QTemporaryDir` roots, with files inside and cleanup by the owner.

Early development runs exposed stale fakes that put `DeferredContext` on
non-App-Server requests, an incomplete App Server turn fixture, and C++ fixture
syntax/enum errors. The fakes now match the production protocol contract;
subsequent targeted runs passed. Invalid UTF-8 can be refused by the JSON parser
before metadata validation, and the fixture accepts either explicit failure
cause. These were development failures, not waived acceptance.

The implementer's own focused review checked exact field bases, aggregate
escaping, request/result correlation, cancellation facts, raw-frame exclusion
and pre-start refusal. It removed an unused unconditional JSON-escape allocation
and delayed copying metadata until input admission succeeds. This is engineering
self-review; the required independent review remains at M10.

| Command / check | Actual result |
|---|---|
| `cmake --preset dev` | PASS; pinned Qt 6.11.2 and dependencies configured. Existing dependency CMake uninitialized-variable warnings did not fail configuration. |
| `cmake --build --preset core-only --target test_better_pull_review_agent_launch_tests test_better_pull_review_agent_protocol_loop_tests test_better_pull_review_agent_finding_protocol_tests test_better_pull_review_agent_invocation_host_tests test_private_review_storage -j 6` | PASS on final code. |
| `ctest --preset core-only --output-on-failure -R '^(sqlite_private_review_storage\|application_agent_invocation_host\|application_agent_launch\|application_agent_protocol_loop\|application_agent_finding_protocol\|core_review_composition)$'` | PASS 6/6, 15.00 seconds, final code. |
| `cmake --build --preset dev --target test_better_pull_review_qt_agent_launch_canary_tests test_better_pull_review_agent_finding_protocol_tests -j 6` | PASS on final code. |
| `ctest --preset dev --output-on-failure -R '^(qt_agent_launch_canary\|application_agent_finding_protocol)$'` | PASS 2/2, 90.73 seconds, final code; child canary 88.32 seconds. |
| `ctest --preset core-only --output-on-failure -R '^(application_agent_launch\|application_agent_protocol_loop\|application_agent_finding_protocol\|application_agent_invocation_host\|core_review_composition\|architecture_boundaries\|architecture_boundary_contract)$'` | PASS 7/7, 112.48 seconds before the final narrow metadata-copy/counter cleanup and new storage/host fixtures. Changed behavior was rerun above. The unchanged boundary-validator self-test is reused. |
| `ctest --preset core-only --output-on-failure -R '^architecture_boundaries$'` | PASS 1/1, 7.07 seconds against final source and CMake graph. |
| `clang-format --dry-run --Werror` on every changed C++ path; `git diff --check` | PASS. |
| `python3 scripts/verify_documentation.py --root .` | PASS, `DOCUMENTATION_OK`. |
| `python3 tests/scripts/verify_documentation_tests.py` | PASS 64 tests; printed ERROR diagnostics belong to intentional invalid-document fixtures. |
| `issue_board.py ... validate --rfc RFC-BPR-0023 --json` | PASS, 1,208 cards, no errors. |
| Compare retained source-pack/plan/live RFC to `fa2eefe9` | PASS. The editable LIVE_IMPORT file has only the board helper's BPR-0581 lane-link repair. |

No application-wide suite or installed-app test was needed for the bounded
protocol milestone. Tests use the repository network-isolation wrapper and
owned local fixtures. The immutable source comparison deliberately excludes
the editable LIVE_IMPORT lane links. Acceptance evidence is attached through
the finish record, preserving the bound card body and obligation fingerprint.
