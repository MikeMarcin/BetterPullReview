---
schema: "implement-rfc.card.v1"
id: "BPR-1503"
title: "Find in pane becomes a floating VS Code-style find widget"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"redesign","reservation":"docs/issues/_reservations/BPR-1503--BPR-1504--606a47192631.json","origin":"owner feedback on RFC-BPR-0077's Find, design accepted 2026-10-14 for the 0.2.0 release","related_cards":["BPR-1504","BPR-1419","BPR-1420"],"related_rfcs":["RFC-BPR-0077"],"frontend_design":"required"}
---

# Find in pane becomes a floating VS Code-style find widget

The owner's feedback on the shipped Find (RFC-BPR-0077): "our find in pane was
supposed to be modeled on the vscode ux". Today the find bar is a strip in the
comparison's layout. It shrinks the panes by about 75 px and labels its
toggles "Word" and "Selection". The owner accepted this redesign on
2026-10-14; it ships in 0.2.0.

## Design (accepted 2026-10-14)

- A floating overlay anchored to the top-right of the focused pane, over the
  code. It does not change the pane's layout or height.
- A rounded panel with a hairline border in the theme's rule colour.
- Left to right: the query field with inline icon toggles at its right end
  (**Aa** match case, underlined **ab** whole word, **.\*** regex; an active
  toggle shows an accent tint and outline); the "N of M" count or
  "No results"; ↑ and ↓; a find-in-selection icon toggle; × to close. An
  invalid or unsupported regex shows as an error state on the field, with
  today's message in its tooltip and status. There is no replace chevron.
- Keyboard: Cmd+F opens or focuses the widget and selects the query; Enter
  goes to the next match and Shift+Enter to the previous one; F3 and Shift+F3
  work; Option+C, Option+W and Option+R toggle case, word and regex; Esc
  closes and returns focus to the pane; Ctrl+Enter inserts a newline.
- Seeding: Cmd+F with a selected word keeps the current match on that
  occurrence ("1 of N" at the selection), as VS Code does.
- Highlights: every match gets a subtle wash and the current match a
  stronger fill plus an outline. Scroll-track ticks stay. Nothing shifts diff
  geometry, in split or unified view.
- Colours come from appearance roles. This card adds `find_match_background`
  and `find_current_match_background` to every built-in theme, held to the
  same contrast floors as the selection wash.

## Acceptance

- AC1: The find widget floats over the focused pane's top-right corner, and
  opening, typing in and closing it leaves the pane's geometry unchanged; a Qt
  test shows it in split and unified views.
- AC2: The query field carries the Aa, ab and .\* toggles; Option+C/W/R toggle
  them; Enter, Shift+Enter, F3, Shift+F3, Esc and Ctrl+Enter behave as
  designed; a Qt test shows each.
- AC3: Cmd+F with a selected occurrence reports that occurrence as the
  current match; a test shows it.
- AC4: Every match and the current match are painted from the two new theme
  roles, which are defined for every built-in theme and pass the theme
  contrast checks.
- AC5: Offscreen renders in light and dark, at 900 px and full width, match
  the design; the existing Find courses pass.
- AC6 (human): the owner accepts the floating widget and its toggles, the
  seeding and the highlights in an attended review of a frozen build.
