---
schema: "implement-rfc.card.v1"
id: "BPR-2176"
title: "Remove the review bar's Comment button and the Tour tab's Add stop button"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2176--BPR-2176--c8e0137a3d08.json", "origin": "owner, 2026-11-14, with a screenshot of both speech-bubble buttons"}
acceptance: [{"criterion":"outcome","outcome":"pass","revision":"612194781","evidence":{"kind":"command","detail":"qt_workbench_window_scale_{100,125,200} (test_the_duplicate_comment_and_add_stop_buttons_are_gone); owner Accept (BPR-2176-buttons-gone) in look ask-v002 on conventional-comments-v1 with a tour"}}]
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

## Progress (2026-11-14)

Built test first: `test_the_duplicate_comment_and_add_stop_buttons_are_gone`
(in `qt_workbench_window_scale_{100,125,200}`) failed on the review bar's
Comment button, then passed. Gone: the review bar's Comment split button and
its `commentMenu`, and the Tour tab header's Add stop tool. Kept and tested:
Option-C (`composeInlineDraftAction`, Alt+C, a window shortcut) and
Option-Shift-T (`addTourStopAction`), both in the Pull Request menu, which now
names Comment's target when it opens. The review-bar order, presenter and
geometry checks follow (18 actions, Compose review last). The owner's look on a
fixture with a tour is in session ask-v002.

## Owner look ask-v002 (2026-11-14)

`BPR-2176-buttons-gone` **Accept** (frozen from `612194781`, on
conventional-comments-v1 with its tour). Done.
