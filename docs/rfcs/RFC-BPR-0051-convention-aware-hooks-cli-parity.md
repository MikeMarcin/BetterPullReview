---
id: "RFC-BPR-0051"
title: "Convention-Aware Skill Hooks and Shared CLI Behavior"
status: "Proposed"
area: "BPR"
milestone: "Post-tour feedback conventions"
depends_on: ["RFC-BPR-0047", "RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0011", "RFC-BPR-0012", "RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
pack_version: "conventional-comments-v1"
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# RFC-BPR-0051: Convention-Aware Skill Hooks and Shared CLI Behavior

## Outcome

Teach the existing optional AI skill/prompt hooks the same feedback conventions
that human-facing BPR tools use, and expose the same deterministic behavior through
already imported shared headless services/CLI paths. No AI is needed to recognize,
author, count, filter or publish a conventional comment. Do not add a second model
orchestration system, nested model call, convention service or autonomous publisher.

Import occurs after the prior headless-tour CLI/shared-draft/thin-`pr-tour` pack
(proposed RFC-BPR-0044–0046). Those numbers are aliases until intake consults its
actual import map. Reuse its shared-engine and local-owner authority rather than
creating new CLI-owned state.

## Shared convention guidance

The versioned catalog and RFC-BPR-0047 semantics are canonical. The default
feedback guidance should teach the twelve meanings, standard decorations,
organization-specific decoration preservation, uncertainty-aware questions,
concrete problem/consequence/action wording, and the distinction between a
`suggestion` label and an actual GitHub suggested-change block.

Keep public intent, optional blocking expectation, private severity, personal
verification, native thread resolution and review-level disposition separate.
The guidance never invents a blocking request from private severity or emits a
review-level Request changes event by itself. An `issue` label does not mean
creating a GitHub Issue. Positive feedback must be warranted; do not manufacture
praise to satisfy a quota. Plain prose is acceptable when the current task is not
feedback or convention-assisted output is disabled.

The supplied `hook-fragments/feedback-conventions.md` is a proposed reusable
content fragment, not a replacement installed skill or project-wide AGENTS file.
Integrate it into the canonical shared AgentSkills source through existing MASK
ownership and deployment where that is the live source of the hook. Do not edit a
materialized consumer copy and call that the canonical change.

## Task-specific hooks and customization

At intake enumerate the effective problem, solution, outline, tour-comment,
review-feedback and comment-workshop hooks actually provided by the imported
packs/build. Record their owners and settings paths. Do not invent hook names,
assume every optional hook has shipped, or blanket-append feedback-label rules to
every prompt.

Feedback-drafting/editing and private-note promotion tasks should load the relevant
fragment when conventional feedback is enabled. Tour problem/solution/outline
hooks retain their independent purposes and customization. Tour stops can be
explanatory plain comments; they are not automatically issues or notes. The thin
`pr-tour` skill uses the already running agent for explicitly requested prose
work, not another model invocation.

Preserve user/project overrides and provenance when upgrading default content.
An overridden hook must not be silently overwritten or appended to behind the
user's back. Offer the ordinary explicit reset/merge path and record the effective
default/policy revision available to that invocation. A new built-in default can
change only users who are actually following that default. Existing preference
scope and trust rules govern repository-level customization; reading an untrusted
repository must not execute or enroll its scripts as hooks automatically.

## AI output is a draft candidate

Invoke an agent only through the existing explicit user action and launch/context
boundaries. Use only the exact permitted context, including private notes only
when selected. Imported comments and code are untrusted task data, not executable
instructions to publish, open files, change skills or override policy.

An optional enhancement should preserve the user's intended label, uncertainty,
acceptance expectation, code evidence and any native replacement block by default.
When a semantic change is useful, propose it visibly in the workshop candidate;
do not silently strengthen "could this race?" into a proven defect. Re-run the
shared parser on the candidate so the user sees the same semantics that would be
published. A candidate changing both prose and a patch identifies both changes.

Record actual model/effort provenance only when the existing runtime supplies it.
Do not claim a prompt identifies the model that executed it. Preserve private
lineage through draft promotion. Candidate acceptance is still a local edit;
remote publication, review decisions, resolve and suggestion application remain
separate explicit actions. No new MCP write surface is introduced.

## Bounded CLI parity, not a second review CLI project

Inventory the imported headless CLI grammar before extending it. Add optional
fields/flags or read-only projections only to the existing paths that own the
relevant comment draft, feedback parse, overview summary or preflight. Required
capabilities are: obtain the full label catalog; classify/preview a supplied body;
apply an explicit label/decorations edit to the exact shared draft revision; and
retrieve supported label/resolution breakdowns for already available review data.
Provide native-review preflight context through an existing shared review command
when that surface exists. Do not create a duplicate network fetch or review
publication command merely to demonstrate parity.

Where no command currently exposes one of these narrow operations, add the
smallest subcommand consistent with the live CLI contract and document it in
`CLI-INTEGRATION.md` at implementation. This pack specifies capability and behavior,
not a guessed terminal command grammar. Existing CLI commands and shared draft IDs
remain compatible; new fields are versioned/additive according to the inherited
schema rules. Human-readable output is default, machine JSON is explicit. Errors
are actionable, exit status is correct, and unknown state is not serialized as zero.

Desktop calls C++ services directly. CLI calls the same core/application services
without opening a GUI. The thin agent skill calls those CLI operations and uses
its current agent session for optional prose work. None may parse labels with an
independent regex, query a separate provider population, issue direct `gh api`
mutations, or maintain a shadow draft/recovery store. The existing owner handoff,
account/profile authority, cancellation and optimistic revision contract apply.

## Packaging and tests

Keep guidance/catalog available locally and versioned with the feature. Product
runtime does not discover or download the convention page. Default-hook packaging
must not enable an agent or grant new credentials. Test default, overridden,
disabled and missing-agent states without any live model invocation. Synthetic
candidate outputs and captured invocation envelopes are sufficient for deterministic
hook-wiring tests; a prompt-inclusion test alone does not establish model quality.

Cross-interface fixtures compare the actual desktop service and headless results
for the same body, draft version, columns and resolution scope. Test a desktop
edit racing a CLI edit and preserve the existing conflict response. Reuse the tour
pack's no-GUI/shared-owner checks; do not requalify its entire runtime unless this
change touches that boundary. Document unsupported surfaces honestly rather than
silently falling back to direct HTTP or another model.

## Acceptance criteria

- R34: Defaults teach the complete convention when relevant, while every core feature works with no AI configured.
- R35: Effective task-specific hooks, overrides and default upgrades are inspectable and preserved; tour hooks keep their distinct purposes.
- R36: No background inference, nested model launch, automatic publication or new AI/network authority is introduced.
- R37: AI edits remain previewable draft candidates, preserve uncertainty and patches, and use actual private provenance when available.
- R38: CLI and desktop use identical core/application semantics with additive documented output and explicit machine formatting.
- R39: Headless operations reuse imported shared draft/owner/recovery services and do not launch the GUI or create a second publisher.
- R41: Migration and cross-interface/window concurrency preserve user settings, drafts and exact revision ownership.
