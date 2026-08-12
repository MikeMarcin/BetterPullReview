---
id: BPR-0270
title: Restore the composed conversation-host contract
status: In Progress
rfc: RFC-BPR-0005
depends_on: [BPR-0191]
---

# Restore the composed conversation-host contract

The complete macOS `dev` matrix fails each of the 100, 125, and 200 percent
composed GitHub Qt courses at the same 640-pixel assertion. The conversation
panel is constructed for the conversations drawer, but the accepted shell now
places it with the review editor, agent surface, and defect controls inside a
drawer-owned content container. `QLayout::addWidget` therefore reparents the
panel to that container, while the test still requires the drawer itself to be
the immediate parent.

## Required final state

- The composed contract proves the threaded conversation remains inside the
  conversations drawer's ownership tree at the 640-pixel minimum and remains
  beside the comparison at 960 pixels.
- The assertion distinguishes a drawer descendant from a widget reparented
  outside the drawer; it does not require an obsolete immediate-parent shape.
- The files drawer, conversations drawer, conversation focus stop, review
  editor, agent surface, and defect controls retain their accepted composition.
- The 100, 125, and 200 percent courses pass from a rebuilt target without a
  production layout or core drawer-policy change.

## Non-goals

- Do not revise the accepted BPR-0191 route-two design, add responsive
  reparenting, or move the conversation outside the conversations drawer.
- Do not weaken the exact 960-pixel side-by-side, focus, accessibility,
  disclosure, selection-retention, or incremental-model assertions.
- Do not execute a live target, use a credential, or claim Windows/Linux
  runtime evidence.

## Verification

- Rebuild the composed GitHub Qt target and run all three scale selectors.
- Run the conversation-panel, workbench-window, drawer, and architecture
  boundary contracts.
- Run repository format/tidy, documentation, diff, and the final macOS `dev`
  continuation.
- Review the final assertion and its hostile outside-drawer counterexample
  independently.
