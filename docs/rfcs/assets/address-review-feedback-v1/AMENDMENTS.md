# Proposed amendments to adopted RFCs

This pack changes one rule in an adopted RFC and extends three others
without changing what they require. The sources were read on lane
`claude/v0-5-0-prerelease-targets-52265d` at `b68e572fea0dac33c2164fdf2eea2be8e1fa8eb0`. Authoring leaves the
live text unchanged; each owning milestone applies its own through
`implement-rfc`.

## A1. RFC-BPR-0098: a changes-requested pull request can be Waiting

Source: [RFC-BPR-0098](../../RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md),
"One next move per authored row".

| Before | After |
|---|---|
| Returned: "The review decision is changes requested, or at least one unresolved review conversation's latest comment is not mine". | Returned: at least one unresolved review conversation's latest comment is not mine; or the review decision is changes requested and some reviewer whose review requested changes has not been asked to review again since that review. |
| A changes-requested pull request is never Waiting. | It is Waiting when every such reviewer has been asked again and no conversation awaits the author. The row's sentence names who was asked. |

Preserved: the order of the table, every other row, the rule that the move
is decided from facts the host observed, and the staleness wording.

Rationale: GitHub keeps a changes-requested decision until that reviewer
reviews again, so without this the author's own Home keeps telling them to
act after they have answered. Reconciliation:
[DRAFT-0009](cards/DRAFT-0009-home-reads-waiting-after-a-response.md).
This is a proposal the owner has not confirmed (P09).

## Additions that change no existing requirement

- **RFC-BPR-0108** lets a local draft be associated with a pull request
  that is found later. This pack also creates an associated local draft
  starting from the pull request, on a verified worktree, and adds Feedback
  as a fifth working area for an associated draft
  ([DRAFT-0001](cards/DRAFT-0001-address-feedback-and-the-pull-request-association.md),
  [DRAFT-0002](cards/DRAFT-0002-feedback-items-placement-and-dispositions.md)).
- **RFC-BPR-0115** starts a fix run from a note, a finding card or Ask for
  a change…. This pack adds a feedback item and a set of them as starting
  points, and attribution of the run's changes to items. Its fix-run rules,
  grant, Keep and Revert are used as written
  ([DRAFT-0005](cards/DRAFT-0005-fix-accepted-items-in-one-run.md)).
- **RFC-BPR-0114** already lets an associated local draft push later
  commits on the author's press. Publish response uses that Push as its
  first step. RFC-BPR-0111's rule against automatic post-publication pushes
  stands
  ([DRAFT-0008](cards/DRAFT-0008-publish-response-in-order.md)).
- **RFC-BPR-0107** allows that "a later RFC may add recognized purposes".
  This pack proposes two: `local-draft.feedback-evaluate` and
  `local-draft.feedback-reply`.
