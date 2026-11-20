# Build spec: the agent's report, its card and its tab

What to build from the owner's two choices of 2026-11-14 for the report of
[RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md):

- **The card:** C, Attachment, from [round 1](../report-card-v1/README.md).
- **The opened report:** a straight read in the file tab with C, Floating,
  from [round 4](README.md).

The frames are the reference for how it looks. Sizes below are the drawings'
points; the frames win where the two differ.

| Part | Frames |
|---|---|
| The card as chosen in round 1, at rest and with its report open | [light](../report-card-v1/png/c-drawer-light.png), [dark](../report-card-v1/png/c-drawer-dark.png) |
| The card with round 4's report | [light](png/start-light.png), [dark](png/start-dark.png) |
| The report as it opens | [light](png/c-open-light.png), [dark](png/c-open-dark.png) |
| The report scrolled, its popover open | [light](png/c-menu-light.png), [dark](png/c-menu-dark.png) |

Each line below is marked **Chosen** (it is in a frame the owner chose) or
**Proposed** (written in a caption or README by the designer and never asked
on its own). Build the chosen lines as drawn. Treat a proposed line as the
default, and raise it if it costs more than it looks.

## Not changed

- The report is a tab in the files' strip, marked with the agent glyph (Q8),
  and the review bar's file controls are drawn off while it is active (Q9).
- There is no Ask on the report's tab. A question about the report is the
  card's Ask more or the drawer's composer.
- The turn's footer (who answered, Copy, Draft, Ask more, the time) stays as
  built. So does which answers are reports (`core::is_report_answer`).
- No hue is added. The agent accent stays on the glyph only.

## The card (`agent_tab_view.cpp`, `agentTurnReportCard`)

| | | |
|---|---|---|
| Chosen | Surface | The reply card is the only surface. Remove the inner frame, the bold title line, the counts line and the Open report button |
| Chosen | Summary | The report's summary, rendered as an answer is (code as code, references as links), in full, at the answer's size |
| Chosen | Attachment row | Under the summary, 9 points down: a row on the window's surface colour with a one point border, 7 point corners, 7 by 8 padding. Left to right: a 24 by 30 page icon carrying the agent glyph, then the report's title in semibold (two lines at most) over a 12 point secondary line, then a chevron |
| Proposed | Row's second line | Build "Report, N sections", round 1's wording. Rounds 2 to 4 drew "6 sections, 2 suggested changes". Neither wording was asked on its own, and the second needs a count the core does not have |
| Chosen | The row is the action | The whole row is Open report: a press opens the report's tab, or goes to it when it is already open |
| Chosen | Selected | While the report's tab is the active tab, the row has a 1.5 point selection outline and the selection tint |
| Chosen | Footer | A rule, then the turn's footer as built |
| Proposed | Keyboard and name | The row is one focus stop; Space and Return press it. Its accessible name is "Open report: " and the title (AC08) |

The summary is what stands between the title and the first section heading.
`core::report_card_of` today returns the first three prose lines wherever they
are; it needs to return that paragraph instead, and to count sections without
the title. **Proposed:** when a report has no prose before its first section,
the summary is the first paragraph after the title wherever it stands.

## The opened report (`agent_report_view.cpp`)

| | | |
|---|---|---|
| Chosen | Layout | Remove the side pane (the glyph, the title and the outline list) and the View source and Copy source buttons. The page fills the pane on the code background |
| Chosen | Column | One column, centred, as wide as Settings › Review › Reading width allows, or Reports under its Advanced (572 points at most until [[BPR-2299]]). The title starts 22 points under the tab strip |
| Chosen | Everything open | The whole report is laid out and reads from the top. Nothing folds, there is no ruler and no bar |
| Chosen | Title | The report's title once, at 22 points bold |
| Chosen | Line under the title | The question as a link with a speech-bubble icon, then who answered, how long ago and at which commit, at 12 points in the secondary colour |
| Chosen | Type | Summary and body at 13 on 20. A section at 15 semibold with a rule over it. A sub-section at 14 semibold. Code in a quiet box at 12 on 18. A code reference as the Agent tab's links are |
| Chosen | Suggested change | A box: a header with "Suggested change" and the reference, the lines as a diff on the added and removed colours with their line numbers, and Add as draft comment with "Nothing is published" beside it |
| Proposed | Suggested change, behaviour | Add as draft comment does what the Agent tab's suggested comments do. Confirm that a report's suggestion can go through the same path before building the box |
| Chosen | The body's height | The page runs the pane's height and scrolls as one. See defect 1 |

### The floating button

| | | |
|---|---|---|
| Chosen | Place | The pane's top right: 14 points from its right edge, 12 from its top. It belongs to the pane, not the page, so it does not move when the report scrolls |
| Chosen | Look | 32 by 28, 7 point corners, the raised button face with a one point border and a soft shadow, the three-line menu glyph at 15 points. Tinted with the selection colour while its popover is open |
| Chosen | Nothing else | It carries no words and does not name the section being read |
| Proposed | Keyboard and name | One focus stop. Space and Return open the popover. Accessible name "Report menu"; its tooltip says the same |

### The popover

| | | |
|---|---|---|
| Chosen | Place | Hangs from the button with an arrow, its right edge on the button's, 10 points below it, 286 points wide, 10 point corners |
| Chosen | Sections | One 24 point row for each heading, in the report's order. The first row is Summary and goes to the top of the report. A sub-section's row is indented 14 points |
| Chosen | Current section | Its row has a 2 point bar in the link colour at its left and the selection tint. One row is marked, the deepest: in the frame it is the sub-section, not its parent |
| Chosen | Other actions | Under a rule: Show in conversation, View source, Copy source, each with its icon |
| Proposed | Choosing a section | Scrolls the report so that heading is at the top of the pane, and closes the popover |
| Proposed | Closing | It closes on a choice, on Esc and on a press outside it. Esc returns focus to the button |
| Proposed | Keyboard | Up and Down move through every row, sections and actions alike; Home and End go to the first and last; Return and Space choose. It opens with the current section's row focused |
| Proposed | Which section is current | The last heading whose top is at or above the top of the pane. Before the first heading it is Summary. At the end of the report, where the last headings cannot reach the top, it is the last heading on screen |
| Proposed | A long outline | The popover is never taller than the pane less its margins; past that its sections scroll and the three actions stay in view |
| Proposed | No headings | A report that is one by its length alone has only Summary and the actions in its list |
| Proposed | View source | Reads Rendered while the source is shown, as the body's own control does today |

### Back to the conversation

| | | |
|---|---|---|
| Chosen | Two ways | The question under the title, and Show in conversation in the popover |
| Proposed | What they do | Both show the Agent tab in the drawer (opening the drawer if it is shut), scroll to the turn and leave the attachment row selected. The report's tab stays open and active |
| Proposed | Closing the tab | Closing the report's tab leaves the Agent tab on its card |

### Elsewhere

| | | |
|---|---|---|
| Chosen | The tab's title | Cut at its end, with the agent glyph kept. See defect 4 |
| Chosen | A code reference | Opens that file's tab at the line; the report's tab stays beside it, one press back. As built |
| Proposed | Option-Up, Option-Down | Move to the previous and next section. Drawn for the straight read in rounds 2 and 3, not in round 4. Leave it out unless asked |

## The five defects in today's build

Found while drawing round 1. Each is in the owner's screenshots of 2026-11-14
or in the code named.

1. **The body stops at about 190 points.** `agent_report_view` adds its
   `markdown_body_view` in the default mode, not in place, with a stretch
   under it. Read from the code, the body is then a framed area that keeps its
   own height and scrolls inside itself. Not confirmed by a run. The page above
   needs the body in place and sized to its content.
2. **"4 section(s), 17 line(s)".** The counts in `agent_tab_view.cpp` have no
   plural forms, and `core::report_card_of` counts the title as a section. The
   lines count goes away with the chosen card; "Report, N sections" still
   needs "1 section" and a count without the title.
3. **The lead keeps its Markdown.** `report_card_of` takes the first three
   prose lines across headings and the card shows them as plain text, so
   backticks show and a section's sentence follows the summary. The chosen
   card shows the summary only, rendered.
4. **The tab's title is cut at its start.** "…d since your review" loses the
   words that tell two reports apart. Cut it at its end.
5. **The title is said four times** with a report open: the tab, above the
   outline, in the outline and in the body. The chosen report says it twice in
   the pane: the tab and the page. The card's row in the drawer names it too.

## Where each piece belongs

- **Core** (`agent_report.hpp`): what the summary is, how sections are counted
  and the outline the popover lists. If Show in conversation needs an intent,
  it sits beside `open_report_tab_intent` in `document_tabs.hpp`.
- **Qt**: the row, the page, the button, the popover and which section is
  current, which is a fact about the scroll position and nothing else.

## Not verified

- Nothing here was prototyped in Qt. The floating button and the popover are
  new widgets.
- A wide code block or a long line passes under the button as the report
  scrolls. The frames do not show it.
- `markdown_body_view` limits what it renders (65,536 bytes; 2,048 lines). A
  report past that needs the same notice the body gives elsewhere.
