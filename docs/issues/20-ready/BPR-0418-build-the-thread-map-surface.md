---
id: BPR-0418
title: Build the thread map surface
status: Ready
rfc: RFC-BPR-0005
depends_on: [BPR-0416, BPR-0417]
---

# Build the thread map surface

The view the owner chose on 2026-08-21: a map of one thread's reply structure,
with the path to where you are visible, and one reply in focus. The accepted
mockup is
`docs/verification/reviews/conversation-view-mockups-v001/mockups/03-treeverse.svg`.

## What the owner asked for beyond the mockup

Verbatim, on the verdict:

> I prefer this view, it still needs to show the full branch that you clicked
> on, or at least up to the noce you selected. I think this could use some more
> iteration but that can come after experience.

So the mockup's highlighted root-to-focus path is the **floor**, not the design.
The full branch that was clicked must be visible; root-to-selected-node is the
minimum acceptable reading. And iteration after use is expected — this card
should ship a first version and learn, not hold open pursuing a final shape
nobody has used yet.

## Known before starting

- The map legends a per-node read/unread state that **nothing carries**.
  `github_conversation_projection` has no read flag, and the review ledger's
  unread notion is a per-conversation cursor, not a per-comment one. Either
  build that state or drop the distinction from the view; do not fake it.
- `RFC-BPR-0005:58` requires that comments "remain in the right drawer" and
  "never become inline full-width rows in the code view". A map surface is
  compatible with that; check any placement decision against it.
- Depth exists only once [[BPR-0416]] lands and [[BPR-0417]] supplies content.

## Verification

A rendered check is required. This is a visual and interaction claim, and a
passing signal test would not show that the branch is actually visible or that
the focused node is the one clicked.

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'github_conversation|workbench'
```
