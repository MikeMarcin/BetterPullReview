# Design record

## Scope and provenance

Pack `address-review-feedback-v1`, first version, authored 2026-11-11. The
owner's words are in [SOURCE_NOTES.md](SOURCE_NOTES.md). Grounded against
lane `claude/v0-5-0-prerelease-targets-52265d` at `b68e572fea0dac33c2164fdf2eea2be8e1fa8eb0`. The pack is not
imported: its IDs are provisional. No review of it has been performed; none
was requested.

## Decisions

Settled means the owner said it. Proposed means the author of this pack
chose it and the owner has not separately confirmed it.

| Decision | Requirement/acceptance | Status and actual answer | Rationale/consequences | Depends on / affected artifacts |
|---|---|---|---|---|
| D01: answer a review inside BPR, as an extension of the local draft for a live pull request | All three RFCs | Settled. "I think I could have that flow fully within BPR as some sort of extension to the current local draft PR workflow for live PR work." ([note 1](SOURCE_NOTES.md)) | The flow is evaluate, respond to and fix all review comments, in the owner's words. | All |
| D02: author it as a pack and have the other session add it to its work | This pack | Settled. "/author-rfc a pack for this and message the other session to add it to it's work" ([note 3](SOURCE_NOTES.md)) | Authoring only. The import is left to the session that owns the lane. | All |
| P01: Address feedback creates an associated local draft from a chosen, verified worktree | Feedback RFC AC02, AC03 | Proposed. | A pull request that never had a local draft can still be answered. BPR moves no branch to make the worktree match. | DRAFT-0001; RFC-BPR-0108 |
| P02: what an item is, its four dispositions, and placement only where the mapping carries the lines unchanged | Feedback RFC AC04 to AC06 | Proposed. | Dispositions are private; nothing is guessed about where a comment now belongs. | DRAFT-0002 |
| P03: one batch change run over all accepted items, with per-item attribution shown as a claim | Agent RFC AC03, AC04 | Proposed. One of the three defaults the owner was shown before asking for the pack ([note 2](SOURCE_NOTES.md)); not separately confirmed. | Matches "all review comments". A single item's Fix remains. | DRAFT-0005; RFC-BPR-0115 |
| P04: evaluation proposes a disposition per item and the author disposes | Agent RFC AC01 | Proposed. | Nothing is accepted automatically; "Needs your answer" exists for what only the author can say. | DRAFT-0004 |
| P05: reviewers' text is quoted data in every brief | Agent RFC AC02 | Proposed. | A comment written by someone else must not be able to steer a run that can change the author's files. | DRAFT-0004 to 0006 |
| P06: two built-in prompts of their own, for evaluation and for replies; a commit a reply names must be on the branch | Agent RFC AC06, AC07 | Proposed. The prompts follow the owner's request for self-review; the owner has also merged three Enhance prompts into one, which points the other way. | Two new prompt purposes. | DRAFT-0004, 0006; RFC-BPR-0107 |
| P07: no conversation is resolved unless the author ticks it | Publishing RFC AC05 | Proposed. One of the three defaults the owner was shown ([note 2](SOURCE_NOTES.md)); not separately confirmed. | Many teams leave resolving to the reviewer. | DRAFT-0008 |
| P08: Re-request review is in the plan, unticked | Publishing RFC AC01, AC05 | Proposed. One of the three defaults the owner was shown ([note 2](SOURCE_NOTES.md)); not separately confirmed. | Uses the existing reviewer request operation. | DRAFT-0008 |
| P09: Home reads Waiting for a changes-requested pull request once every requesting reviewer has been asked again | Publishing RFC AC07; amendment A1 | Proposed. Not shown to the owner before authoring. | Changes an adopted rule of RFC-BPR-0098. Without it, Home keeps saying Returned after the author has answered. | DRAFT-0009 |

## Open frontier and explicit deferrals

No question blocks import. P09 is the one to put to the owner before it is
built, because it changes an adopted rule and the owner has not seen it.

Deferred, with the boundary each leaves:

- **A worktree BPR makes for you.** Address feedback needs a worktree that
  already has the branch checked out. Cloning, fetching or checking out the
  branch is not done by BPR.
- **Bringing a behind or diverged branch up to date.** It is a recovery
  task here. RFC-BPR-0115's Merge target in is for the target branch, not
  for the pull request's own hosted branch.
- **Feedback that is not a review conversation or a review summary:**
  ordinary pull request comments, failing checks and merge conflicts.
- **An editable prompt for the fix run**, and a commit made by Publish
  response.
- **Anything automatic:** evaluating on open, fixing without acceptance,
  posting on commit.

## Checks and confirmation

1. **Structural validity.** `python3 <implement-rfc>/examples/milestone-plans/validate.py milestone-plan.json`
   answered "Structurally valid; references and semantic coverage
   unchecked." [MILESTONES.md](MILESTONES.md) is that validator's
   `--render` output.
2. **Design readiness.** All three milestones have defined behavior and
   acceptance. D01 and D02 are the owner's; P01 to P09 are proposals.
3. **Confirmation.** The owner's two statements. The owner was shown three
   defaults (P03, P07, P08) and then asked for the pack without commenting
   on them. No RFC text was confirmed. The pack was not reviewed or grilled.
4. **Execution eligibility.** Not evaluated by authoring, and the pack is
   not imported. All three milestones wait on local-draft work that is not
   yet built.

## Continuation

Next action: import through implement-rfc on the lane that holds
RFC-BPR-0108 to 0115, then add the three milestones to that lane's queue.
Artifacts: the [overview](README.md), the three RFCs, [cards/](cards/), the
[proposed amendments](AMENDMENTS.md), the [plan](milestone-plan.json) and
the [implementation entrypoint](IMPLEMENTATION_PROMPT.md).
