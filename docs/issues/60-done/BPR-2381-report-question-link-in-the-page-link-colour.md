---
schema: "implement-rfc.card.v1"
id: "BPR-2381"
title: "A report's question link wears the page's link colour"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner feedback, attended review 374cb080088e516421011d3a88d707e0, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2381--BPR-2381--69f92e124dac.json","related":["BPR-2184","BPR-2219","BPR-2236"],"acceptance":["BPR-2381:question","BPR-2381:page","BPR-2381:theme"]}
---

# BPR-2381: A report's question link wears the page's link colour

## Report

The owner, attended review 374cb080088e516421011d3a88d707e0, 2026-11-24:
"Why is "Review this pull request" at the top in the dark blue and the other
links are in the more pastel blue? Can we have the softer blue for the
question link too on reports."

## Cause

A report page had two sources for a link's colour.

- The page (`report_page_html`) writes each place and each link in its text
  with the blue `core::derive_report_shades` derives from the pane's ground
  and ink and holds to 4.5:1 ([[BPR-2184]]): `#1B5D8C` in the built-in
  Light, `#9CC3DF` in Dark.
- The line under the title (`agentReportByline`) is a `QLabel` whose question
  was a bare `<a href>`, and its speech bubble and the section menu's
  current-section bar read `QPalette::Link`. No theme owns that role
  (`appearance_profile` leaves it as inherited), so it is the platform's link
  blue: measured through Cocoa, `#094FD1` in Light and `#3586FF` in Dark.

In Dark that is a saturated blue over pastel places, which is what the owner
saw. A report drawn as Markdown (one that does not keep to the template) and
the page's code boxes also took the platform's role.

A `QLabel` takes a rich-text link's colour from the application's palette
when it reads the text, not from the label's own palette: setting the role on
the label changed no pixel (measured in the new test's first draft). The
question therefore carries its colour in its own markup.

## Fix

- `report_link_colour` (`report_page.hpp`) gives the page's link colour to
  the view.
- `agent_report_view::apply_colours` writes the question in that colour
  (`show_byline`), tints the speech bubble with it, sets it as the link role
  of the page's palette (so a report drawn as Markdown and the code boxes
  inherit it) and as the link role the menu's current-section bar reads.
- `agent_report_view::changeEvent` redraws the page on `PaletteChange`. The
  page's colours are written into what it draws; before, a theme reached it
  only through `set_tone_colours`, which redraws only when a tone differs.

The Agent tab and the side chats have no such link. A prompt's name there
(`agentTurnName`, `tourSideChatPromptName`) is a plain label in the text
colour, and the item side chat shows the question as plain words. The file
references inside an Agent tab question (`agentTurnQuestionText`) are links
in the platform's role; they stand in the drawer, not on a report, and are
unchanged.

## Acceptance

- **BPR-2381:question** `qt_agent_report_view` (`check_question_link`), with
  the application's link role set to a magenta no page derives, in the
  built-in Light and Dark: the question's markup names the colour
  `core::derive_report_shades` gives for the theme, its drawn pixels reach
  that colour exactly (nearest pixel within 0) and none leans to the
  platform's role; the speech bubble's pixels are the same colour. Control: a
  label with the old bare link draws the platform's role. Pass.
- **BPR-2381:page** same test: a place and a link in the summary are written
  in that same colour, the section menu's link role is that colour, and a
  report with no sections, drawn as Markdown, draws its link and the question
  over it in that colour. Pass.
- **BPR-2381:theme** same test: the theme changes under the report shown
  with no new report and no tone told (`appearance_profile::apply_to` alone),
  and the page, the question and the bubble are in the new theme's colour;
  Light's and Dark's differ. Pass.

Also run and passing with the change: `qt_agent_tone_colours`,
`qt_agent_restored_byline`, `qt_agent_report_summary`,
`qt_side_chat_tab_and_send`, `qt_markdown_code`, `qt_nested_scroll_latch`,
`qt_item_side_chat`, `application_review_report_public_path`,
`python3 scripts/verify_documentation.py --root .`, `coding-format-check` on
the changed files and `tidy.py` on the changed sources.

Evidence revision: 4caef315a.

Renders (not committed): before and after, Light and Dark, of
`tests/fixtures/agent/promo-review-v1/review.md` as the report page shows it,
through Cocoa with windows kept off screen (the system's own link role) and
on the offscreen platform (the macOS link role stood in by
`BPR_REPORT_RENDER_PLATFORM_LINK`). The question's colour, sampled from the
native renders: Light `#094FD1` before, `#1B5D8C` after; Dark `#3586FF`
before, `#9CC3DF` after.

Owner look: whether the softer blue reads right for the question on the real
display, in Light and Dark.
