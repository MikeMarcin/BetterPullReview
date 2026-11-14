# The straight read, with a menu of sections (round 4 mockups, no choice made yet)

How an agent's report
([RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md))
reads in its file tab. [Round 1](../report-card-v1/README.md) chose the card,
[round 2](../report-card-v2/README.md) fixed the place (the file tab only), and
[round 3](../report-card-v3/README.md) ended with the owner naming the reading
on 2026-11-14:

> Can we get like straight read with a navigation hamburger menu or something
> like that.

So the report opens with everything open and reads from the top. There is no
ruler and nothing folds. One button opens a list of the report's sections;
choosing one scrolls to it, and the list marks the section being read. This
round asks only where that button is and what its list is. Nothing here is
chosen and no product code changed.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames, light and dark; none
is a screen capture.

## The same in all three

- The report is a tab in the files' strip, and the whole report is open.
- The review bar's file controls are drawn off while it is active, and there
  is no Ask on the report's tab.
- The chosen card's attachment row is selected while the tab is active:
  [light](png/start-light.png), [dark](png/start-dark.png).
- The page is one 572 point column, and the report is the long one from rounds
  2 and 3.

No variant changes anything the RFC decided.

## The variants

Each has two frames: the tab as it opens with the menu closed, and the tab
scrolled into Must fix with the menu open.

| | The button | Names the current section | Its list | Marks the current section with | Back to the conversation |
|---|---|---|---|---|---|
| A, Section name | At the left of a slim bar over the page | Yes: its label is the section being read | A menu that drops from the button: every heading, one line each | A tick | Show in conversation, a button at the bar's right |
| B, Contents | At the left of a slim bar over the page | No: it always reads Contents | A popover: each section with its first sentence, sub-sections under it | A blue bar and a tint | The popover's first row; and the question, under the title |
| C, Floating | Floats at the page's top right; there is no bar | No: three lines, no words | A popover: every heading, one line each, then the report's other actions | A blue bar and a tint | A row in the popover; and the question, under the title |

### A, Section name: the button says where you are

- As it opens: [light](png/a-open-light.png), [dark](png/a-open-dark.png).
  A 32 point bar over the page. Its button reads Summary, because that is what
  is on screen, and changes as the reader scrolls. Show in conversation is at
  the bar's right, with View source and Copy source as two icons.
- The menu open: [light](png/a-menu-light.png), [dark](png/a-menu-dark.png).
  Scrolled into Must fix, the button reads Must fix. Pressed, it lists every
  heading, sub-sections indented, with a tick on the one at the top of the
  page. Choosing a heading scrolls to it and closes the menu.
- Gives: the reader always knows which section they are in; it is the standard
  pop-up button, with nothing to learn; the way back is always in view.
- Costs: the bar takes 32 points; the list is names only; the button's width
  changes as its label does.

### B, Contents: a popover that says what each section holds

- As it opens: [light](png/b-open-light.png), [dark](png/b-open-dark.png).
  The same bar, holding only the Contents button, View source and Copy source.
  Under the title the question is a link back to the conversation.
- The popover open: [light](png/b-menu-light.png), [dark](png/b-menu-dark.png).
  It hangs from the button: Show in conversation first, then each section with
  its first sentence and its sub-sections under it. The heading being read has
  a blue bar and a tint. It closes on a choice, on Esc and on a press outside.
- Gives: the list is a small digest; the button never changes; the
  conversation is one press away from anywhere, in the same list as the
  sections.
- Costs: nothing says which section the reader is in until the popover is
  open; the bar takes 32 points for one button and two icons; the popover is
  tall for a long report and is a new part, where A's menu is not.

### C, Floating: one button, no bar

- As it opens: [light](png/c-open-light.png), [dark](png/c-open-dark.png).
  No bar. One small button floats at the page's top right and stays there as
  the reader scrolls. Under the title the question is a link back to the
  conversation.
- The popover open: [light](png/c-menu-light.png), [dark](png/c-menu-dark.png).
  Every heading on one line, the one being read marked, then Show in
  conversation, View source and Copy source.
- Gives: the most page (no bar); one place for everything the report can do;
  the button is in the same spot at any scroll position.
- Costs: the button sits over the page, so a wide code block or a long line
  passes under it; three lines say nothing about where the reader is; View
  source and Copy source take two presses.

## Renders

Made as the earlier rounds' were: Chrome's headless mode on a throwaway
profile, one frame and theme at a time (`?theme=dark&frame=a-menu`), then
trimmed to the frame. `frame` is one of `start`, `a-open`, `a-menu`, `b-open`,
`b-menu`, `c-open` and `c-menu`.

## Status

Awaiting the owner's choice, asked on 2026-11-14 as one select question (A, B
or C) with a note:

- Attended-review session `e9bcad5b1ed74517e12673a2b445ec3c`, candidate
  `report-card-v4-menu` at revision `5c095e49f988` (the commit that added
  these mockups).
- External state directory
  `~/.attended-review-state/report-card-v004-2026-11-14/` (`state/` is the
  session, `material/` a copy of the four rounds' folders at that revision
  for the owner to open).
- Lifecycle when this was written: prepared and served, unanswered.
