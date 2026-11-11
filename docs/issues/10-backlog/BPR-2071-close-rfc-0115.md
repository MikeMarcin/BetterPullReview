---
schema: "implement-rfc.card.v1"
id: "BPR-2071"
title: "Verify and close RFC-BPR-0115"
status: "Backlog"
rfcs: ["RFC-BPR-0115"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09","AC10","AC11","AC12","AC13"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0010-close-agent-changes.md","draft_title":"Close agent changes in a local draft's worktree"}
depends_on: ["BPR-2068", "BPR-2069", "BPR-2070"]
closeout_for: ["RFC-BPR-0115"]
---

# BPR-2071: Verify and close RFC-BPR-0115

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0115](../../rfcs/RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`, `AC10`, `AC11`, `AC12`, `AC13`.

Acceptance assessment for agent fixes and history runs on a local draft.

## Intended verification

Judge every criterion on the composed application with an installed agent as well as the fixture agent, and say for each installed agent whether its own runtime enforces the grant or only follows it.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `b0d54697a6db6b17b459a751515de60aff14d65d`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Fix on a note, Fix on a finding card and Ask for a change… each start a change run in the draft's worktree after the one-time grant question; the grant line then reads "Edits this worktree"; declining starts nothing; and a pull request review offers neither the controls nor the grant. | Pending | Pending | Not implemented |
| **AC02:** While a change run runs and after it ends, the current candidate and its notes, evidence and tour are unchanged until the author accepts a refresh, and a second change run, Save, Commit… and Push are unavailable with that reason while it runs. | Pending | Pending | Not implemented |
| **AC03:** The run's changes list every file that differs from the state recorded before the run. Keep puts a file in the successor candidate. Revert restores exactly the recorded bytes, removes a file the run created and restores one it deleted, and refuses with the reason when the file is no longer what the run left. | Pending | Pending | Not implemented |
| **AC04:** A fix run that moved the branch head, and any run that changed another branch or a tag, is reported after the run with each name. | Pending | Pending | Not implemented |
| **AC05:** On a local draft a finding card offers Keep as note, and a reply with several offers Keep all as notes with Draft all's counting, skipping and single Undo; a kept note carries its agent origin and starts no run. | Pending | Pending | Not implemented |
| **AC06:** Check the fix asks a read-only question on the successor candidate and shows the answer as the agent's claim; the note is resolved only by the author. | Pending | Pending | Not implemented |
| **AC07:** A history run refuses to start with uncommitted tracked changes, a different checked-out branch or a head that is not the candidate's, and every history run's record names the head the branch had before it. | Pending | Pending | Not implemented |
| **AC08:** A squash whose result has identical content is reported as such with its commit counts and keeps notes, evidence and tour attached; one whose content differs is reported as changed and reviewed as a successor candidate. | Pending | Pending | Not implemented |
| **AC09:** A merge or rebase left in progress is reported as incomplete and makes no successor candidate; a complete one yields a successor candidate against the new base with every file changed on both sides since the old base marked. | Pending | Pending | Not implemented |
| **AC10:** Squash and Rebase on commits observed on the hosted branch stop with the force-push statement and proceed only on confirmation; Push afterwards refuses; Merge target in shows no such stop. | Pending | Pending | Not implemented |
| **AC11:** Tour runs and the one-shot hooks remain read-only and tool-free, the draft's conversations still read only the candidate workspace, and the agent accent appears only on the controls that start a run. | Pending | Pending | Not implemented |
| **AC12:** On a frozen build with a fixture agent, the owner accepts the run's changes with Keep and Revert, the grant question and the three history reports. | Pending | Pending | Not implemented |
| **AC13:** Review my change runs read-only on the candidate under the Self-review prompt and no other: the prompt is listed in Built-in prompts with its own editor, default, version and Reset; an edit to it reaches the next self-review's brief and leaves the review skill, the Suggested comments prompt and the whole-change discussion prompt unchanged, and edits to those leave it unchanged; and it exports and imports as its own item of the sharing format. | Pending | Pending | Not implemented |
