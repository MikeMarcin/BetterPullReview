# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **local-draft-authoring-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## edit-and-save

The author edits a listed file of a local PR draft in the existing code editor and saves it to the worktree, and the save returns as a successor candidate.

**Work:** `BPR-2062`, `BPR-2063`, `BPR-2064`

**Prerequisites:** None

**Completion:**

- A saved edit is on disk byte for byte and nothing is staged or committed.
- A file changed on disk since it was opened is never overwritten, and a not-editable file refuses with its reason.
- The draft offers one refresh with the saved files selected; the previous candidate is retained.
- No Qt widget or model performs the write, and the workspace adapter is still read-only.

**Boundary:** Editing and saving only: no commit, no push and no agent. Needs the local draft's durable candidates from RFC-BPR-0108 first.

## commit-and-push

The author commits the selected files and pushes the draft's branch from the local draft through their own Git, and Create draft PR follows without leaving the application.

**Work:** `BPR-2065`, `BPR-2066`, `BPR-2067`

**Prerequisites:** None

**Completion:**

- A commit holds exactly the selected files' captured content with the author's identity, signing and hooks, and leaves every other change as it was.
- A push reaches the resolved hosted repository by its configured URL with the author's credentials and pre-push hooks, and never forces.
- The isolated runner is unchanged, and the own-Git catalogue holds commit and push only.
- Declining the one-time question leaves the named commands of RFC-BPR-0111 as the path.
- RFC-BPR-0111's live text and BPR-2029 are reconciled with the reopened PUBLISH-01.

**Boundary:** Reopened PUBLISH-01: BPR commits and pushes on the author's press. No merge, squash, rebase or force push. Needs the native pull request creation of RFC-BPR-0111 first.

## agent-fixes

The author has an agent review their own change under a prompt of its own, fix a note or a finding in the local draft's worktree, keeps or reverts each changed file, and checks the fix.

**Work:** `BPR-2068`, `BPR-2069`

**Prerequisites:** edit-and-save

**Completion:**

- A change run starts only on the author's press and under the Edits this worktree grant, never in a pull request review.
- The candidate and everything attached to it are unchanged until the author accepts a refresh.
- Keep and Revert act per file, and Revert restores exactly the recorded bytes.
- Self-review runs read-only under its own editable prompt from Agent settings, separate from the review skill.
- Findings become notes on a local draft, and Check the fix shows the agent's answer as a claim.
- RFC-BPR-0104's live text is reconciled with the new grant.

**Boundary:** Fix runs change files and make no commit. Needs the agent grant line of RFC-BPR-0104, the local-draft sessions of RFC-BPR-0110 and the prompt sharing format of RFC-BPR-0107 first.

## agent-history-operations

The author has an agent merge the target in, squash or rebase the draft's branch, and BPR reports what it can establish about the result.

**Work:** `BPR-2070`, `BPR-2071`

**Prerequisites:** agent-fixes, commit-and-push

**Completion:**

- A history run refuses to start on an unclean or mismatched worktree and records the earlier head.
- Identical content after a squash keeps notes, evidence and tour attached; changed content is reviewed as a successor candidate.
- An incomplete merge or rebase makes no successor candidate; a complete one marks every file changed on both sides.
- Rewriting commits observed on the hosted branch stops first, and nothing in BPR force-pushes.
- The agent-changes RFC closeout judges every criterion with an installed agent as well as the fixture agent.

**Boundary:** History operations exist only as agent runs. BPR itself performs none and offers no reset.
