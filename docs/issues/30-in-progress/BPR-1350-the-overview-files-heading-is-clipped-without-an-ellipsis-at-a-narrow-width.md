---
schema: "implement-rfc.card.v1"
id: "BPR-1350"
title: "The Overview Files heading is clipped without an ellipsis at a narrow width"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"bpr-main-linux, BPR-1094's rendered checks, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","requested_skill":"frontend-design","owner_look":"required, on the Mac","related_rfcs":["RFC-BPR-0049","RFC-BPR-0050"],"related_cards":["BPR-1094","BPR-1089"]}
---

# BPR-1350: The Overview Files heading is clipped without an ellipsis at a narrow width

## Finding

[[BPR-1094]] rendered the Overview offscreen at `269f7ccff`, with the page at
1,000 px (its files column about 680 px) and the "?" chip showing. The Files
heading is cut mid-word, reading "…2 not rea" under the Open button. It has no
ellipsis, and its accessible name has no full text. The composed 1,400 px
window shows its shorter heading ("Files: 17 in this pull request, 17 not
read") whole at 100%, 125% and 200%. The chips themselves fit whole.

## Outcome (coordinator, 2026-10-08)

When the heading does not fit, it is shortened with an ellipsis. The full
sentence is its tooltip and its accessible name.

## Acceptance

- At a narrow Overview, with "?" and Open/All showing, the heading never cuts
  mid-word without an ellipsis. Check 100%, 125% and 200% and every theme.
- The tooltip and accessible name carry the full sentence. A Qt test asserts
  both, with a failing-before control.
- **Owner look on the Mac.** The design pass uses the `frontend-design` skill
  (AGENTS.md), and the owner reviews the result in a short attended session.

## Boundaries

Presentation only: no count, drill-down or settings behaviour changes.

## Built on bpr-main-linux (2026-10-09)

**Design skill.** The `frontend-design:frontend-design` plugin is not
enabled on this station. Enabling it would change Claude Code's
configuration, which a peer request cannot authorize. Its guidance was read
from the official marketplace copy
(`~/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`)
and applied where it bears on a one-line Qt heading: restraint (no new
control or styling), plain words (the sentence is unchanged), accessibility,
and reviewing renders.

**Change** (`src/adapters/qt/src/pull_request_overview_page.cpp`, presentation
only). The Files heading is now an eliding heading, a `QLabel` subclass
private to the page:
- Its text stays the whole sentence, so every reader of `files_heading()->text()`
  is unchanged. It paints the sentence shortened at the right with an
  ellipsis (`QFontMetrics::elidedText`) to the width its row gives it.
- Its minimum width is one letter and the ellipsis, so the row narrows it
  rather than clipping it.
- Its accessible name is always the whole sentence. It was the fixed "Files
  in the pull request".
- Its tooltip is the whole sentence whenever it is shortened. The cached
  catalogue's historical note, the only other tooltip it carried, follows the
  sentence after a blank line. Since [[BPR-1354]] that tooltip is anchored
  next to the heading like every other.
- Every place that set the heading's text now goes through
  `present_heading`, which recomputes the shortening. A resize, font change
  or style change recomputes it too.

**Test** (in `tests/adapters_qt/pull_request_overview_page_tests.cpp`, inside
the chips course). It runs in all four themes and both mark styles, through
`qt_pull_request_overview_page` (100%) and
`qt_pull_request_overview_chips_scale_{125,200}`, with the label chips and the
"?", Open/All and label-key controls showing. At page widths of 1,000, 800,
600 and 420 px it checks:
- the heading keeps the whole sentence as its text and accessible name;
- it stays on the page and runs under no visible control whose centre is in
  its row;
- when the sentence does not fit, the tooltip begins with it; when it fits,
  the tooltip does not repeat it;
- the narrowest page does shorten it.

With `BPR_OVERVIEW_CHIPS_RENDER_ROOT` set, it writes
`files-heading-<theme>-<scale>-<width>.png`.

**Renders** (offscreen, not committed): at 1,000 px and 125% the heading reads
"Files: 3 in this pull request, 2 changed since my last review, 2 not …",
ending before the Open button. Before, it was cut at "…2 not rea" under it.
At 800 px and 200% in the dark theme it reads "Files: 3 in this pull
request, 2 cha…". At 420 px only "F…" or the ellipsis fits beside the
controls.

**Failing-before controls** (each built, run and reverted):
- The page source before this change fails at "the heading keeps the whole
  sentence as its text and accessible name", at 100%, 125% and 200%. The
  label measured 432 px against a 440 px sentence at 1,000 px and 125%, and
  clipped its paint.
- Dropping the tooltip when shortened fails at "a shortened heading's tooltip
  is the whole sentence".
- Restoring the full-width minimum did not fail. The Overview's pane sits in
  a splitter whose minimum is 0, so the old label was squeezed and clipped,
  never widening the page. The layout checks guard against overlap and
  running off the page, not this mutation.

**Rows.** At this card's commit, in `build/dev`, 24 of 24 pass: the overview
page and chips scale rows, `qt_files_tree`, the overview public paths,
`application_accepted_file_in_overview` (its exact heading text), the window
scales, the round-trip contract, `architecture_boundaries` and
`test_network_isolation_manifest`. `application_offline_inventory` (the
catalogue-unavailable heading) also passes. `coding-format-check` (clang-format
22) and `DOCUMENTATION_OK` pass. Not run: the optimized `-O3` tree.

**Owner look: open.** The Mac freezes the build and opens the session. Look
at the ellipsis at narrow widths, the tooltip beside the heading, and the
420 px case, where only the ellipsis fits.
