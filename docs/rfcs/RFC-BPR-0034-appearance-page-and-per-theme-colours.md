---
id: "RFC-BPR-0034"
title: "Appearance Page and Per-Theme Colour Customisation"
status: "Auto-Verified"
area: "BPR"
milestone: "Shell Surfaces v1"
depends_on: ["RFC-BPR-0006","RFC-BPR-0018"]
metadata: {"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# RFC-BPR-0034: Appearance page and per-theme colours

## Status

**Closed on 2026-09-22 (`Auto-Verified`).** [[BPR-0791]] records direct
evidence for C1 to C6. On 2026-09-16 the owner accepted the real macOS
application built from `ed2b3f14`: the rendered page, the VoiceOver walk over
the tiles, the real macOS colour panel and two cosmetic departures. Those
verdicts stay bound to that candidate. The page's own sources are unchanged
since then, and at closeout head `f06088b8` the focused appearance courses and
all three end-to-end appearance contracts pass. The only thing holding the
status was closure order. RFC-BPR-0018 became `Auto-Verified` on 2026-09-21
and reached main on 2026-09-22. RFC-BPR-0006 is not reopened.

Three limitations remain, and this status does not close them:

- C1's two storage limitations.
- No automated course opens the real macOS colour panel.
- C4's painter drift: the two hunk-header roles are painted by nothing, and
  the current-line band and tab markers are painted only by this page's
  preview. That drift is now [[BPR-0986]].

## Summary and scope

The Appearance page of Settings is rebuilt in the language RFC-BPR-0032 gives
the Accounts page, and the way customised colours relate to themes is changed
so that each theme keeps its own. The owner reviewed the page on 2026-09-13 and
accepted the design drawn in `design/appearance-v2.html` (three boards: the
page, editing a colour, and a light desktop with two customised themes). The
current page stacks a preview strip, a six-way mode combo box that repeats it,
three group boxes of spin boxes whose sizes read in hundredths of a point, a
role combo box with a hex field, and five full-width buttons.

This RFC owns two things: the presentation of the page, and one behaviour
change in the appearance domain. Everything else about appearance (the six
modes, the four built-in palettes, the 47 semantic roles, typography bounds,
theme import and export documents, preview and commit) is RFC-BPR-0006's and
RFC-BPR-0018's and is consumed as it is.

## The behaviour change: one customisation per theme

Today the appearance store keeps exactly one customised palette, tagged with
the variant it was made for (`PaletteStored`, `PaletteVariant`). The Qt
appearance profile reads it as overrides against that variant's built-in
palette, applies them only while that variant is on screen, and, when a
palette is committed while another variant is on screen, makes that the
customised variant and discards the previous overrides. So a reviewer can tune
Dark or Light but not both, and tuning one silently forgets the other. The
owner's verdict: "I'm a bit confused at how colors and theme interact", and,
told the rule, "I'm fine with redesigning that behaviour".

The new rule: the store keeps one customised palette per variant (light, dark,
high-contrast light, high-contrast dark). Overrides for a variant apply only
while that variant is resolved, as now. Committing while a variant is on
screen changes that variant's customisation and no other. Migration files the
existing single palette under its recorded variant; a record without a stored
palette carries no customisation, as now. Export writes the theme on screen
and says so in its dialog; import previews into the theme on screen and says
so; Reset all resets the theme on screen. A theme's customised-colour count is
the number of roles whose override differs from that variant's built-in value,
which is the comparison the profile already makes.

## Accepted design

The page is a title, the lede "Changes apply as you make them and show in the
review window behind this one.", and three cards. Each card's header carries
its name and, at the right, its Reset; a card body is rows in the shared
language, one line each: label, an info glyph whose popover carries the
consequence, and the control.

### Theme

An inset well the width of the card holding a sideways-scrolling strip of six
tiles, one per mode, in the order System, Light, Dark, High contrast system,
High contrast light, High contrast dark. Three tiles and part of a fourth are
visible at the page's default width; the well takes a thin scrollbar and Left
and Right move the selection and scroll it into view. A tile is a 232 by 132
point frame painted by the existing theme preview strip from that mode's
built-in palette plus that theme's overrides: four lines of real code with a
gutter, a keyword, a type, a function, a removed line and an added line and a
comment, so the diff and syntax colours are seen. The two System tiles split
diagonally, light in the top-left triangle over dark in the bottom-right, each
half painted in its own palette and the edge being nothing but the change of
palette; their caption reads "dark now" or "light now". The selected tile has
the highlight ring and its label in the highlight colour. A tile whose theme
has customised colours captions it: "2 colours customised". The mode combo box
is gone; the tiles are the control.

### Type

Two columns. Left, eleven rows: Interface font (the system font unless chosen,
with a chooser), Interface size in points with a stepper, Interface weight as
a 100 to 900 stepper, Line height as a percentage stepper; Code font, Code
size, Code weight, Tab width as a 2, 4, 8 segment, Ligatures, Bold syntax
emphasis and Italic syntax emphasis as switches. Sizes are shown in points
with two decimals dropped when zero, never in hundredths. Right, a labelled
Preview: an interface sample (a drawer row and a path at the interface font,
size, weight and line height) and a six-line code sample painted from the
accepted typography and the theme on screen that exercises every control: an
italic comment, bold keywords, a type name, a function name, `!=`, `&&` and
`>=` where ligatures would join, two tab depths with a faint tab marker at the
chosen width, a removed line, an added line carrying a selection and a macro,
and the current-line highlight. A caption under it states the values in
effect. The preview is the review window's own painter in miniature, not a
second renderer.

### Colours

The card is titled for the theme on screen, "Colours for Dark", with the
subtitle "Edits change Dark only. Each theme keeps its own colours." Its header
holds Import, Export and Reset all. Under a filter field, a list of all 47
roles in four groups with sticky group headers and counts, Surfaces and text,
Diff, Review states, Syntax, each row a swatch, the role's name in words and
its value in the code font. The list scrolls inside the card. A swatch opens
the platform colour picker and the change previews in the review window as
the picker moves; an edited row shows its new value in ink and the old one
beside it. While anything is previewed, one bar at the card's foot says
"Previewing 2 colour changes to Dark in the review window." with Cancel and
Keep changes; Keep changes is the existing commit. The role combo box, hex
field and the separate Preview, Commit and Cancel preview buttons are gone.

### Language

The row language of RFC-BPR-0032 with one amendment the owner made here and
that applies to every Settings page: a row is one line, label, info glyph,
control. The consequence sentence lives in the glyph's popover, shown on hover
and on keyboard focus, and is the glyph's accessible description, so it is
never reachable by pointer only. A row whose second text is a value rather
than a consequence (Rate limit, Access) keeps that value in the row.

## Architecture

The core decides. The host performs. Qt presents.

- **Core.** The appearance settings record gains one stored palette per
  variant; validation, defaults and reset semantics per variant follow the
  existing single-palette rules. No Qt, no store.
- **Application host.** The appearance host accepts commit, reset, import and
  export intents scoped to a variant (the resolved one, supplied by the
  composition, never guessed in Qt), persists per-variant palettes, and
  publishes per-variant customisation counts in its projection.
- **Persistence.** The appearance settings store bumps its schema; migration
  copies the single stored palette to its recorded variant's slot, is
  transactional, restart-tested and fail-closed on future or malformed
  versions, as RFC-BPR-0018 requires.
- **Qt adapter.** The appearance profile keys its override table by variant.
  The theme preview strip lays its six cards in a scrolling well, paints the
  wider snippet and the diagonal split by clipping, and captions from the
  projection. The settings panel becomes the three cards: an `info_glyph`
  widget (**new**) with popover and accessible description, shared with the
  Accounts page; a `type_preview` widget (**new**) painted by the existing
  code painter; a `colour_role_list` (**new**) over the 47 roles with a
  filter, sticky group headers and the picker; the preview bar. Spin boxes
  show points.
- **Composition root.** Supplies the resolved variant to scoped intents and
  the theme on screen to the Colours card title.

Prohibited: a second stylesheet or hard-coded palette; a widget that reads or
writes the store; guessing the variant in Qt; a preview painter of its own;
losing an existing customisation in migration.

## Non-goals

New roles, new modes, new typography fields, a theme gallery or marketplace,
per-window themes, and any change to theme document format beyond carrying a
variant, which it already does.

## Acceptance criteria

- **C1 — One customisation per theme.** The store keeps a customised palette
  per variant; migration files the existing palette under its recorded
  variant with nothing lost; overrides apply only while their variant is
  resolved; committing changes only the theme on screen; Export, Import and
  Reset all act on the theme on screen and say so; customised counts equal
  the roles that differ from the built-in palette.
- **C2 — The theme strip.** Six tiles, one per mode, in a sideways-scrolling
  well with a thin scrollbar and Left and Right selection; each painted by the
  existing preview painter from its built-in palette plus its overrides with
  the wider snippet; both System tiles split diagonally by clipping with the
  half in effect named in the caption; customised counts captioned; the mode
  combo box absent.
- **C3 — Type rows.** Eleven one-line rows of label, info glyph and control
  with sizes in points, weight as a 100 to 900 stepper, tab width as a 2, 4, 8
  segment and switches for ligatures and emphasis; every consequence reachable
  by hover, by keyboard focus and as the glyph's accessible description; Reset
  in the card header.
- **C4 — The preview.** A labelled preview painted by the review window's
  painter from the accepted typography and the theme on screen, whose six code
  lines show an italic comment, bold keywords, a type, a function, ligature
  pairs, two tab depths with markers, removed and added lines, a selection, a
  macro and the current line, with a caption stating the values in effect,
  repainting on every accepted change.
- **C5 — The colours card.** Titled for the theme on screen with its
  subtitle; Import, Export and Reset all in the header; a filtered, grouped,
  scrolling list of all 47 roles with sticky headers, swatch, name and value;
  a swatch opens the picker and previews live; edited rows show new and old;
  one bar with Cancel and Keep changes appears only while previewing; the
  role combo box, hex field and separate preview buttons are absent.
- **C6 — Rendered and reachable.** The page in its default, editing and
  light-desktop states, with every tile, renders at 100, 125, 150 and 200
  percent in the four appearances, every control reachable by keyboard with
  an accessible name and the tiles' captions announced, and the owner accepts
  the real macOS application through the attended-review workflow.

## Verification

| Claim | Direct surface | Check |
|---|---|---|
| C1 | Appearance host, store and profile with prior-schema fixtures | Migrate a stored light and a stored dark palette; commit on each variant and reopen; assert both survive, counts, and that the other variants render built-ins; export and import name the variant |
| C2 | Composed Settings with the preview strip | Tile inventory and order; scroll and keyboard selection; pixel assertions on both halves of the System tiles against the built-in palettes; captions per resolution and customisation |
| C3, C4 | Composed Settings and the shared painter | Row inventory and one-line geometry; popover on hover, focus and its accessible description; preview repaint after each accepted change with the expected glyph runs |
| C5 | Composed Settings with the recording picker port | Title follows the variant on screen; list inventory equals 47; filter; picker preview and Keep changes through the existing commit; bar presence only while previewing |
| C6 | Deterministic captures then attended review | Four appearances, four scales, three states; keyboard walk; VoiceOver; one attended candidate |

## Documentation impact

The settings guide's Appearance page, the appearance and accessibility UX
document's customisation section, the storage documentation for the schema
bump, and the theme document notes on variant scope. Historical cards and
packets are not edited.
