---
name: implement-rfc
description: Implement one or more RFCs end to end for the project described in config/project.json. Use when the user asks to implement or resume an RFC or RFC pack, break it into bounded issue slices, keep affected documentation current, run acceptance-focused verification, commit each completed slice, and reconcile RFC and issue-board status.
---

# Implement RFC

Carry requested RFCs from the live RFC and issue-board state to a truthful
closeout. Verification proves the named acceptance boundary; it does not
become a second deliverable.

This body is portable and identical across projects. Everything
project-specific lives in two files: `config/project.json` (machine-read
facts: name tokens, tracker paths, lane names, the structure gate, the
attended-review skill) and [references/project.md](references/project.md)
(the project's own rules: what to read first, the side-effect boundary, what
counts as a direct surface, the closeout-card contract). Read `project.md` at
Start; every `project.json` key named below means the value it holds.

## Finish Condition

Work is complete when:

1. every in-scope acceptance criterion has direct passing evidence;
2. the changed public or selected path has been exercised when applicable;
3. no known in-scope critical defect remains; and
4. code, tests, documentation, RFC status, and issue-board status describe the
   same bounded truth.

Residual uncertainty, unrequested configuration coverage, and possible future
improvements do not block completion. Record them as limitations or follow-up
work.

## Start And Reorientation

1. Confirm from live Git state that this checkout is the project
   `project.json` names and note the current branch. Read
   [references/remote-routing.md](references/remote-routing.md) only when
   `.remote-station.json` exists, a `remote-task` envelope arrived, or this
   session will dispatch work to a station. Otherwise follow the local rules
   in this skill and [references/pack-lifecycle.md](references/pack-lifecycle.md).
2. Inspect `git status --short`; preserve unrelated work and stage explicit
   paths only. Concurrent sessions may share a checkout: claim the current
   card by committing its move to the in-progress lane before editing, leave
   nothing staged between commits, and commit with explicit pathspecs.
3. Recover or record the RFC's immutable `RFC_BASE_SHA` in its closeout card.
   Conversation memory is not a durable home. For work that predates a
   closeout card, use the first parent of the earliest in-scope commit.
4. Read `references/project.md` and the repository instructions it names
   (root and nested `AGENTS.md`, read-first documents) before editing source.
   Open a deeper reference only when the change intersects a hazard it names.
5. Read the requested live RFCs, the tracker READMEs under `rfc_root` and
   `issue_root`, the relevant cards, their dependencies, and the next-ID
   source. Live files outrank stale paths or remembered state.
6. If the user supplied an RFC-pack folder, import it with the procedure in
   [references/pack-lifecycle.md](references/pack-lifecycle.md) before
   planning; a copied issue snapshot never overrides the live board.
7. Build a short working plan from acceptance criteria and dependency-ready
   issue slices. If the RFC has no closeout card, create one in the planning
   slice — objective, `RFC_BASE_SHA`, the requested RFC set and this RFC's
   position, planned card IDs — and commit the planning slice before
   implementation. Create other cards only when implementation genuinely
   needs them, allocating IDs with `allocate_id_command`.

Reorient from the live tree at the top of every task iteration and after
interruption, compaction, conflict resolution, or a material external change:

- current RFC: the first requested RFC not at `terminal_rfc_status` in the
  RFC index;
- closeout card: find it in any lane and read back `RFC_BASE_SHA`, the
  planned cards, and the verification record;
- queue: the planned cards plus every card whose `rfc_link_field` names the
  current RFC — select by linkage, never by lane listing;
- next task: the queue card already in the in-progress lane, else the
  lowest-ID queue card outside the done lane whose dependencies are done and
  whose lane is startable here — the ready lane always, a platform-ready lane
  only when it matches this host or station platform. A card ready only on
  another platform is deferred, not skipped silently; a blocked card's
  blocker is surfaced;
- cross-check `git log --oneline "$RFC_BASE_SHA"..HEAD` against the queue:
  commits without a done card mean an interrupted task, and a done card
  without commits needs reconciliation;
- list live delegated children from the runtime (native agents, plus
  `scripts/codex_lane.py status` for Codex lanes); a compaction summary is
  not live status.

Do not restart completed verification whose inputs are unchanged. Context
pressure is not a stop condition: hand the next slice to a bounded
`rfc-worker`, or end the turn as an explicit resume handoff with the closeout
card current and the next task named.

## Pack Lifecycle

Pack import, lane setup and commit authority, mid-work ID discovery,
milestone integration, renumbering, handoff, and worktree cleanup follow
[references/pack-lifecycle.md](references/pack-lifecycle.md). Import,
integration, renumbering, and every next-ID or reservation write belong to
the project integrator on `main`.

## Implement Bounded Slices

For each dependency-ready card:

1. Confirm ownership, non-goals, applicable instructions, acceptance criteria,
   and the narrowest direct verification command before editing.
2. Move the card through the board's normal lifecycle and keep its
   front-matter status equal to its lane.
3. Implement the smallest coherent change, keeping each concern in the layer
   `references/project.md` assigns it.
4. Add focused tests for changed behavior. Update an instruction or reference
   only when the change makes a purpose, ownership, hazard, or verification
   rule it states untrue.
5. Run the selected focused checks, record commands and outcomes concisely in
   the card or closeout record, review the diff, and commit the slice with
   explicit pathspecs.
6. Move the card to done only when its own acceptance criteria are satisfied.
   A separable noncritical improvement becomes follow-up work rather than an
   expanded current card.

When a slice is delegated to `rfc-worker` or `rfc-worker-routine`, give it the card, the relevant
acceptance criteria, the applicable instructions, an explicit exclusive list
of allowed paths, explicit non-goals, and the focused verification. Do not
edit while it runs. Afterwards inspect the complete diff, reject out-of-scope
changes, reproduce the focused verification, and keep board moves, staging,
and commits with the integrator.

Run the repository's code-simplifier pass over recently changed code after the
task set, then rerun only checks affected by simplification.

## Select Verification By Acceptance

Choose the narrowest commands that directly exercise the changed behavior and
the RFC's stated acceptance criteria.

- Run focused changed-surface tests.
- Add one public or selected-path lane when focused tests do not exercise it;
  `references/project.md` names what counts as a direct surface.
- Let one command prove several related claims.
- Treat verification tables and preset catalogs as catalogs of available
  lanes, not matrices. Select only lanes relevant to the changed surface.
- Reuse a passing result when its source, configuration, fixture, and
  dependency inputs did not change.
- Report unavailable or omitted lanes honestly without promoting them into
  requirements.

Full project or repository validation, repeated clean builds, package
matrices, broad soaks, clean-room replay, and hermetic evidence construction
require an explicit user request. Do not infer them from ordinary
implementation, closeout, release proximity, residual uncertainty, or the
existence of a validator.

Verification must not create new requirements. Do not build proof-of-proof
artifacts, hash-bound result packages, evidence manifests, replay trees, or
process audits unless the task explicitly names them as product acceptance.
A source checkout is never retained as evidence.

An attended human verdict is never a stop condition and never blocks a card's
unattended work: finish the unattended evidence, park the card as
`references/project.md` describes, and continue with work that does not
depend on it. Present pending verdicts together at the run boundary.

## Findings And Convergence

Ask of every recorded check: could it pass while the claim is false? If yes,
strengthen the check or narrow the claim. Then classify each finding by
relevance and expected cost:

- Correct it and narrowly rerun the affected check when it falsifies an
  existing acceptance criterion, the repair is contained, and the check is
  clear.
- Record or create follow-up work when it is separable, noncritical, or an
  improvement beyond the accepted boundary.
- Ask the user when it changes scope or architecture, materially enlarges the
  task, requires broad verification, or has an uncertain risk/benefit tradeoff.

A correction never restarts whole-candidate review. Recheck only the changed
surface; unchanged passing lanes remain valid.

## Optional Isolation And Independent Review

Isolation and independent review are tools, not universal phases. Consider
them when they are likely to add discriminating evidence and the changed
surface materially involves one of these trigger classes:

- wire or protocol contracts;
- persistence or migration behavior;
- authority or security boundaries;
- concurrency primitives;
- destructive migrations.

They are also appropriate when the user explicitly requests them. Use at most
the isolation needed for the selected check, and remove task-created
worktrees/build roots as soon as their result is consumed.

When review is selected, use one reviewer and prefer the opposite model family
from the implementation where the driver can launch it. Use the critical role
instead of the ordinary role for a named material risk. A second review requires
an unresolved material concern or an explicit request, including when the same
role runs on another provider. If the other family is unavailable, use the native
reviewer and record that limitation. Read
[references/codex-lanes.md](references/codex-lanes.md) before launching any Codex
lane, and record each review's runtime on the closeout card as
`Closeout review runtime: <runtime> (<model>, <effort>)`.

A reviewer may block closeout only with an evidence-backed failure of an
existing acceptance criterion or a critical regression in the changed surface.
Unsupported suspicions, broader hardening ideas, and new requirements are
non-blocking. Read
[references/delegated-execution.md](references/delegated-execution.md) when
delegation, a test runner, or an independent reviewer will materially help.

## Documentation

Ordinary code work requires no documentation review or ledger. When a change
makes a retained rule untrue in an instruction file or reference, apply the
smallest source-backed update and move on. Before closeout run every
`structure_gate` command; a failure blocks the status change until fixed.
Documentation never invalidates otherwise unchanged product evidence.

## Status And Closeout

Use the RFC status vocabulary `references/project.md` lists; `Proposed` means
design not yet approved, `terminal_rfc_status` means the finish condition
above is satisfied, and any intermediate or rejection statuses mean what the
project defines.

Before the terminal status, confirm the final diff against `RFC_BASE_SHA`,
direct acceptance evidence, issue-card state (every required card done, no
required work left in an active lane, including unlinked cards created since
`RFC_BASE_SHA` whose scope belongs to this RFC), RFC/index agreement, and a
passing `structure_gate`. Do not create new verification work merely to make
a record look more complete. Unrelated pre-existing findings remain separate
work.

The closeout card is the durable run anchor. Keep it in the project's
closeout-card contract (`references/project.md`): `RFC_BASE_SHA`, the planned
cards, and for each acceptance criterion the check run, its result, and the
commit it was produced at; add the `Closeout review runtime:` line when an
independent review ran. On a station branch the status flip is a proposal the
project integrator confirms on `main`.

Final reporting names completed cards/commits, exact verification run,
omitted or unavailable lanes, documentation updated, temporary resources
removed, and remaining follow-ups or decisions.

## Temporary Resources

Ordinary RFC work creates no separate test-runner worktree. If this task creates a
temporary worktree, build root, log root, or generated review root:

1. record its exact path and consumer;
2. keep it only while that consumer is active;
3. remove it promptly after the result is consumed, after confirming no
   untracked user files or live process depend on it; and
4. compare the final worktree/resource state with the starting state.

Never retain a temporary checkout as proof. Handoff- and closeout-time
worktree cleanup follows
[references/pack-lifecycle.md](references/pack-lifecycle.md).

## Portable Runtime

The canonical skill and role semantics are vendor-neutral. Generated Codex and
Claude adapters come from `config/roles.json`, `config/routing.json`, and the
per-role addenda in `config/project.json`. Read
[references/runtime-compatibility.md](references/runtime-compatibility.md)
only when using those adapters or remote runtime features. Regenerate and
drift-check every adapter whenever `SKILL.md`, a config source, a reference,
a script, or a test changes: `generated/portable-manifest.json` records the
portable set, and `scripts/validate_portable_agents.py --compare` proves two
projects carry the same skill.
