---
id: BPR-0456
title: Flatten the reply feed under its root instead of indenting it
status: Done
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

## Done — 2026-08-26

The drawer reads as a feed now. A root sits flush; every reply sits one step
in, whatever its depth.

### What was chosen, of the two the card named

The card flagged that `setIndentation(0)` flattens the drawing while leaving
expand/collapse attached to rows that no longer look nested, and asked which
the owner meant. Both halves were needed, in different places:

- `github_conversation_panel` sets `setIndentation(0)` and
  `setRootIsDecorated(false)`, so the **view** stops indenting by depth;
- `github_conversation_delegate` gives any row with a parent one fixed
  `ReplyIndent`, so the **grouping** survives that flattening.

Doing only the first would have put replies flush against their root and lost
the grouping the owner explicitly kept ("we still have a grouping by root ala
collaborator"). Doing only the second was not possible — Qt indents by depth
before the delegate ever sees the rect.

The `Reply by <author>` heading the delegate already rendered for any row with
a parent is what keeps the feed readable now that position no longer says a row
is a reply. It needed no change, which is why it is worth naming: it was
already carrying the meaning that indentation is no longer carrying.

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| the view no longer indents by depth | `ctest --preset dev -R 'qt_github_conversation_panel'` | `indentation() == 0` and no root decoration | pass |
| a five-deep reply occupies the same extent as a one-deep reply | same | `visualRect` left and width equal for both | pass |
| replies really start at the same place | same | the nearest painted pixel in each row's raster is the same inset for depth 1 and depth 5 | pass |
| a reply is still grouped under its root | same | the root's painted inset is strictly smaller than a reply's | pass |
| nothing adjacent regressed | `ctest --preset dev -R 'github_conversation\|thread_map\|workbench\|composed_github\|diff_workbench\|measured_alignment\|link_map\|comparison_scroll'` | panel, model property, thread map, three window and diff scales, composed GitHub, public-path courses, and the three geometry surfaces | 28/28 pass |
| formatting | `cmake --build --preset dev --target coding-format-check` | 425 files | pass |
| documentation | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | pass |

### The tests fail when the claim is false

- restoring the view's depth indent fails *"the view no longer indents rows by
  their depth"*;
- keeping the view flat while giving the delegate a depth-scaled inset fails
  *"replies start at the same place whatever their depth"* — the pixel
  assertion, not the settings one. That loophole is the reason the check
  measures rasters rather than reading `indentation()` and stopping.

The measurement needed two corrections of its own before it could say that. It
first sampled a single scanline at each row's mid-height and found blank space,
because a row's text does not fill its height; it now takes the nearest painted
pixel anywhere in the row. It also measured rows that were scrolled out of
view, which would have compared blank against blank forever; it now requires
each measured row to be on screen.

### Not done here

Nothing about the model. The reply edge [[BPR-0416]] carried is untouched,
[[BPR-0418]]'s map reads the same projection and is unchanged, and whole-thread
anchor filtering still keeps a root and its replies together.
