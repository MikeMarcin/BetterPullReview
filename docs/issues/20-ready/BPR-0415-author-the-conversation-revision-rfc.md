---
id: BPR-0415
title: Author the conversation-revision RFC
status: Ready
rfc: none
depends_on: [BPR-0413]
---

# Author the conversation-revision RFC

[[BPR-0212]] recorded an idea the owner named as the thing GitHub is worst at —
seeing conversations and code evolve together across revisions — and deliberately
refused to work it as a slice, because it changes what a conversation *is* in the
model. On 2026-08-21 the owner accepted the [[BPR-0413]] mockup subject for it,
which authorizes exactly one thing: writing the RFC.

This card writes it. It does not implement it, and it does not add a field.

## Why this is a card and not a slice of BPR-0212

[[BPR-0212]] is the durable record of the idea and the measurement behind it. If
it were moved to an executable lane, the next session would reasonably read it as
permission to add a revision to the projection, which the card explicitly warns
against and which the verdict did not authorize. Keeping the authoring work here
leaves that warning intact and gives the RFC a card that can actually close.

## What the RFC must settle

The measurement is already done and must be carried in rather than rediscovered:

- The conversation projection carries no revision identity — verified field by
  field at `src/application/include/bpr/application/github_projection.hpp:124-139`.
- A **review's** commit is already fetched and required
  (`src/adapters/github/src/github_read_contract.cpp:1214-1241`,
  `src/application/include/bpr/application/github_read_model.hpp:223`) and then
  dropped at `src/application/src/github_projection_mapping.cpp:145-156`.
- The **write** path already sends `commit_id` for a new review comment
  (`src/adapters/github/src/github_mutation_contract.cpp:117-122`).
- The **read** path never asks for a comment's commit: the review-thread
  selection requests `id bodyText path line originalLine outdated author replyTo`.

So the RFC's first open question is narrow and answerable: can the per-comment
commit be read, and can it be read unauthenticated? That has not been checked
against the live API and this card must not assume it.

Beyond that it has to settle ownership, because the change crosses two RFCs:
the conversation model belongs to RFC-BPR-0002 and the epoch axis to
RFC-BPR-0003. Both owners have to agree before a field is added.

## Required final state

- A new RFC exists in `docs/rfcs/` with the next canonical ID, registered in
  `docs/rfcs/README.md` at `status: Proposed`.
- It states the model change, the fetch change, the migration for conversations
  already persisted without a revision, and what the UI would then be able to
  show.
- It records the unauthenticated-read question as open, with the exact GraphQL
  field it depends on.
- It does not claim any implementation, and no projection field is added by this
  card.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```
