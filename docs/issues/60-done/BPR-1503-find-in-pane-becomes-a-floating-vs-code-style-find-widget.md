---
schema: "implement-rfc.card.v1"
id: "BPR-1503"
title: "Find in pane becomes a floating VS Code-style find widget"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"redesign","reservation":"docs/issues/_reservations/BPR-1503--BPR-1504--606a47192631.json","origin":"owner feedback on RFC-BPR-0077's Find, design accepted 2026-10-14 for the 0.2.0 release","related_cards":["BPR-1504","BPR-1419","BPR-1420"],"related_rfcs":["RFC-BPR-0077"],"frontend_design":"required"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"application_source_find (+scale_1_25, scale_2_0) passes: pane geometry (left/right pane rects and viewport size) is identical before Find opens, after a query and reveal, and after close; the widget is in no layout and its top-right sits within 8 px of the right pane's and the unified pane's viewport corner, also at 1920 px."}},{"criterion":"AC2","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"application_source_find: Aa/ab/.* are checkable toggles inside the query field; Option+W toggles Whole Word without typing; Option+L toggles Find in Selection; Enter/Shift+Enter, F3/Shift+F3, Esc (focus returns to the pane) and Ctrl+Enter newline pass. Owner decision 2026-10-14: Option+C/Option+R stay Comment/Compose; Case/Regex tooltips name no key."}},{"criterion":"AC3","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"application_source_find: selecting the last 'needle' then Cmd+F makes match 2 current ('2 of 2'); the first occurrence gives '1 of 2' and keeps its selection."}},{"criterion":"AC4","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"application_source_find reads one find_match_background and one find_current_match_background wash at selection_wash_alpha; core_appearance_contrast (710 pairs, both washes over every washable background), qt_appearance_profile (headroom ratchet unchanged), core_theme_exchange, qt_appearance_role_display, qt_colour_role_list, qt_appearance_settings_panel and the appearance theme contracts pass."}},{"criterion":"AC5","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"artifact","detail":"Renders in build/dev/source-find-renders (split, unified, unified full width, 900/720 px, invalid regex, dark 900 px and full width; also at 1.25x/2x) reviewed by the integrator against the design; the dark hairline and the 900 px squeeze were fixed after review. 60 focused tests pass at eae0e6333c137b1b6e4685682bafb95bcd61fb0b."}},{"criterion":"AC6","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"freeform","detail":"Owner waived the attended look for this landing on 2026-10-14 (\"skip the review for now just land it on main and accelerate the process\", relayed by the coordinator). Follow-up: the look can be taken on the 0.2.0 release build."}}]
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
  work; Option+W toggles whole word and Option+L find in selection while the
  query has focus; Esc closes and returns focus to the pane; Ctrl+Enter
  inserts a newline. (Option+C and Option+R: see the owner decision below.)
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
- AC2: The query field carries the Aa, ab and .\* toggles; Option+W toggles
  whole word; Enter, Shift+Enter, F3, Shift+F3, Esc and Ctrl+Enter behave as
  designed; a test shows each.
- AC3: Cmd+F with a selected occurrence reports that occurrence as the
  current match; a test shows it.
- AC4: Every match and the current match are painted from the two new theme
  roles, which are defined for every built-in theme and pass the theme
  contrast checks.
- AC5: Offscreen renders in light and dark, at 900 px and full width, match
  the design; the existing Find courses pass.
- AC6 (human; waived by the owner for this landing, 2026-10-14): the owner
  accepts the floating widget and its toggles, the seeding and the highlights
  in an attended review of a frozen build.

## Owner decisions

- 2026-10-14: keep the existing Option+C (Comment) and Option+R (Compose
  review) keys. Find's Match Case and Regular Expression toggles take no key,
  even inside the query. Option+W and Option+L remain.
- 2026-10-14: "skip the review for now just land it on main and accelerate
  the process". The attended look (AC6) is waived for this landing. Follow-up:
  the owner can take the look on the 0.2.0 release build.

## Outcome

- The widget is `source_find_bar`, a child of `diff_workbench` in no layout,
  placed by `place_source_find()` over the target pane's viewport. A narrow
  pane first extends the panel left, then tightens spacing, then lets the
  find-in-selection icon give way to Option+L.
- `find_match_background` and `find_current_match_background` are appended to
  the role vocabulary with values for all four built-in themes, held to the
  text floor under the selection's wash opacity. High-contrast light keeps its
  recorded 18-hundredth headroom. The current-match outline is that hue
  carried to the 3:1 indicator ratio.
- Renders reviewed: `build/dev/source-find-renders/` (split, unified and
  unified full width, 900 and 720 px, invalid regex, dark at 900 px and full
  width, also at 1.25x and 2x).
