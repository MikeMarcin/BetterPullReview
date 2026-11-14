# Report templates (mockups, no choice made yet)

A template for each report the app asks an agent for, the system prompt that
asks for it, and two ways to show the result. The report's card and tab are
built and accepted ([the build spec](../report-card-v4/SPEC.md)); this is about
what is in the report. The owner, 2026-11-14, on the built Was this addressed?
report:

> the report is ugly as crap. Like we need maybe a system prompt and a template
> to fit the reports in.

Nothing here is chosen and no product code changed.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its frames, light and dark; none
is a screen capture. Each frame is the report tab's page, drawn whole, beside
the card in the Agent drawer.

## What is in the folder

| | |
|---|---|
| `prompts/` | Six system prompt entries for the Prompt library, each a draft default: [the shared format](prompts/report-format.md), then one a kind |
| `examples/` | One realistic report a kind, written to its template. The frames are rendered from these files |
| `png/` | `<kind>-a` and `<kind>-b`, light and dark |

| Kind | Prompt | Example | A, Markdown only | B, dressed |
|---|---|---|---|---|
| What changed since my review | [prompt](prompts/what-changed.md) | [example](examples/what-changed.md) | [light](png/wc-a-light.png), [dark](png/wc-a-dark.png) | [light](png/wc-b-light.png), [dark](png/wc-b-dark.png) |
| Explain this file | [prompt](prompts/explain-file.md) | [example](examples/explain-file.md) | [light](png/ex-a-light.png), [dark](png/ex-a-dark.png) | [light](png/ex-b-light.png), [dark](png/ex-b-dark.png) |
| What could this change break? | [prompt](prompts/could-break.md) | [example](examples/could-break.md) | [light](png/br-a-light.png), [dark](png/br-a-dark.png) | [light](png/br-b-light.png), [dark](png/br-b-dark.png) |
| Was this addressed? | [prompt](prompts/was-addressed.md) | [example](examples/was-addressed.md) | [light](png/ad-a-light.png), [dark](png/ad-a-dark.png) | [light](png/ad-b-light.png), [dark](png/ad-b-dark.png) |
| Review this pull request, Review file | [prompt](prompts/review.md) | [example](examples/review.md) | [light](png/rv-a-light.png), [dark](png/rv-a-dark.png) | [light](png/rv-b-light.png), [dark](png/rv-b-dark.png) |

## The convention every report shares

1. **Title.** The first line, a level-one heading. The tab and the card's
   attachment row show it.
2. **Summary.** One paragraph of two to four sentences, the answer in its
   first. The card shows it and nothing else of the report.
3. **Sections.** Level-two headings with fixed names and a fixed order for the
   kind. An empty section is left out. These are the report menu's rows.
4. **Items.** A level-three heading, then a line that is only the place
   (`path:line` in backticks), then why. The menu lists items under their
   section.
5. **Rows.** A list row for a short thing: the place, then what to know. Where
   the template labels rows, the label comes first in bold.
6. **Evidence.** A fenced code block that names its language, eight lines at
   most. A proposed change is a fenced block marked `suggestion` holding the
   lines as they should read; it replaces the lines the item names.
7. **Nothing else.** No heading below level three, no tables, no bold outside
   the template's own.

| Kind | Title | Sections, in order | Items and their words |
|---|---|---|---|
| What changed | What changed since your review | Changed; Merged main; Your conversations | An item a code area under Changed, rows for its changes. Conversation rows labelled Lines changed, Replied or Unchanged |
| Explain this file | What `{file}` does | How it works; What this pull request changes; How it fits; Worth knowing | An item a function or block, in file order. Fit rows labelled Declared in, Called by, Calls or Tested by |
| What could break | What this change could break | Likely; Possible; Unlikely; Not checked | An item a thing that could go wrong, named as it would be reported, ending in a bold Check line |
| Was this addressed? | Was this addressed? | Not addressed; Partly; Cannot tell; Addressed | An item a thread, headed with the comment's first words, the comment quoted when it says more |
| Review | Review of #N, or of the file | Must fix; Worth a look; Good; Questions for the author; Not reviewed | An item a finding, headed with a Conventional Comments label: issue, suggestion, question, nitpick or praise |

## Markdown, not a JSON block

The agent writes the report as Markdown in a fixed shape. It does not emit the
report as JSON for the app to lay out.

- **The headings the app navigates by are the ones the reader sees.**
  `core::report_outline` already reads them; the report menu, the card's
  section count and the summary need nothing new from the agent.
- **A report that strays still reads.** A missing section or an odd label
  costs a badge, not the report. A JSON report with one fault is unreadable,
  which is what `agent_answer_findings.hpp` records of a torn findings block:
  the reply showed the payload's escapes as text.
- **The source stays worth having.** Copy source, Draft and a pasted report are
  the same readable text.
- **Every agent the app runs writes Markdown well.** Whether each holds a long
  JSON schema as reliably is untested.
- **The two machine blocks the app already reads stay.** `bpr-claims` after Was
  this addressed? carries each thread's identity and claim to its chip.
  `bpr-findings` after a review carries the draftable comments. Both come
  after the report and are not shown.

What the app can rely on is small: the title line, the first paragraph, the
level-two names for the kind, level-three items, and the words in the table
above.

## The two directions

The Markdown is the same in both. They differ in how much the app draws.

- **A, Markdown only.** The prompts and templates do all the work, and the
  report shows as any Markdown does. A verdict is a word in a heading. Nothing
  to build beyond the prompts.
- **B, dressed.** The app reads the template. Verdict and label words become
  badges, with a tally under the summary and on the card. An item's place sits
  at the right of its heading. Code carries its line numbers, counted from the
  item's place. A suggestion is a diff with Add as draft comment. Rows line up
  in a table. Badge colours are the app's own: the state roles for claims and
  likelihood, the feedback label roles for review labels. Each is a
  recognition the app makes when the template is followed, and plain Markdown
  when it is not.

## Open

- **One source for a suggested change.** A review's suggestion is drawn here
  from the report's own `suggestion` block. The review skill's `bpr-findings`
  block carries draftable comments today. Whether a report keeps both, or the
  app drafts from the item, is not decided here.
- **How the shared entry reaches the agent.** The format rules are one entry
  sent before the kind's. If the Prompt library cannot send two, each kind's
  default starts with the shared text.
- **What the app does when a template is not followed.** Proposed: nothing.
  The report shows as written, and only what is recognised is dressed.
- **The prompts are untested.** No agent has been run with them.

## Renders

Made by Chrome's headless mode on a throwaway profile, one frame and theme at
a time (`?theme=dark&frame=ad-b`), then trimmed to the frame. The builder
parses each file in `examples/` by the convention's rules, so a frame is also
evidence that its example parses.

## Status

Awaiting the owner's choice.
