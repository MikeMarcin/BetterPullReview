# RFC-BPR-0023 Verification Plan

Status: **implementation authorized under mandate r2; M1 complete; M2 architecture accepted under amended review timing; final independent review and feature criteria pending**

Import baseline: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

Immutable implementation RFC_BASE_SHA: `899090f39304dca5a2c0b034c4684a50ca73605a`.

Owner mandate r1 (2026-09-26) authorizes implementation, live-plan revision and
coordinator integration of completed milestones to main. This supersedes the
import-only deferral; it does not satisfy acceptance. Owner amendment r2
(2026-09-26) directs implementation to continue and moves M2's independent
architecture review into one capability-critical review of the final integrated
candidate at M10/BPR-0591. That final review is authorized and required before
RFC closeout; it has not been supplied. The current r2 M2 completion appears at
the end of this ledger. Earlier r1 handoffs remain historical observations. The
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
