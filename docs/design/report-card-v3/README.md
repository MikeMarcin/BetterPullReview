# The opened report, in the file tab (round 3 mockups, no choice made yet)

How an agent's report
([RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md))
reads once its attachment is opened. [Round 1](../report-card-v1/README.md)
chose the card. [Round 2](../report-card-v2/README.md) drew the opened report
in a widened drawer and a preview as well as the file tab, and the owner
rejected it on 2026-11-14:

> Gross, no. They should all be in the file tab that was already decided upon.

This round keeps the place fixed and asks only the reading: what is folded
when the report opens, how the reader moves between sections, and how they get
back to the conversation. Nothing here is chosen and no product code changed.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames, light and dark; none
is a screen capture.

## Fixed in all three

- The report is a tab in the files' strip, marked with the agent glyph (Q8).
- The review bar's file controls are drawn off while it is active (Q9).
- There is no Ask on the report's tab.
- The card is the one round 1 chose, and its attachment row is selected while
  the report's tab is the active one:
  [light](png/start-light.png), [dark](png/start-dark.png).
- The report is round 2's long one: a review of #21 with a summary, six
  sections, four sub-sections, two code blocks, eleven code references and two
  suggested changes. A suggested change is its lines as a diff with Add as
  draft comment under it; nothing is published.

No direction changes anything the RFC decided.

## The directions

Each has two frames: the tab as it opens, and the tab a moment into reading.

| | As it opens | What is folded | Moving between sections | Back to the conversation |
|---|---|---|---|---|
| A, Outline | The summary, then every heading with its first sentence | Every body. A press on a heading opens it in place | The headings themselves: Up and Down, Right and Left, Expand all | The question, as a link under the title |
| B, Straight read | The top of the document, all of it open | Nothing | The bar at the top names the section; a ruler down the right edge has a tick for every heading | Show in conversation, a button that stays in the bar |
| C, One section at a time | The first page: the summary and the contents | Every section but the one on the page | Previous and next in the bar, a menu of sections, and the neighbours by name at the foot of the page | Conversation is the page before the first, and the first row of the menu |

### A, Outline: it opens as its headings

- As it opens: [light](png/a-open-light.png), [dark](png/a-open-dark.png).
  Every heading is a disclosure bar, at both levels, and every body is folded
  to its first sentence. A bar that holds suggested changes says how many. The
  whole report is on one screen.
- Reading: [light](png/a-read-light.png), [dark](png/a-read-dark.png). One
  press on a heading opens its body in place; everything else keeps its place.
  Up and Down move between bars, Right and Left open and close them as in the
  files' tree, Option-click opens a bar and everything under it, and Expand all
  is beside the title. The tab remembers what was opened while it lives.
- Back: under the title the question is a link that shows the card in the
  Agent tab.
- Gives: the whole report at first sight, with where the work is; an outline
  that is the document; no bar and no side list.
- Costs: reading from top to bottom takes a press a heading or Expand all
  first; Find has to open what it finds in; the body becomes one view for each
  heading.

### B, Straight read: nothing folds

- As it opens: [light](png/b-open-light.png), [dark](png/b-open-dark.png).
  The title, who answered, the summary and the first section, all open. A bar
  at the top names the section the reader is in and its place (Summary,
  Section 1 of 7). A ruler down the right edge has a long tick for each section
  and a short one for each sub-section, with a band for what is on screen.
- Reading: [light](png/b-read-light.png), [dark](png/b-read-dark.png).
  Scrolled into Must fix: the bar names it and the blue ticks mark the place. A
  press on a tick goes there, and pointing at the ruler lists the headings (the
  strip under the window). Option-Up and Option-Down move a section at a time.
- Back: Show in conversation stays in the bar wherever the reader is.
- Gives: it reads like a document and Find works as it does anywhere; the
  ruler shows the report's shape and the reader's place in sixteen points; the
  way back is always in the same place.
- Costs: a long report is a long scroll and nothing says what a section holds
  until it is reached; the bar takes thirty points of the pane; the ruler is a
  new part.

### C, One section at a time

- As it opens: [light](png/c-open-light.png), [dark](png/c-open-dark.png).
  The first page: the title, the summary and one row for each section with its
  first sentence. A row goes to that section. The bar has previous and next,
  the page's name and its place.
- Reading: [light](png/c-read-light.png), [dark](png/c-read-dark.png). One
  section fills the page. The foot names the pages before and after. The
  page's name in the bar is a menu of every section (the strip under the
  window). The paging is all in the pane; the review bar stays off.
- Back: on the first page, previous is Conversation, and the menu lists
  Conversation first.
- Gives: each section is one screen; stepping through is one press and the
  page says where it leads; the way back is before the first page.
- Costs: a narrative report is cut into pages it was not written for; a section
  longer than the pane still scrolls; from a late page the conversation is the
  menu's first row, not one press of previous.

A and C open alike: both first show the summary and a list of sections. They
differ in what a press does. In A the section opens where it is and the rest
stay around it; in C the section replaces the page. B is the only one that
opens on the report's own words.

## Renders

Made as the earlier rounds' were: Chrome's headless mode on a throwaway
profile, one frame and theme at a time (`?theme=dark&frame=a-open`), then
trimmed to the frame. `frame` is one of `start`, `a-open`, `a-read`, `b-open`,
`b-read`, `c-open` and `c-read`.

## Status

Awaiting the owner's choice, asked on 2026-11-14 as one select question (A, B
or C) with a note:

- Attended-review session `bff4dd72e9f6b027789ae5c1da4eeefe`, candidate
  `report-card-v3-reading` at revision `629e69ca7374` (the commit that added
  these mockups).
- External state directory
  `~/.attended-review-state/report-card-v003-2026-11-14/` (`state/` is the
  session, `material/` a copy of the three rounds' folders at that revision
  for the owner to open).
- Lifecycle when this was written: prepared and served, unanswered.
