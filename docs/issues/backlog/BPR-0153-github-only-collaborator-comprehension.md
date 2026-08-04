---
id: BPR-0153
title: Confirm a GitHub-only collaborator understands a published defect
status: Backlog
rfc: RFC-BPR-0009
depends_on: [BPR-0149, BPR-0142]
---

# Confirm a GitHub-only collaborator understands a published defect

RFC-BPR-0009 requires that a collaborator who has never used BetterPullReview
can understand and respond to every published defect. That is a claim about
whether prose is *sufficient*, and no fixture can settle it: the fixture
contains the words this tool wrote, and the question is whether a person who
lacks all local context can act on them.

## Exactly what is missing

1. The designated safe repository and write-scoped token from BPR-0142.
2. **A person who has not used this tool**, reading the thread as an ordinary
   GitHub collaborator.

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
