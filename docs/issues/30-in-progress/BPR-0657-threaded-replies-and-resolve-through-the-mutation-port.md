---
schema: "implement-rfc.card.v1"
id: "BPR-0657"
title: "Threaded replies and resolve through the mutation port"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Threaded replies and resolve through the mutation port

RFC-BPR-0028's cards and feed offer Reply and Resolve on a thread. The
review mutation port publishes a comment at a line
(`review_mutation_request` carries a `review_publication_proposal` and its
anchor) and knows nothing of the thread it answers, so a "reply" published
today lands as a new comment on the line rather than in the thread; resolve
and unresolve exist only as capabilities (`github_capability::resolve_thread`)
with no intent, port method or host path. [BPR-0648](../60-done/BPR-0648-aligned-comment-cards-in-read.md)
therefore offers "Comment here" on a card, which starts a draft on the
thread's line, and no Resolve.

## Scope

- Core: a reply proposal that names the thread (and the comment) it answers,
  and resolve/unresolve intents with their refusals (capability absent,
  anonymous read, thread already in the state asked for).
- Application and GitHub adapter: the mutation port carries the reply
  linkage (`in_reply_to`) and the resolve mutations; the recorded contracts
  cover both against the fixture; no live mutation outside the designated
  test repository.
- Qt: the card's action becomes Reply, Resolve appears beside it and in the
  Discuss feed (BPR-0649), and the draft editor names the thread it replies
  to.
- Owned paths: `src/core` review round trip, `src/application` mutation
  port and hosts, `src/adapters/github`, `src/adapters/qt` cards, editor and
  feed, their focused tests and the mutation contracts.

## Acceptance

- A reply published from a card appears under its thread in the recorded
  fixture's projection; Resolve and Unresolve change the thread's state
  through the port and refuse exactly the cases above.
- `application_review_mutation`, `application_review_submission_contract`,
  the recorded GitHub contracts and the Qt selectors for the cards and the
  editor pass.
