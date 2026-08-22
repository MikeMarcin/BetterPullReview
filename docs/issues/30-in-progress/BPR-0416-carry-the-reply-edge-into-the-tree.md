---
id: BPR-0416
title: Carry the reply edge into the conversation tree
status: In Progress
rfc: RFC-BPR-0005
depends_on: [BPR-0413]
---

# Carry the reply edge into the conversation tree

The first slice of the Treeverse map the owner chose on 2026-08-21. Nothing
visual happens here: this is the data foundation the map needs, and on its own it
already deepens the shipped drawer wherever GitHub actually gives a reply chain.

## Why this is not the one-line change it looks like

[[BPR-0413]] originally costed this as one line and an independent review
falsified that before the page reached the owner. Both halves are required, and
the first without the second makes the product worse:

- `src/application/src/github_projection_mapping.cpp:175` writes
  `.ParentId = threadId` for every comment, discarding `comment.ReplyTo`, which
  the read contract already requests and validates.
- `src/adapters/qt/src/github_conversation_model.cpp:317-326` accepts a parent
  **only** when the parent's `Kind` is `review_thread`. Carry `ReplyTo` through
  the mapping alone and every reply is rejected here, promoted to a root, and
  the drawer flattens from two levels to one.

Two tests mirror the current rule and must move with it:
`tests/adapters_qt/github_conversation_model_property_tests.cpp:101-113`
reimplements the parent-kind check as its expectation model, and
`tests/application/github_application_host_tests.cpp:1573-1590` asserts
`threadReply.ParentId == thread.Id`.

## The mapping resolves the edge; the model just stops refusing it

`ReplyTo` is resolved in the mapping, against the comments of the thread being
projected, so the projection is self-consistent before any view sees it. Three
cases have to be answered there rather than left to the model:

- **No `ReplyTo`.** A thread's first comment carries `replyTo: null` — confirmed
  in `tests/fixtures/github/contracts/v1/github-com/responses/review_threads.json`.
  Its parent stays the thread root, so every chain still terminates at the thread.
- **`ReplyTo` naming a comment that is not in this thread.** Deleted, paginated
  out, or simply absent. Fall back to the thread root. Left to the model this
  becomes a top-level row floating outside its own thread, which is the silent
  flattening again in a different disguise.
- **A cycle.** Two comments replying to each other resolve within the thread and
  never reach the root, so both would disappear from the tree entirely — the
  model builds children by parent id and only attaches what descends from a
  root. Walk the chain, bound the walk, and fall back to the thread root if it
  does not terminate.

The model's change is then narrow: accept a parent whose `Kind` is
`review_comment` as well as `review_thread`.

## Required final state

- A reply's `ParentId` is the comment it replied to, when that comment is in the
  same thread; otherwise the thread root.
- No comment is ever dropped from the tree, whatever GitHub returns.
- The Qt model nests a chain of arbitrary depth, and the two tests that encoded
  the two-level shape assert the new one.
- New coverage for all three mapping cases and for a chain at least three deep.
- The delegate's existing `Reply by @author` heading still reads correctly at
  depth, because it asks whether the row has a parent.

## Non-goals

No map surface, no read state, no fixture with recorded depth. [[BPR-0417]] and
[[BPR-0418]] own those. This card changes no GraphQL request: `replyTo` is
already fetched.

## Verification

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'github_conversation|github_application_host|github_projection'
cmake --build --preset dev --target coding-format-check
```
