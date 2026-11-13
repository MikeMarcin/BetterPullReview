# Discuss on a busy pull request

The owner's chosen design for [BPR-2171](../../issues/30-in-progress/BPR-2171-discuss-reads-cleanly-on-a-busy-pull-request.md),
2026-11-13: option **B** of [index.html](index.html), with the decisions
below. The page holds the findings (with code citations), today's state and
options A, B and C. `png/` holds headless renders of the HTML frames in
`html/`, light and dark; `current/` holds offscreen renders of the build of
that morning. The frames are reconstructions with invented content.

## The owner's words, verbatim

> Can we update the design of the discussion tab a little. It's just a bit
> noisy on a busy pr.

> it's only the collapsed cards with formatting issues, on a full screen
> window it just looks like a mess

> I think I like B although I dunno how you detect bots, and I think treating
> bot comments differently should be a quick filter at the top not always
> true. Do we need the ability to mark a user as a bot and store it as a repo
> setting?

On the column width: "should be a setting, Centered I think." On threads
under a review: "yes it needs them" (their author name and avatar).

## What B is, as decided

- **Collapsed preview.** The first paragraph as words; Markdown marks
  removed; one quiet word per kind of block that is not words (List, Quote,
  Image, Table, Details, Suggestion, or the code's language), in order. An
  ellipsis only when words were left out. Review's Comments and Overview's
  Comments share it.
- **Column.** Discuss's list is a centred column, 820 points by default; its
  width is a setting. The header's chips and counts share its edges. Replies,
  new count and resolved check keep fixed slots. An open card may grow wider
  than the column for code blocks and suggested changes.
- **Files once.** A file is named once over its threads; each thread carries
  its line number. Threads under a review keep their author's name and
  avatar (the frames of B leave them out; the owner overrode that).
- **Folding.** A file's resolved threads fold into a count on its row, which
  opens them. Runs of bot comments fold to one row, but only while the
  **Fold bots** quick filter is on: off by default, beside Hide resolved,
  which stays. The frames' Folded menu is not built.
- **Bots.** GitHub's author type decides automatically (an App's login,
  held as `<name>[bot]`). Any author can be marked "Mark as bot" or
  "Not a bot"; the mark is kept per repository on this computer.
- **Scope.** The "Pull request | File" choice becomes one toggle, **Only this
  file**.
