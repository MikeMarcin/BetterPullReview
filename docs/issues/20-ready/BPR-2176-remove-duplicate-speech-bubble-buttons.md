---
schema: "implement-rfc.card.v1"
id: "BPR-2176"
title: "Remove the review bar's Comment button and the Tour tab's Add stop button"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2176--BPR-2176--c8e0137a3d08.json", "origin": "owner, 2026-11-14, with a screenshot of both speech-bubble buttons"}
---

# BPR-2176: Remove the review bar's Comment button and the Tour tab's Add stop button

The owner:

> I feel like both of this speech bubble boxes for add comment and add tour stop are both useless duplicate ways to do what we have better ways to do. Am I wrong? Do we need them for some reason?

Asked whether to remove both: "Yes, file a card".

## What goes

- The review bar's speech-bubble split button (`CommentAction`, `composeInlineDraftAction`, with `CommentMenu`, which only repeats that one action), in `workbench_window_construction.cpp`. It sits beside Compose review, which reads "Comment…" on your own pull request, so the two have nearly the same name.
- The Tour tab header's Add stop tool (`workbenchAddTourStopTool`, comment icon), in `workbench_window_central_surface.cpp`. Expand stays.

## What stays

- Commenting: Return in the code, the gutter's +, the C key (`shortcut_command::comment`, still bound on the window), Reply in the drawer, and the code menu's Add comment.
- Tour stops: Alt+Shift+T, the Pull Request menu's Add tour stop, and the code menu's item.

## Outcome

Neither button appears. The C key and Alt+Shift+T still work, with no window action lost (tested). Nothing else in the review bar or the Tour tab header shifts. Tests and courses that look for either object name are updated. Owner look on a fixture with a tour.
