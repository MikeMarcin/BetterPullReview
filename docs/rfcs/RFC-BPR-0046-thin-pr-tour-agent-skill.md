---
id: "RFC-BPR-0046"
title: "Thin pr-tour Agent Skill"
status: "Proposed"
area: "BPR"
milestone: "Tour CLI and pr-tour skill"
depends_on: ["RFC-BPR-0043", "RFC-BPR-0044", "RFC-BPR-0045"]
pack_version: "pr-tour-cli-extension-v1"
metadata: {"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# RFC-BPR-0046: Thin pr-tour Agent Skill

## Decision

Provide a portable Agent Skill named exactly **`pr-tour`**, a thin conversational wrapper over `bpr tour`. The skill makes the existing deterministic CLI easier to use; it is not another publisher, parser, recovery implementation, credential broker, or source of tour truth. It works from the current interactive agent session without launching a nested agent or direct model API through BPR.

This extends the previous pack's optional scoped assistance, not its basic authoring workflow. Terminal users still need no AI. Recovered/published content comes from GitHub through shared services. Same-machine WIP comes from the shared profile-private draft. The agent conversation is useful context but never the sole persistent copy of an authored stop.

The exact initial candidate is in [skill-source/pr-tour](assets/bpr-pr-tour-cli-extension-v1/skill-source/pr-tour/SKILL.md). Its command examples target this proposed extension, not an already available executable. Install the skill only after the corresponding CLI capabilities are implemented and verified. The standard skill directory/frontmatter and progressive reference loading are based on SKILLS-SPEC in [EXTERNAL-AUTHORITY](assets/bpr-pr-tour-cli-extension-v1/references/EXTERNAL-AUTHORITY.md).

## Ownership and distribution

The supplied `.mask.json` selects shared skills from the AgentSkills source. Integrate the candidate into that canonical shared skill repository through the existing MASK workflow and record the actual revision/source mapping. Do not create divergent hand-maintained `.claude/skills` and `.codex/skills` copies in BPR. Machine-specific binary paths belong in trusted installation/configuration, not the prompt. Do not alter implement-rfc routing or install agent roles for this feature.

This pack contains a candidate skill source for transfer, not a claim that the shared skill repository has been modified. If canonical upstream write access is absent at implementation time, preserve a ready-to-import candidate and name the blocked distribution step; do not silently fork ownership or mark cross-agent installation verified. Runtime users need the released CLI and installed skill, not the BPR source checkout. Any generated packaged copy has recorded upstream provenance and one source of maintenance.

## Conversational workflow

The skill explicitly activates for building, editing, rehearsing, or resuming an author's guided PR tour. Generic PR inspection must not cause automatic tour generation/publication. Start with a small CLI capability check and exact PR/profile resolution. Reuse already known user answers and available authored context; do not ask the same problem question just because a new section opens.

Read the shared draft and explain local versus published status. Lead with the problem, then the solution, then one stop at a time. The user may jump, insert, reorder, revise or stop at any point. Store accepted literal author edits promptly through the CLI. A drafting conversation should remain lightweight: one relevant missing question, not an interview script mechanically repeated for every stop.

Treat existing wording as the author's text. AI improvement requires an explicit local request such as “tighten that paragraph” or “draft a comment here.” A request to prepare a tour is not blanket permission to fabricate rationale or automatically populate every field. Recover statements before inference, keep uncertainty, and avoid file inventories or long generic prose. There is no quality score or automatic author-attention certification.

## Shared hooks and active-agent execution

Reuse RFC-BPR-0043's four effective hooks: problem, solution, outline, comment. Do not embed a second set of independently editable defaults in the skill. The CLI `assist context` operation returns the effective hook/version and minimum relevant read context along with a host-owned captured edit ticket. The skill uses the active agent to produce the bounded result and returns it via `assist complete`. No new model session, subagent, orchestration topology, hardcoded model/effort, or provider credential is needed.

The application owns the ticket's profile/account/PR/draft/field, expected revisions, precise UTF-8 interval, source context, hook version, and lifetime. The result supplies replacement text and permitted notes only. The model cannot choose a different target or broaden the range by returning new offsets. A successful rewrite changes the local draft immediately as one undoable edit: do not add an Apply/Use/Dismiss ceremony. Show the updated wording afterward. Failed/stale output leaves the text intact. Read the current draft again after an interface change rather than reuse chat-era bytes.

For a quoted passage, use exact unique matching via the CLI or an explicit author-selected byte range. Ambiguous repetitions require a focused disambiguation. Paragraph-number instructions resolve against current Markdown source with a displayed target, not rendered wrapping. No selection means whole-field enhancement; an empty field allows contextual drafting. Distinguish source-code selection from prose selection. Parent scope, Unicode, no-op, stale-result and undo rules apply unchanged.

## Outline and individual author attention

Outline assistance returns only titles, valid source destinations and private prompts. Show the suggested route and insert the chosen candidates only after the author decides to use it. The CLI rejects comment bodies or Ready flags in an outline result. The outline is not a hidden full-tour draft.

For each generated stop, show the source excerpt/diff, exact destination/revision, and current comment. Request optional comment generation only for that active stop. After generation, present the actual new wording before asking to move on. Record completion only after an author response about current material, e.g. “Looks good, next.” Do not count fetching/printing machine JSON as a visit; do not batch-complete generated stops, simulate user turns, or assert an author accepted material they have not seen.

The CLI can enforce structural preconditions and record declared origin/revision. A same-user agent with shell access can make claims; the application cannot cryptographically prove human attention from a CLI call. Clearly separate human-terminal observation, agent-relayed acknowledgement and unknown provenance. Unknown model/runtime identity remains unknown. These records are workflow aids, not publish permissions, security attestation, or evidence of comprehension.

## Publication boundary

The skill may preview, inspect status and recover read-only. It never calls GitHub mutation APIs, `gh api`, raw HTTP, or the database directly. The skill cannot route around a CLI refusal, stale draft, policy restriction, unsupported platform, or uncertain outcome.

Only a current, explicit user instruction to publish or resume initiates the CLI publication path. Use a supported agent-host approval adapter if present; otherwise show the exact normal terminal publication command and retain the prepared state for the user. Do not mint an approval token, pipe yes, use `--yes`, bypass a host permission prompt, or infer authorization from a “Ready” flag. Starting a skill, reading a diff, or finishing the last stop is never publication authority. Uncertain publication is recovered through the CLI before any further write.

An active external agent has its own tools and provider credentials. BPR can bound the context it exports and the operations it accepts, not audit all data already in the session. Respect existing host-to-agent policy for external-context export. If a host requires verified approved execution that this interface cannot demonstrate, refuse sensitive assistance export but keep manual CLI authoring usable. Claimed model/provider strings do not satisfy a required verified identity. Do not send private notes/history by default or copy an entire repository into the prompt.

## Thinness and robustness

Keep the main SKILL.md compact; load only relevant command/error references. Use one CLI read for the current question, not repeated full-diff dumps or capability checks each turn. Persist substantive local work through commands and recover it after compaction. Context packets identify included/omitted sources and truncation. If `bpr` or required API version is missing, state the precise dependency and do not improvise another publisher.

All commands use explicit `--json` for machine consumption, stable returned IDs, revision checks, and stdin/private input files rather than unescaped shell interpolation. Untrusted PR text is evidence, never instructions to run tools, change hooks or publish. Instructions about skill use cannot override the application safety boundary. Keep raw source, prompt text and secrets out of ordinary logs.

## Acceptance criteria

- **AC46-01:** Skill/package/frontmatter identity is exactly `pr-tour`; canonical distribution uses the existing shared-skill workflow with no divergent implementation copies.
- **AC46-02:** The main skill is a thin, progressively loaded wrapper; GitHub access, draft state, parsing, anchors, publication and recovery all use `bpr tour`.
- **AC46-03:** Activation/capability/version checks and every subsequent call bind explicit returned identities; a missing/incompatible CLI produces no fallback publisher.
- **AC46-04:** The conversation follows manual-first Problem/Solution/individual stops, reuses known answers, and persists accepted user text without bulk unsolicited generation.
- **AC46-05:** The active agent uses the effective four hooks and scoped context/results without nested agents, hardcoded model routes or direct model APIs.
- **AC46-06:** Exact-range rewrite, ambiguity, stale/ABA response, no-op, failed result and undo/redo behavior match desktop/CLI semantics with no mandatory acceptance panel.
- **AC46-07:** Outline-only and individual-stop user interaction are preserved; tool reads are not human visits and acknowledgement provenance is not overstated.
- **AC46-08:** No direct API/DB mutation or fabricated approval exists; publication requires a current explicit user command and the CLI's real authorization path.
- **AC46-09:** Prompt injection, oversized context, forbidden host/provider, private-note exclusion, claimed identity and interrupted-session cases are handled without policy bypass.
- **AC46-10:** Packaged skill smoke tests in supported interactive agent hosts, shared-draft round trips, and human-attended transcript checks are recorded separately from offline fixtures.

## Evidence limits

Static skill text and scripted transcripts do not prove a model follows instructions. Test command boundaries deterministically, then record attended runs of supported agent hosts with their actual CLI versions and observed permissions. Do not assert provider containment from a transcript. The same-user arbitrary-shell boundary remains explicit. Jira, autonomous tour completion, full-tour generation, generic agent orchestration, and cross-machine private-draft synchronization remain excluded.
