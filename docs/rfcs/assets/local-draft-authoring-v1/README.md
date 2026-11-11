# Local draft authoring: proposed pack

This pack lets an author change their own change from inside a local PR
draft: edit and save a file, commit and push, and have an agent fix findings
and merge, squash or rebase the branch. It extends the adopted local PR
draft RFCs (RFC-BPR-0108 to 0112) and reopens one of their settled choices,
PUBLISH-01. Nothing here has been implemented.

The owner's words are preserved verbatim in
[SOURCE_NOTES.md](SOURCE_NOTES.md). [DECISIONS.md](DECISIONS.md) is the
single record of what the owner said, what is only proposed, the
deferrals, the checks run on this draft and the unevaluated execution
eligibility. [AMENDMENTS.md](AMENDMENTS.md) gives the before and after text
for the adopted RFCs this changes. The [glossary](GLOSSARY.md) fixes the
terms the three RFCs share.

## The RFCs

| RFC | Outcome | Owner's statement |
|---|---|---|
| [RFC-DRAFT-0001](RFC-DRAFT-0001-edit-and-save-files-in-a-local-pr-draft.md) | A listed file of a local draft opens in the existing code editor and saves to the worktree; the save returns as a successor candidate | Notes 1 to 3 |
| [RFC-DRAFT-0002](RFC-DRAFT-0002-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md) | BPR commits the selected files and pushes the draft's branch through the author's own Git, on the author's press | Notes 1 and 4 |
| [RFC-DRAFT-0003](RFC-DRAFT-0003-agent-changes-in-a-local-pr-draft-worktree.md) | An agent reviews the author's own change under a Self-review prompt of its own, fixes a note or finding in the worktree and merges, squashes or rebases the branch; every change comes back to be kept or reverted | Notes 1, 3 and 6 |

One rule holds across all three: whoever changed the worktree, the author or
an agent, the change reaches the draft only as a successor candidate the
author accepts.

## Cards and plan

Ten proposed cards are in [cards/](cards/): two or three implementation
cards and one closeout per RFC. The canonical grouping is
[milestone-plan.json](milestone-plan.json), with its
[schema](milestone-plan.schema.json); [MILESTONES.md](MILESTONES.md) is
generated from it. The four milestones are `edit-and-save`,
`commit-and-push`, `agent-fixes` and `agent-history-operations`. The first
two are independent of each other.

Start from the [implementation entrypoint](IMPLEMENTATION_PROMPT.md).

## Not in this pack

Go to definition and compiler-backed highlighting stay on the wishlist by
the owner's choice. The reviewer-side agent ideas from the same session were
not taken up. Both are recorded under
[explicit deferrals](DECISIONS.md#open-frontier-and-explicit-deferrals).
