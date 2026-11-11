---
id: "RFC-BPR-0115"
title: "The agent changes a local PR draft's worktree: fixes and history operations"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the local-draft-authoring-v1 pack, where it was RFC-DRAFT-0003","inspected_revision":"5380330fb1e1f15f5a3da476e0e8260ba4a730a3","milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"baseline":"b0d54697a6db6b17b459a751515de60aff14d65d"}
depends_on: ["RFC-BPR-0096", "RFC-BPR-0103", "RFC-BPR-0104", "RFC-BPR-0107", "RFC-BPR-0108", "RFC-BPR-0110", "RFC-BPR-0113", "RFC-BPR-0114"]
---

# The agent changes a local PR draft's worktree: fixes and history operations

## Problem and grounded current behavior

On a local draft an agent can read and propose, and nothing more:

- A local draft's conversations run in a private workspace materialized from
  the captured candidate. "Runs in it are read-only", and "reading the live
  worktree is not a fallback"
  ([RFC-BPR-0110](RFC-BPR-0110-local-tours-and-description.md)).
- The grants an agent can be given stop short of the author's files: "BPR
  does not let an agent write into the reviewer's own working tree"
  ([RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md)).
- An agent reply's finding cards offer **Add as draft comment** and **Draft
  all**, which make review comments for GitHub
  ([BPR-2059](../issues/40-parked/BPR-2059-suggested-comments-read-as-postable-comments.md)).
  A local draft has no pull request to comment on.
- [RFC-BPR-0111](RFC-BPR-0111-draft-publication-and-continuity.md)
  adds no "merge, force push, automatic rebase" operation.

So a finding on the author's own change is fixed by leaving the application,
and so is bringing the branch up to date or tidying its commits. The owner
asked for the opposite ([notes 1 and 3](assets/local-draft-authoring-v1/SOURCE_NOTES.md)): "let the agent fix
findings on the local PR draft", "let the agent do the merges squashes
whatever", and "minor self edits and let the agent do most of the changes".

Core already names an entry point for judging a fix,
`evaluate_proposed_fix`
([agent_chat_scope.hpp](../../src/core/include/bpr/core/agent_chat_scope.hpp)),
which no surface calls.

## Proposed behavior and boundaries

### Change runs

A **change run** is an agent turn whose working directory is the local
draft's own worktree and which may change files there. It is a different
kind of run from the draft's conversations, which stay read-only on the
candidate workspace exactly as RFC-BPR-0110 defines.

Change runs belong to one **Changes session** per local draft and agent,
under the session contract of
[RFC-BPR-0096](RFC-BPR-0096-native-agent-sessions-and-tour-context.md).
Its source binding is the worktree, so it continues across candidates, and
a follow-up ("not like that, keep the old name") is a turn in it. A running
turn shows its work and ends in one of the three ways of
[RFC-BPR-0103](RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md),
with the files written and the commands run in its activity line.

The author starts one, and nothing else does:

- **Fix** on a note of the local draft, whoever wrote the note.
- **Fix** on a finding card in the draft's whole-change conversation.
- **Ask for a change…**, whose subject is the whole change or attached lines
  of the candidate, as
  [RFC-BPR-0102](RFC-BPR-0102-agent-questions-say-what-they-point-at.md)
  defines subjects, with the author's instruction.

Each is an agent control and carries the agent accent of
[RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md). There is no
run on open or refresh and no "fix all".

A change run starts only when the worktree holds what the current candidate
describes (no refresh is pending), the checked-out branch is the draft's
source branch, and no editor in the draft has unsaved edits. While one is
running, a second change run, Save, Commit… and Push are unavailable in that
draft and say why. Read-only conversations continue.

### Self-review has its own prompt

A local draft offers **Review my change**: an agent control that asks for a
review of the whole candidate in the draft's whole-change conversation. It is
a read-only run on the candidate workspace, as RFC-BPR-0110 defines, and its
reply carries finding cards.

The owner asked that "self-review gets its own prompt in agent settings"
([note 6](assets/local-draft-authoring-v1/SOURCE_NOTES.md)). So the run is briefed by a built-in prompt of
its own, **Self-review**:

- It is listed in Settings › Agents & context › Built-in prompts with the
  other built-in prompts, with the same editor, built-in default, version and
  Reset they have.
- It is separate from the review skill, which is the method for reviewing
  someone else's pull request, from the Suggested comments prompt and from
  the whole-change discussion prompt of RFC-BPR-0110. Editing any one of them
  changes none of the others, and Review my change uses this one only.
- Its stable purpose ID is `local-draft.self-review`. It is a recognized
  purpose and an exportable item of the sharing format of
  [RFC-BPR-0107](RFC-BPR-0107-agent-and-prompt-sharing.md), under that
  format's rule for a purpose an older importer does not recognize.
- Like the other prompts it may name the author's personal or repository
  skills, and it grants nothing: the run stays read-only.
- Its default asks the agent to read the change as a reviewer would before
  one sees it: report what a reviewer would raise, verify each finding
  against the candidate before reporting it, say what it could not verify,
  and address each finding to the author as something to change, not as a
  comment to post.
- The run's details name the prompt's source (built-in or the author's own)
  and version, as an Agent tab brief names its comment format.

### Findings on a local draft become notes

On a local draft a finding card offers **Keep as note** where a pull request
review offers Add as draft comment, and a reply with several offers **Keep
all as notes (N)** with the counting, skipping and single Undo that Draft all
has. A kept note is a private note of the draft at the finding's file and
lines, marked with its agent origin. Keeping notes starts no run.

### The grant

A change run needs the grant **Edits this worktree**. The first change run
for a local draft and agent asks: "This lets *agent* change files in *path*
and run commands there with your account's rights. Allow for this local
draft?" The answer is remembered for that local draft and agent, shows as
the grant line's fourth value beside the three of RFC-BPR-0104, and is
withdrawn from that line. Declining starts nothing.

- BPR asks the agent's own permission system for exactly this: writes
  confined to the worktree root, plus its scratch folder when that is on.
  BPR's wording never calls it a sandbox, and network access stays as the
  agent's own configuration has it, as RFC-BPR-0104 requires.
- When **Build and run tests** is on for the agent, a change run may build
  and test in the worktree. The question above then also says that building
  runs this repository's code.
- A fix run is told to change files and to make no commit, push or branch
  change, and BPR passes no allowance for them.
- Nothing in a pull request review can be given this grant. RFC-BPR-0104's
  refusal there is unchanged. Tour runs stay read-only and the one-shot
  hooks stay tool-free.

### What comes back

Before a change run starts BPR records the worktree's state: the branch and
its head, every other branch and tag it can see, and the exact bytes of
every file with uncommitted changes. A file whose bytes cannot be captured
within the existing bounds is named, and the run starts only when the author
accepts that BPR will not be able to revert that file.

When the run ends, however it ends, BPR observes the worktree again and
lists **the run's changes**: each file that differs from the recorded
state, with its difference, in the file list and diff the draft already
uses. Each file offers **Keep** and **Revert**.

- Keep selects the file for the successor candidate.
- Revert writes back the recorded bytes through the worktree write port of
  [the editing RFC](RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md),
  under that RFC's rule: only while the file still holds what the run left.
  Reverting a file the run created removes it; reverting one the run deleted
  restores it.
- The draft then offers the ordinary refresh with the kept files selected.
  Until the author accepts it, the current candidate and its notes, evidence
  and tour are unchanged.

After a fix run BPR also reports, by name, anything it did not ask for: a
moved branch head, or any other branch or tag that changed.

A note that started a Fix records the run and the candidate that holds its
result. BPR does not resolve the note. **Check the fix** asks a read-only
question about that note on the successor candidate, through the existing
`evaluate_proposed_fix` entry point, and shows the answer as the agent's
claim. Only the author marks the note resolved.

### History runs

The draft's branch offers three agent operations: **Merge target in**,
**Squash commits…** and **Rebase onto target**. Each is a change run with
one further allowance: running the author's own Git in this worktree to
fetch the target branch and to create commits on the draft's branch. The
agent runs Git as its own process, so the author's identity, hooks and
credentials apply as they would in a terminal.

A history run starts only when the candidate is fully committed, no tracked
file has uncommitted changes, the checked-out branch is the draft's source
branch and its head is the candidate's head. Its record names the head the
branch had before the run, so the branch can be put back; BPR itself offers
no reset.

When it ends BPR reports what it can establish without the agent's word:

- **Complete or not.** A merge or rebase still in progress is reported as
  incomplete, with Git's own description, and makes no successor candidate.
- **Squash.** When the new head's content is identical to the old head's,
  BPR says so and how many commits became how many; the candidate's material
  is unchanged and its notes, evidence and tour stay attached. When it is
  not identical, BPR says the content changed and the result is reviewed as
  a successor candidate.
- **Merge and rebase.** The result is a successor candidate compared against
  the new base. Every file changed both on the draft's branch and on the
  target since the old base is marked as combined with target changes, so
  the author knows where to read. The agent's own account of conflicts is
  shown as its claim.
- **Anything else that moved.** Any branch or tag other than the draft's
  branch that changed is reported by name.

Squash and Rebase rewrite commits. When the commits they would rewrite have
been observed on the hosted branch, BPR stops first and says that the hosted
branch will not accept the result without a force push, which BPR never
performs. The run proceeds only on the author's confirmation, and afterwards
[Push](RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md#push)
refuses as it does for any push that is not a fast-forward. Merge target in
never needs a force push.

Evidence bound to an earlier candidate follows
[RFC-BPR-0109](RFC-BPR-0109-author-evidence.md) and RFC-BPR-0111: it
is not carried to a candidate whose content differs.

### Boundaries

- A change run is BPR's own agent run under a grant, in the manner of
  RFC-BPR-0104. It is not a configured extension action, so ACTIONS-01 in
  [RFC-BPR-0112](RFC-BPR-0112-optional-workflow-extensions.md) still
  governs those unchanged.
- BPR performs no merge, squash, rebase, reset, stash, force push, branch
  deletion or tag operation itself. The agent is told not to force-push,
  delete branches or move tags, and BPR's report after the run is the check.
- The grant is a permission the agent follows, with whatever enforcement the
  agent's own runtime provides. BPR claims no more than that.

### Architecture

Core owns the change-run kinds, their preconditions, the recorded state, the
comparison that yields the run's changes and each mechanical report. The
application launches the run, records and observes the worktree through the
existing isolated readers, and performs Revert through the write port. Qt
presents. No provider type crosses inward.

Proposals in this RFC that the owner has not separately confirmed are listed
in the [decision record](assets/local-draft-authoring-v1/DECISIONS.md#decisions) as P03, P04, P07, P09,
P10 and P11.

## Acceptance criteria

- **AC01:** Fix on a note, Fix on a finding card and Ask for a change… each start a change run in the draft's worktree after the one-time grant question; the grant line then reads "Edits this worktree"; declining starts nothing; and a pull request review offers neither the controls nor the grant.
- **AC02:** While a change run runs and after it ends, the current candidate and its notes, evidence and tour are unchanged until the author accepts a refresh, and a second change run, Save, Commit… and Push are unavailable with that reason while it runs.
- **AC03:** The run's changes list every file that differs from the state recorded before the run. Keep puts a file in the successor candidate. Revert restores exactly the recorded bytes, removes a file the run created and restores one it deleted, and refuses with the reason when the file is no longer what the run left.
- **AC04:** A fix run that moved the branch head, and any run that changed another branch or a tag, is reported after the run with each name.
- **AC05:** On a local draft a finding card offers Keep as note, and a reply with several offers Keep all as notes with Draft all's counting, skipping and single Undo; a kept note carries its agent origin and starts no run.
- **AC06:** Check the fix asks a read-only question on the successor candidate and shows the answer as the agent's claim; the note is resolved only by the author.
- **AC07:** A history run refuses to start with uncommitted tracked changes, a different checked-out branch or a head that is not the candidate's, and every history run's record names the head the branch had before it.
- **AC08:** A squash whose result has identical content is reported as such with its commit counts and keeps notes, evidence and tour attached; one whose content differs is reported as changed and reviewed as a successor candidate.
- **AC09:** A merge or rebase left in progress is reported as incomplete and makes no successor candidate; a complete one yields a successor candidate against the new base with every file changed on both sides since the old base marked.
- **AC10:** Squash and Rebase on commits observed on the hosted branch stop with the force-push statement and proceed only on confirmation; Push afterwards refuses; Merge target in shows no such stop.
- **AC11:** Tour runs and the one-shot hooks remain read-only and tool-free, the draft's conversations still read only the candidate workspace, and the agent accent appears only on the controls that start a run.
- **AC12:** On a frozen build with a fixture agent, the owner accepts the run's changes with Keep and Revert, the grant question and the three history reports.
- **AC13:** Review my change runs read-only on the candidate under the Self-review prompt and no other: the prompt is listed in Built-in prompts with its own editor, default, version and Reset; an edit to it reaches the next self-review's brief and leaves the review skill, the Suggested comments prompt and the whole-change discussion prompt unchanged, and edits to those leave it unchanged; and it exports and imports as its own item of the sharing format.

## Proposed delivery

[The grant, change runs and their changes](../issues/10-backlog/BPR-2068-worktree-grant-change-runs-keep-and-revert.md),
[self-review with its own prompt, notes from findings, Fix and Check the fix](../issues/10-backlog/BPR-2069-notes-from-findings-fix-and-check-the-fix.md),
[history runs](../issues/10-backlog/BPR-2070-history-runs-merge-squash-rebase.md) and
[closeout](../issues/10-backlog/BPR-2071-close-rfc-0115.md) form the third and
fourth [milestones](plans/local-draft-authoring-v1/milestone-plan.json).
