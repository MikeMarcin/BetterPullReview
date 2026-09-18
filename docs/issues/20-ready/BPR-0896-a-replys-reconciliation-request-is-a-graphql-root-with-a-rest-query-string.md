---
schema: "implement-rfc.card.v1"
id: "BPR-0896"
title: "A reply's reconciliation request is the GraphQL root with a REST query string, so a live reply can never reconcile or recover"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006 worker on BPR-0879, while adding the reply mutation's pull-request binding","found_at":"5c8ee790","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0896: A reply's reconciliation request is the GraphQL root with a REST query string, so a live reply can never reconcile or recover

## Defect

`compile_review_comment_observation_request` builds its reconciliation GET as
the mutation's own path plus `"?per_page=100"`. That is right for an ordinary
REST comment mutation, whose path is the comments collection.

**For a reply it is not.** A reply is a GraphQL mutation
(`addPullRequestReviewThreadReply`), so its path is the GraphQL root, and the
reconciliation request comes out as `GET /graphql?per_page=100` with an empty
body — a request no real server can answer meaningfully.

The consequence is that **a reply through the live session can never reconcile,
and therefore can never recover.** Reconciliation is how the session confirms
what the server actually recorded and how a restart resumes an interrupted
write. Both are unavailable for exactly one kind of mutation, and it is the one
a reviewer uses most.

This strongly suggests the shipped reply path has never been exercised end to
end against a real server: the first live reply would have produced a
nonsensical GET. It was found by reading, while [[BPR-0879]] was adding the
reply mutation's pull-request binding.

## Scope

- Give a GraphQL-rooted mutation its own reconciliation observation rather than
  deriving one from a REST path. A reply's natural observation is the thread it
  landed in, which [[BPR-0879]] already compiles and parses
  (`compile_thread_target_observation_request`, and the `pullRequest { ... }`
  selection now on the reply document) — reuse that rather than inventing a
  third shape.
- Check the same question for every other operation whose path is the GraphQL
  root. Resolve and unresolve reconcile through their own response; a reply is
  the one that goes back for a second look. Confirm that by reading, not by
  assuming this is the only case.
- Decide what a reply's restart recovery should do. `live_review_mutation_restart_recovery`
  passes today, which means it is not covering this.

## Acceptance

- A reply's reconciliation request is one a real server can answer, and the card
  names what it asks for.
- A contract row pins the reply's reconciliation shape, so a REST-derived path
  cannot come back.
- Restart recovery for an interrupted reply is either proven or recorded as
  deliberately unsupported with its reason.
- [[BPR-0879]]'s pull-request binding on the reply still holds.
