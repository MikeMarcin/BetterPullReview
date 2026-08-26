---
id: BPR-0456
title: Flatten the reply feed under its root instead of indenting it
status: In Progress
rfc: RFC-BPR-0005
depends_on: []
---

# Flatten the reply feed under its root instead of indenting it

Owner direction from the [[BPR-0414]] verdict on 2026-08-25, verbatim:

> The replies need not be indented, that takes too much space, just a straight
> linear reply feed ala twitter is better, we have the treverse view if things
> get complicated. Not literally twitter feed, we still have a grouping by root
> ala collaborator.

## What is being asked for, and what is not

Asked for: within one thread, replies read as a **linear feed** rather than a
staircase. Grouping stays — roots still group their replies, SmartBear
Collaborator style. What goes is the per-depth horizontal indent, which spends
drawer width that the comment text needs.

Not asked for: removing depth from the model. The reply edge [[BPR-0416]]
carried stays exactly as it is. This is a presentation change in the drawer
only.

This also settles something [[BPR-0211]] left open. The owner's reason for
accepting a flat feed is *"we have the treverse view if things get
complicated"* — the thread map is what makes flattening safe, which is the
first evidence from use that the map earns its place. Record that on
[[BPR-0211]] rather than losing it here.

## Known before starting

- `github_conversation_panel.cpp:226` sets `View->setRootIsDecorated(true)` on a
  `QTreeView`, and Qt indents by depth from there. Turning decoration off is not
  the same as removing indentation, and `setIndentation(0)` on a tree flattens
  the *drawing* while leaving expand/collapse semantics attached to rows that no
  longer look nested. Decide deliberately which of those the owner is asking
  for.
- `github_conversation_delegate.cpp:82` already renders `Reply by <author>` for
  any row with a parent, which is what keeps a flat feed readable — a reply
  still says it is a reply. Check it reads correctly once depth is not carried
  by position.
- The drawer must keep whole-thread filtering by anchor. Flattening presentation
  must not flatten the *forest*: two roots on the same file are still two
  groups, not one feed.
- [[BPR-0418]]'s thread map reads the same model and is unaffected. Verify that,
  do not assume it.

## Required final state

- Replies under one root read as a linear feed at a readable width.
- Root grouping remains visible and unambiguous with several threads shown.
- A reply is still identifiable as a reply, and to whom.
- The map, the anchor marker, exact line filtering, and selection
  synchronisation are unchanged.

## Verification

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'github_conversation|workbench|thread_map|composed_github'
```

A rendered check is required, at a drawer width the owner would actually use.
