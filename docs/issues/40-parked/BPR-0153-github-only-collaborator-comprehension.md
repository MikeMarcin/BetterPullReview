---
id: BPR-0153
title: Confirm a GitHub-only collaborator understands a published defect
status: Parked
rfc: RFC-BPR-0009
depends_on: [BPR-0149, BPR-0142]
parked_on: awaiting-owner-verdict
---

# Confirm a GitHub-only collaborator understands a published defect

RFC-BPR-0009 requires that a collaborator who has never used BetterPullReview
can understand and respond to every published defect. That is a claim about
whether prose is *sufficient*, and no fixture can settle it: the fixture
contains the words this tool wrote, and the question is whether a person who
lacks all local context can act on them.

## Exactly what is missing

1. A concrete defect and disposable PR on which to publish it. BPR-0142 is
   Done, but its round-trip comment names BetterPullReview and is not a defect
   report suitable for this comprehension check. Existing MikeMarcin sign-in
   is retained; no new token request is needed.
2. **A person who has not used this tool**, reading the thread as an ordinary
   GitHub collaborator. The owner must identify that collaborator; an agent
   cannot supply their interpretation or acceptance.
3. A concrete publication/reply course after those inputs are known. Earlier
   source-comment/reply authority does not authorize a new defect publication
   or contacting another person.

## What must not be done instead

Do not answer this by reading the prose yourself. You know what severity and
category the defect carries and what the local lifecycle means, and you cannot
un-know it to judge whether the comment stands alone.

Do not answer it by adding a marker, footer, or legend to the published
comment. The RFC's non-goals forbid hidden BetterPullReview metadata, and a
comment that needs a legend has already failed this criterion.

## Required direct evidence

- A defect is published, and the thread is reproduced here in full as it
  appears to a collaborator.
- That collaborator states what corrective work is being asked for, without
  being told the local severity or category.
- They reply, and the reply returns to the workbench as an ordinary reply.
- Nothing in the published prose refers to BetterPullReview.

## Parked checkpoint — 2026-09-05

Both implementation dependencies are Done. The remaining evidence needs the
collaborator and defect/target above, so this card is parked awaiting the owner.
No result is inferred from the owner's earlier visual approval of BPR-0142.
Prepare the exact prose and attended packet once the target and collaborator
are identified, then collect their ordinary-GitHub interpretation and reply.

Standing owner CI constraint: do not dispatch Actions, push, or incur paid CI.
Before any future GitHub mutation, inspect the target's workflow triggers and
do not execute a test action that would trigger Actions. No mutation or message
to another person was performed while preparing this checkpoint.
