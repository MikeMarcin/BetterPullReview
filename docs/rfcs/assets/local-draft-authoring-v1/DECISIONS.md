# Design record

## Scope and provenance

Pack `local-draft-authoring-v1`, first version, authored 2026-11-10 from one
session with the owner, plus one message sent during import. The owner's words are preserved in
[SOURCE_NOTES.md](SOURCE_NOTES.md). The pack extends the adopted local PR
draft RFCs (RFC-BPR-0108 to 0112) and was grounded against lane
`claude/v0-5-0-prerelease-targets-52265d` at
`5380330fb1e1f15f5a3da476e0e8260ba4a730a3`, the only branch that holds
them. No review of this pack has been performed; none was requested.

## Decisions

Settled means the owner said it. Proposed means the author of this pack
chose it and the owner has not separately confirmed it.

| Decision | Requirement/acceptance | Status and actual answer | Rationale/consequences | Depends on / affected artifacts |
|---|---|---|---|---|
| D01: edit files in a local draft, with real saves | [Editing RFC](RFC-DRAFT-0001-edit-and-save-files-in-a-local-pr-draft.md), AC01 to AC07 | Settled. "I think we can introduce the text editor with real mutation and saving with git operations too." ([note 3](SOURCE_NOTES.md)) | Uses the existing editing model; needs a new write port, since the workspace adapter is read-only. | DRAFT-0001 to 0003; amendment A3 |
| D02: PUBLISH-01 reopened; BPR commits and pushes through the author's own Git | [Commit-and-push RFC](RFC-DRAFT-0002-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md), AC01 to AC08 | Settled. Asked "Reopen PUBLISH-01 so BPR runs commit and push through your own Git?", the owner answered "Yes, reopen it." ([note 4](SOURCE_NOTES.md)) | Supersedes D13 of the earlier pack. A second runner beside the isolated one; the named push stays as the fallback. | DRAFT-0004 to 0006; amendment A1; BPR-2029, BPR-2031; the adopted plan's `publish-prepared-draft` exit |
| D03: the agent fixes findings and does merges, squashes and rebases | [Agent-changes RFC](RFC-DRAFT-0003-agent-changes-in-a-local-pr-draft-worktree.md), AC01 to AC12 | Settled as a direction. "let the agent fix findings on the local PR draft … let the agent do the merges squashes whatever" ([note 1](SOURCE_NOTES.md)); "minor self edits and let the agent do most of the changes is probably good" ([note 3](SOURCE_NOTES.md)) | The answers set the direction, not the mechanics; P03, P04, P07, P09 and P10 below are the mechanics. | DRAFT-0007 to 0010; amendment A2 |
| D04: go to definition and compiler-backed highlighting | None in this pack | Explicitly deferred. "I think the goto definition stuff can stay on the wishlist for now" ([note 3](SOURCE_NOTES.md)) | See deferrals. | None |
| D05: author as a pack and import | This pack and its live import | Settled. "so /author-rfc our final suggestions as a pack and import it" ([note 5](SOURCE_NOTES.md)) | Authorizes import. It is not a mandate to implement. | All |
| D06: self-review has its own prompt in agent settings | Agent-changes RFC, "Self-review has its own prompt", AC13 | Settled. "make sure that self-review gets its own prompt in agent settings" ([note 6](SOURCE_NOTES.md)) | A built-in prompt of its own, separate from the review skill; a new recognized purpose for the sharing format of RFC-BPR-0107. | DRAFT-0008 |
| P01: limits on the author-Git runner and on saving | Commit-and-push RFC, "The author's own Git", AC04; editing RFC, AC02 | Proposed. The three limits were in the message the owner's "Yes, reopen it" answered; the answer was to the question, and the limits were not separately confirmed. | Only a local draft's own worktree and only on a press; nothing force-pushes and a rewrite of pushed commits stops and asks; a save writes only if the file still matches. | DRAFT-0001, 0004, 0005, 0009 |
| P02: one question per local draft before the first Commit or Push | Commit-and-push RFC, AC05 | Proposed. Follows the build question of RFC-BPR-0104. | Hooks run the repository's code with the author's rights, and a local draft can be opened on any checkout. | DRAFT-0004 |
| P03: the Edits this worktree grant, asked once per local draft and agent | Agent-changes RFC, "The grant", AC01 | Proposed. Follows RFC-BPR-0104's grant line and question. | Local drafts only; a pull request review can never be given it. | DRAFT-0007; amendment A2 |
| P04: a fix run changes files and makes no commit; only history runs create commits | Agent-changes RFC, "The grant" and "History runs", AC04, AC07 | Proposed. | Keeps each fix reviewable per file before it enters history, and keeps Revert sound. | DRAFT-0007, 0009 |
| P05: unsaved edits are not kept across a crash | Editing RFC, "Save", AC05 | Proposed. | Smallest first version; durable unsaved buffers are a deferral. | DRAFT-0002 |
| P06: Edit covers files the draft lists, up to 1 MiB, with no create, delete or rename | Editing RFC, "Edit opens a file of the draft", AC03 | Proposed. The bound is the editing model's existing one. | "Minor self edits"; larger work belongs in the author's editor. | DRAFT-0001, 0002 |
| P07: one Changes session per local draft and agent, continuing across candidates | Agent-changes RFC, "Change runs" | Proposed. Differs deliberately from P04 of the earlier pack, where a conversation restarts on a new candidate: that rule is for sessions bound to a candidate, and this one is bound to the worktree. | A follow-up to a fix is a turn, not a new session. | DRAFT-0007 |
| P08: Push from an associated local draft sends later commits on the author's press | Commit-and-push RFC, AC07 | Proposed. | Consistent with RFC-BPR-0111's ban on automatic post-publication pushes. | DRAFT-0005 |
| P09: recorded state before a run; Keep and Revert per file | Agent-changes RFC, "What comes back", AC03 | Proposed. | Revert needs bytes BPR recorded itself; a file it could not record is named before the run. | DRAFT-0007 |
| P10: findings become notes on a local draft; Fix; Check the fix | Agent-changes RFC, AC05, AC06 | Proposed. Mirrors Add as draft comment and Draft all. | A local draft has no pull request to comment on. Uses the existing, uncalled `evaluate_proposed_fix` entry point. | DRAFT-0008 |
| P11: the Self-review prompt's name, purpose ID `local-draft.self-review`, default wording and the Review my change control | Agent-changes RFC, "Self-review has its own prompt", AC13 | Proposed. The owner asked for the prompt; its name, ID, default and the control that uses it are the author's. | Follows the existing built-in prompts' list, editor, version and Reset. | DRAFT-0008 |

## Open frontier and explicit deferrals

No question is waiting on the owner for import. P01 to P11 are proposals:
implementation should not treat them as the owner's answers, and the scope
each names is the scope to confirm before it is built. P04 and P09 are the
two with a real alternative (a fix run that commits; no per-file Revert).

Deferred, with the boundary each leaves:

- **Go to definition and compiler-backed highlighting** (D04). Nothing in
  this pack depends on them. The discussion's shape, kept for a later RFC:
  an installed `clangd` with a compile database, the agent used once to work
  out how a repository produces that database, local drafts and C++ first.
- **The reviewer-side agent ideas from the same session.** A summary of what
  changed since the last review (`analyze_latest_upload`), an Addressed?
  check on a pull request thread (`evaluate_proposed_fix` there),
  explain-this-file (`analyze_current_file`), comment to exact suggested
  edit, failing-check triage, agent-proposed evidence checks and an
  acceptance rate in Insights. The owner did not take these up. Only
  `evaluate_proposed_fix` is used here, and only on a local draft's notes.
- **More editor.** Creating, deleting and renaming files; files the draft
  does not list; files over 1 MiB; unsaved edits that survive a crash.
- **Prompts for the change runs.** Fix, Ask for a change… and the history
  runs have no editable prompt of their own here; only self-review was asked
  for.
- **More Git.** Committing part of a file, amend, a staging view, an
  agent-drafted commit message, and any BPR-run reset or restore after a
  history run.
- **Native pull request creation on other hosts.** Unchanged from
  RFC-BPR-0111.

## Checks and confirmation

1. **Structural validity.** `python3 <implement-rfc>/examples/milestone-plans/validate.py milestone-plan.json`
   answered "Structurally valid; references and semantic coverage
   unchecked." [MILESTONES.md](MILESTONES.md) is that validator's
   `--render` output. Issue and criterion selectors were not resolved
   against a live board by the validator.
2. **Design readiness.** All four milestones have defined behavior and
   acceptance. D01, D02, D03 and D06 are the owner's; P01 to P11 are
   proposals with no owner answer. No behavior is left undecided, but
   eleven mechanics are unconfirmed.
3. **Confirmation.** The owner's six statements in the source notes, each
   covering what it says and no more. No blanket confirmation of this
   pack's text was asked for or given. The pack was not reviewed or
   grilled.
4. **Execution eligibility.** Not evaluated by authoring. Every card is
   Backlog. All four milestones wait on adopted work that is itself
   unimplemented: RFC-BPR-0108's candidates, RFC-BPR-0111's pull request
   creation, RFC-BPR-0104's grant line and RFC-BPR-0110's local-draft
   sessions. The owner asked for import, not implementation.

## Continuation

Next action: implementation needs its own mandate. Before `agent-fixes`,
put P03, P04, P07, P09, P10 and P11 to the owner. Artifacts: the
[overview](README.md), the three RFCs, [cards/](cards/), the
[proposed amendments](AMENDMENTS.md), the [glossary](GLOSSARY.md), the
[plan](milestone-plan.json) and the
[implementation entrypoint](IMPLEMENTATION_PROMPT.md).
