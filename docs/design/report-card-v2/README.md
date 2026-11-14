# The opened report (round 2 mockups, no choice made yet)

[Round 1](../report-card-v1/README.md) chose the card for an agent's report
([RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md)):
the summary as an ordinary answer with the report attached as one row. The
owner chose it on 2026-11-14 with this note:

> But you didn't properly ask me about what it should look like when we open
> the attachment so make a separate review round for that.

This round asks that. Three directions for what the reviewer sees when they
open the attachment, each drawn in the file tab, and in another place where it
suits one. Nothing here is chosen and no product code changed.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames, light and dark; none
is a screen capture.

## How the frames are drawn

- Each frame is the window at its real size (1280 by 820 points; the tab's
  pane is 631 points) in the application's own colour roles, as in round 1.
- The report is a long one: a review of #21 with a summary, six sections,
  four sub-sections, two code blocks, eleven code references and two suggested
  changes. All three directions show the same report.
- A suggested change is drawn the same way in all three: its lines as a diff,
  the reference to where they go, and Add as draft comment, the action the
  Agent tab's suggested comments already have. Nothing is published.
- The starting point, the chosen card at rest and with its report open:
  [light](png/start-light.png), [dark](png/start-dark.png).

## The directions

| | Where it opens | Which sections start open | Moving between sections | Back to the card |
|---|---|---|---|---|
| A, Digest | A file tab | None: each folds to its first sentence. A report that fits the pane opens whole | The disclosure bars: Up and Down, Right and Left, Expand all | The question, as a link under the title |
| B, Straight read | In the drawer, widened, beside the code; Open in a tab moves it | All. Nothing folds | The heading that stays at the top, and a ruler of sections down the right edge | Conversation, at the reader's top left; in the tab, Show in conversation |
| C, One section at a time | A preview over the window (Space, as Quick Look); Open in a tab keeps it | One: the page you are on | The review bar's Section 2 of 7 and its keys; a list in the preview | Space or Esc from the preview; in the tab, Show in conversation |

The reading (how sections fold and how you move between them) and the place
(tab, drawer, preview) can be chosen from different directions.

### A, Digest: it opens folded

Round 1's drawing of C, asked properly.

- Tab: [light](png/a-tab-light.png), [dark](png/a-tab-dark.png). The window
  shows the tab after two presses; the strip under it shows the tab as it
  opens.
- Every heading is a disclosure bar, at both levels. A folded bar shows its
  first sentence, and a bar that holds suggested changes says how many.
- A report that fits the pane opens whole; a longer one opens folded. The tab
  remembers what was opened while it lives.
- Up and Down move between bars, Right and Left open and close them as in the
  files' tree, Option-click opens a bar and everything in it, and Expand all is
  beside the title.
- Under the title the question is a link that shows the card in the Agent tab.
- Gives: the whole report in one screen at first sight, with where the work is;
  an outline that is the document; no bar and no side list.
- Costs: reading from top to bottom takes a press a section or Expand all
  first; Find has to open what it finds in; the body becomes one view for each
  heading.

### B, Straight read: nothing folds, and it opens beside the code

- In place: [light](png/b-drawer-light.png), [dark](png/b-drawer-dark.png). A
  press on the attachment widens the Agent drawer from 323 to 560 points and
  the report takes the conversation's place. No tab is opened, so a code
  reference moves the code while the report stays put. Conversation goes back
  to the card; Open in a tab moves the report beside the files. The composer
  stays under the report and asks about it.
- Tab: [light](png/b-tab-light.png), [dark](png/b-tab-dark.png). The same read
  with the pane's whole width. The section's heading stays at the top with its
  place in the report. A ruler down the right edge has a long tick for each
  section and a short one for each sub-section, a band for what is on screen
  and blue ticks for where the reader is. A press on a tick goes there, and
  pointing at the ruler lists the headings (the strip under the window).
  Option-Up and Option-Down move a section at a time.
- Gives: it reads like a document and Find works as it does anywhere; in the
  drawer the report and the code are on screen together; the ruler shows the
  report's shape in sixteen points.
- Costs: a long report is a long scroll; the widened drawer leaves the code
  about 400 points in a default window; two places to read it; the ruler is a
  new part.

### C, One section at a time: preview it, then keep it

- Previewed: [light](png/c-lightbox-light.png),
  [dark](png/c-lightbox-dark.png). Space on the attachment, or a press, opens
  the report over the window, dimmed from under the title row as the
  compose-review lightbox is. A list of sections with their first words is at
  the left and the chosen section's page at the right. Space, Esc or a press
  outside closes it. A code reference, or Open in a tab, turns the preview into
  a tab.
- Tab: [light](png/c-tab-light.png), [dark](png/c-tab-dark.png). One section
  fills the page. The review bar's first slot is live and reads Section 2 of 7,
  and the keys that step between files step between sections. The page ends
  with the sections before and after by name. Contents goes to the first page
  (the strip under the window): the summary and one row for each section.
- Gives: each section is one screen; stepping through is the key already used
  for files; the preview makes no tab until asked; the pane needs no bar of its
  own.
- Costs: a narrative report is cut into pages it was not written for; a section
  longer than the pane still scrolls; the most to build.

## The same in all three

- **No Ask on the report's tab.** The tab strip's small Ask belongs to file
  tabs. A question about the report is the card's Ask more, or the drawer's
  composer.
- **The card and the report point at each other.** The attachment row is
  selected while the report is open, and the report has one way back.
- **A code reference** opens that file's tab at the line. In a tab the report's
  tab stays beside it, one press back, as the RFC says.
- **View source and Copy source** are two small icons, not the two buttons
  built today.

## What a choice would change in RFC-BPR-0128

- A changes nothing the RFC decided.
- B's drawer is a second place to read a report. Q8 chose the file tab over the
  lightbox and a window of its own; B keeps the tab and adds the drawer before
  it. B's tab alone changes nothing.
- C's preview reopens Q8 in the same way, and its review bar changes Q9, which
  draws every file control off while a report's tab is active. C's paging
  without either changes nothing.

## Renders

Made as round 1's were: Chrome's headless mode on a throwaway profile, one
frame and theme at a time (`?theme=dark&frame=a-tab`), then trimmed to the
frame. `frame` is one of `start`, `a-tab`, `b-drawer`, `b-tab`, `c-lightbox`
and `c-tab`.

## Status

Awaiting the owner's choice.
