---
id: BPR-0416
title: Carry the reply edge into the conversation tree
status: Done
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

## Done — 2026-08-21

The tree now carries the reply edge, and the drawer nests a chain to whatever
depth GitHub reports.

### The card's own premise was half wrong, and the test found it

This card listed three mapping cases to answer: no `ReplyTo`, a `ReplyTo` naming
a comment outside the thread, and a cycle. A fixture was written for all three
and the projection then refused to happen at all.

`src/application/include/bpr/application/github_read_model.hpp:1416-1434`
already validates the relationship. It accumulates the comment identities it has
seen *as it iterates a thread*, and returns `invalid_relationship` for any
`ReplyTo` that does not name one of them. So a target outside the thread is
rejected, a forward reference is rejected, and therefore **a cycle cannot be
constructed at all** — it needs a forward reference to close. A thread's first
comment cannot carry a `ReplyTo` either, because nothing precedes it.

Two of the three cases are unreachable through the validated path, and the
fixture that claimed to exercise them was itself invalid. The mapping keeps its
fallbacks — it is a pure function and should not assume its caller validated —
but they are belt-and-braces, and this card no longer claims host-level coverage
for them. The reachable guarantee is the one now tested: a valid chain nests.

Cycle safety at the **Qt model** is a different matter and is genuinely
reachable, because the model takes whatever projection a caller hands it. The
property test proves it: it generates arbitrary parent graphs, and it failed
until the model stopped dropping unreachable rows.

### Two defects the property test exposed

Neither was in the plan; both are in the shipped model rather than the tests.

**The model silently dropped rows on a cyclic parent chain.** It builds children
by parent identity and attaches only what descends from a root, so every node in
a cycle was constructed and then attached to nothing. It now promotes such nodes
to roots — an unexpected shape costs nesting, never a row.

**The first fix for that was nondeterministic.** Breaking the cycle in place, by
resetting parents while iterating, made the resulting tree depend on
`unordered_map` iteration order: the same input could produce two different
trees, and the model and its independent expectation model disagreed about which
node became the root. That is worse than the defect it fixed. The decision is
now a read-only pass over the parent map, so it is order-independent by
construction.

### One invariant was corrected rather than weakened

`preserves_persistent_index` asserted that a node whose own parent identity is
unchanged keeps its persistent index. That was true only because nothing could
sit between a node and the top: with a two-level tree, "my parent is unchanged"
and "my ancestry is unchanged" are the same statement. They are not the same at
depth, because the model rebuilds a subtree when a node is reparented and every
index beneath it dies with it. The predicate now walks the whole ancestor chain,
which is the exact guarantee the model provides, and it still has teeth — it
gates the role-coverage checks that follow it.

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| a reply nests under the comment it replied to | `ctest -R '^application_github_host$'` | asserts `deepReply.ParentId == threadReply.Id` through the host's own projection | pass |
| the change is load-bearing | same, with `review_comment_parent` stubbed to return the thread root | the exact assertion fails; build exit 0 in both directions, so it is not a stale binary | **fails without the fix** |
| the model nests to any depth | `ctest -R '^qt_github_conversation_model_property$'` | the tree walk recurses instead of asserting `rowCount(child) == 0` | pass, 12.9 s |
| no row is lost to a cyclic parent chain | same | "every unique nonempty identity is reachable exactly once" over generated parent graphs | pass, and it failed before the model fix |
| nothing adjacent regressed | `ctest -R 'github_conversation\|application_github_host\|composed_github\|qt_composed'` | the panel, window composition, and three rendering scales | 6/6 pass |
| formatting | `cmake --build --preset dev --target coding-format-check` | 388 files | pass |

### Not done here

No map surface, no read state, no fixture with recorded depth — [[BPR-0417]] and
[[BPR-0418]]. The drawer will now nest wherever GitHub reports a chain, which is
a visible change to the shipped drawer and the intended consequence of carrying
the real edge.
