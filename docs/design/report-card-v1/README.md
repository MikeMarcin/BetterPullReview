# The agent's report: its card and its tab (round 1; C chosen)

Three ways to make the report of
[RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md)
look better in both places it appears, drawn beside what is built today. The
owner asked for them on 2026-11-14:

> I think we can make both the agent drawer report card and the file tab
> report card look a little better. Start a deep worker to give a couple
> improved front-end design mockups.

**The owner chose C, Attachment** (see [Status](#status)), and asked for a
separate round on what the opened report looks like. No product code changed
here.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames, light and dark; none
is a screen capture.

## How the frames are drawn

- Each frame is the window at its real size, measured from the owner's two
  screenshots of 2026-11-14: 1280 by 820 points, a 323 point drawer and a 631
  point pane for the tab. The colours are the roles in
  `src/adapters/qt/src/appearance_profile.cpp` and the surfaces sampled from
  those screenshots. No hue is added.
- The pull request is the fixture the owner looked at (#21, nested folders).
  The report is longer than the fixture agent's, so each design carries a code
  block, a sub-heading and a list. All four designs show the same report.
- The turn's footer (who answered, Copy, Draft, Ask more) and the rest of the
  drawer are drawn as built in every frame. Only the report changes.
- A card and a tab can be chosen from different options. They are paired by
  the idea they share.

## The options

| | The card in the drawer | The report in its tab | Distance from what is built |
|---|---|---|---|
| Current | A frame inside the reply: title, counts, three raw lines, Open report | A 220 point outline pane, two buttons above a boxed body | None |
| A, Tidy | One surface: glyph and title, counts, the summary set as prose, Open report | A quiet 176 point outline rail, a one-line bar, a typeset page | Small: the same widgets, restyled |
| B, Contents | The summary, then the sections as rows; a row opens the report there | The whole pane is the page; a jump bar holds the outline | Medium: a new row list and a jump bar |
| C, Attachment | The summary as an ordinary answer, the report attached as one row | The whole pane is the page; sections fold to their first sentence | Largest: the body is split by section |

All four cards side by side: [light](png/cards-light.png),
[dark](png/cards-dark.png).

### Current: what is built

Drawn from `src/adapters/qt/src/agent_tab_view.cpp` (the card) and
`src/adapters/qt/src/agent_report_view.cpp` (the tab).

- Drawer: [light](png/current-drawer-light.png),
  [dark](png/current-drawer-dark.png). Three outlines nest (the question, the
  reply, the report's frame). The lead is the answer's first three prose lines
  with their Markdown kept. Nothing shows that the report is open.
- Tab: [light](png/current-tab-light.png), [dark](png/current-tab-dark.png).
  The title is said four times. The outline pane takes 220 of the pane's 631
  points and stands beside the files' tree. View source and Copy source are
  full-size buttons above a boxed body. The body is drawn at the height the
  code intends, not the height the build gives it (see
  [What any option fixes](#what-any-option-fixes)).

### A, Tidy: the same card, set properly

Nothing moves; every part is set with more care.

- Drawer: [light](png/a-drawer-light.png), [dark](png/a-drawer-dark.png). The
  reply is the card, so the inner frame is gone. The agent glyph marks the
  title. The lead is the report's summary (what stands before the first
  section), rendered, held to five lines. Open report has room above it and a
  rule under it. While the report's tab is active the card wears the selection
  outline.
- Tab: [light](png/a-tab-light.png), [dark](png/a-tab-dark.png). The outline
  is a 176 point rail on the window's surface with the current section marked.
  One 30 point bar holds who answered, when and at which commit, then Rendered
  or Source and Copy. The page is typeset: a 20 point title, 15 point
  sections, 20 point leading.
- Gives: the cheapest build, and nothing to learn.
- Costs: the page stays narrow (about 55 characters a line in a default
  window), two lists still stand side by side, and the card still says only
  that a report exists.

### B, Contents: the sections in the card, a jump bar in the tab

- Drawer: [light](png/b-drawer-light.png), [dark](png/b-drawer-dark.png).
  Title and summary, then one row for each section: its name and its first
  words on one line. A row opens the report at that section; the corner button
  opens it at the top. Sub-headings stay out of the card, and more than six
  sections end in a count of the rest. While the report is open the card is
  its outline: the section being read is marked and a row scrolls the tab.
- Tab: [light](png/b-tab-light.png), [dark](png/b-tab-dark.png). No side
  pane. A jump bar under the tabs names the report and where the reader is,
  and lists every heading when pressed (drawn open under the window). View
  source and Copy source are two icons at its right. The page has the pane's
  whole width with a 560 point measure.
- Gives: a card that is useful before it is opened, about 80 characters a
  line, one list beside the files' tree, and a bar that stays one line however
  long the outline is.
- Costs: the tallest card; the tab's outline is one press away when the drawer
  shows another tab; Open report is an icon, not a labelled button; a row list
  and a jump bar to build.

### C, Attachment: a summary with the report attached; sections that fold

- Drawer: [light](png/c-drawer-light.png), [dark](png/c-drawer-dark.png). The
  summary in full, as any answer reads. Under it one row: a page marked with
  the agent glyph, the report's title, what it holds. The whole row is Open
  report, and it is selected while the report is open.
- Tab: [light](png/c-tab-light.png), [dark](png/c-tab-dark.png). No side pane
  and no bar. After the title and summary each section is a disclosure bar:
  folded, it shows the section's first sentence and its count of code
  references; open, the section. Expand all, View source and Copy source sit
  beside the title.
- Gives: a long report that is scannable at once, the shortest card with the
  summary uncut, and an outline that is the document itself.
- Costs: reading straight through takes presses or Expand all, and which
  sections start open needs a rule; the largest build (one view a section, and
  find-in-report has to open what it finds in); the card says least about what
  the report holds.

## The tab's Ask, moved

A separate change moves the Ask at the end of the tab strip to a small control
on the active file tab, with none on a report's tab. Every tab frame here is
drawn on that strip, the current design included, so the options are judged
where they will live. The strip alone, against today's:
[light](png/tabstrip-light.png), [dark](png/tabstrip-dark.png). The control's
exact shape belongs to that change.

## What any option fixes

Found while drawing the current design. Each is in the owner's screenshots or
in the code named; none depends on which option is chosen.

- **The body stops at about 190 points.** In the second screenshot the
  report's body is a short box that ends at "Merged main". `agent_report_view`
  adds its `markdown_body_view` in the default mode, not in place
  (`markdown_body_view::set_in_place`), with a stretch under it: read from the
  code, the body is then a framed area that keeps its own height and scrolls
  inside itself while the stretch takes the rest of the pane. Not yet
  confirmed by a run.
- **"4 section(s), 17 line(s)".** The counts in `agent_tab_view.cpp` have no
  plural forms, and `core::report_card_of` counts the title as a section.
- **The lead keeps its Markdown.** `report_card_of` takes the first three
  prose lines across headings and the card shows them as plain text, so
  backticks show and a section's sentence follows the summary.
- **The tab's title is cut at its start.** "…d since your review" loses the
  words that tell two reports apart.
- **The title is said four times** with a report open: the tab, above the
  outline, in the outline and in the body.

## Renders

Made offscreen by Chrome's headless mode on a throwaway profile, one frame and
theme at a time, then trimmed to the frame:

```sh
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new \
  --user-data-dir="$(mktemp -d)" --force-device-scale-factor=2 \
  --window-size=1700,1500 --hide-scrollbars \
  --screenshot=a-tab-dark.png \
  "file://$PWD/docs/design/report-card-v1/index.html?theme=dark&frame=a-tab"
```

`frame` is one of `cards`, `current-drawer`, `current-tab`, `a-drawer`,
`a-tab`, `b-drawer`, `b-tab`, `c-drawer`, `c-tab` and `tabstrip`.

## Status

**Chosen: C, Attachment** (owner, 2026-11-14). The note, verbatim:

> But you didn't properly ask me about what it should look like when we open
> the attachment so make a separate review round for that.

So C's card is chosen: the summary as an ordinary answer with the report
attached as one row. What the report looks like once that attachment is opened
is not chosen; C's tab above is one drawing of it, and
[round 2](../report-card-v2/README.md) asks it properly.

- Attended-review session `c95f9e8417c8ea6f35b145e15f81c63a`, candidate
  `report-card-v1` at revision `86dda070d240` (the commit that added these
  mockups), decision `18f04c46-bf89-4fb2-8356-f0983b3f75c4`: select
  "C, Attachment".
- External state directory
  `~/.attended-review-state/report-card-v001-2026-11-14/` (`state/` is the
  session, `material/` a copy of this folder at that revision).
- Lifecycle: handed back 2026-11-14 with the one candidate decided; the host
  is stopped.
