---
schema: "implement-rfc.card.v1"
id: "BPR-2302"
title: "Review and Comments settings sections take Appearance's card header"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","reservation":"docs/issues/_reservations/BPR-2302--BPR-2302--01c0ff81307f.json","origin":"owner chat 2026-11-19 (\"I just noticed that the review settings page has these things that look like they are section headers but they don't look like section headers\"; \"Actually it looks like comments has this too\"; \"I think we should take the section header style from the appearance menu and port it to those two menus\")","related_cards":["BPR-0661","BPR-1513","BPR-1597","BPR-2299"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2302: Review and Comments settings sections take Appearance's card header

## The owner's words (2026-11-19, verbatim)

"I just noticed that the review settings page has these things that look like
they are section headers but they don't look like section headers"

"Actually it looks like comments has this too"

"I think we should take the section header style from the appearance menu and
port it to those two menus"

## What was wrong

Settings › Review and Settings › Comments group their rows under a title with
its own Reset (`addGroup` in `workbench_window_drafts_and_behaviour.cpp`). The
title was a plain `QLabel` at the body weight and size, on the page ground,
with nothing around the group. It read as one more row name: "Opening" looked
like a setting called Opening with a Reset button. Settings › Appearance draws
each section as a card: a panel frame, a raised header strip (`Light` role)
with the title in semibold and the Reset on the right, a hairline, then the
rows.

## What changed

- The Appearance card moved out of `appearance_settings_panel.cpp` into a
  shared `src/adapters/qt/src/settings_card.{hpp,cpp}`
  (`detail::make_settings_card`, `detail::make_settings_hairline`, and the
  inset and gap constants). Appearance builds its cards from it. The
  extraction changes nothing on that page.
- Every Review and Comments group is now that card. The title and its Reset
  sit in the header strip and the rows sit in the body, with the cards 12 px
  apart, as on Appearance. Object names are kept (`<reset>Heading` for the
  title, `<reset>` for the Reset) and the card is `<reset>Card`, so Settings
  search still files each row under its section.
- A card title is a `settings_card_title`, which assistive technology reads as
  `QAccessible::Heading` with its words as its name. This applies on
  Appearance too, where the titles had been plain static text.
- The card's insets take 34 px of width, so a row's name now asks for the
  180 px column (its size hint) instead of insisting on it (its minimum)
  (`row_name_label`). At Settings' minimum width (760 × 480), Reading
  width's name gives up the room its choices, Advanced… and info glyph need.
  That row's choices then start about 16 px left of the other rows'. At
  ordinary widths every row lines up as before.
- Reset keeps its raised quiet chip (RFC-BPR-0087's shared treatment, which
  `qt_feedback_label_settings` checks). The chip now takes the header's
  `Light` ground as its surface, and its 3:1 border is checked against that
  ground. Appearance's Reset stays the flat push button it was.

### Labels changed

| Page | Label | Before | After |
|---|---|---|---|
| Review | Opening | Plain body-weight label on the page ground; bezel Reset at the far right | Semibold card title in the raised header strip with its Reset; hairline; rows in the card |
| Review | Reading | Same as Opening | Same as Opening |
| Review | Finishing a review | Same as Opening | Same as Opening |
| Review | Layout | Same as Opening | Same as Opening |
| Comments | Labels | Same as Opening | Same as Opening |
| Comments | Comment workshop | Same as Opening | Same as Opening |
| Comments | Descriptions and comments | Same as Opening | Same as Opening |

The row names ("A pull request opens in", "Reading width", "Label marks" and
the rest) stay ordinary labels.

### The other pages (swept, not changed)

Other pages also have headings that differ from Appearance's card. None of
them is the same pseudo-heading problem, so they were left alone:

- Home & queue: "Observer rules" and "Saved repositories" are bare semibold
  headings with no card; "Queue defaults" is a group box title.
- Repositories: "Clone folders" and "Linked clones" are bare semibold
  headings.
- Agents: "Agents" is a semibold inline heading beside its sentence.
- Storage & privacy: "Troubleshooting" is a group box title. "Local data
  location" is a plain label over its field: a field's name rather than a
  section heading.
- Prompts keeps its own surface cards. General, Profiles, Accounts & hosts and
  Notifications have no section headings.

## Acceptance

- AC1 Every Review and Comments section is a card on its page whose frame,
  header ground, header margins and title face are Appearance's, with the
  title and its Reset in the header.
- AC2 Each card title is announced as a heading named by its words, as
  Appearance's titles now are. A row name is not a heading. No other semibold
  label on either page (besides the page title) sits outside a card header.
- AC3 Appearance looks as it did; Settings search, Reset per group, the
  RFC-BPR-0087 chip treatment and the minimum-size fit pass as before.
- AC4 Renders in Light and Dark at 1.0 and 2.0 (offscreen) and in the macOS
  native look were compared with Appearance.
- AC5 Gates: format and tidy clean on the changed files; documentation gate.

## Evidence (2026-11-19)

- `qt_feedback_label_settings`:
  `test_review_and_comments_sections_wear_the_appearance_header` checks every
  section listed above against Appearance's Theme card. It checks the frame
  shape and shadow, the header's ground role, fill and margins, and the
  title's weight, size and family. It checks that the title and Reset sit in
  the header, and that the title is a `Heading` named by its words (and
  Appearance's title too). Negative controls: `markReadLabel` is an ordinary
  label and not a heading, and no other semibold label on either page sits
  outside a card header. `test_settings_choices_share_the_overview_treatment`
  now reads Reset's 3:1 border against the header strip.
- `qt_settings_window` and its `_scale_125`, `_150` and `_200` runs: every
  page fits 760 × 480 with no sideways scroll in all four themes. Search still
  files rows under Opening, Reading, Labels and the other sections.
- `qt_appearance_settings_panel`, `qt_feedback_label_threads`,
  `qt_workbench_window_scale_100`, `_125` and `_200` pass.
- Renders from `--section-headers-render FOLDER` (every Settings page in
  Light and Dark): offscreen at `QT_SCALE_FACTOR` 1 and 2, and under
  `BPR_NATIVE_LOOK=1` (Cocoa, QMacStyle, off screen) in the light and dark
  schemes. They were compared with Appearance before and after the change.
  Appearance's renders did not change.
- Not run: the end-to-end `setup_destination_public_path` capture course,
  which only saves a picture of Comments, and the full application. Disk was
  short (about 5 GB free).
- Format (clang-format 22) and clang-tidy 22 on the changed C++ files: clean.
  `scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.

## Owner look

No frozen build was prepared. The renders above are offscreen and native
look. If the owner wants a live look, a tests-on freeze can open Settings ›
Review and Comments. Ask one Accept/Reject question per page: "Review's (or
Comments') sections read as Appearance's do".
