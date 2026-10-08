---
schema: "implement-rfc.card.v1"
id: "BPR-1334"
title: "Line details: the current line, base over head, full width under the comparison"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner chat request, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1334--BPR-1334--7d299944dc95.json","related_cards":["BPR-0646","BPR-1264","BPR-1212","BPR-1330","BPR-1313"],"requested_skill":"frontend-design","acceptance":["BPR-1334:C1","BPR-1334:C2","BPR-1334:C3","BPR-1334:C4","BPR-1334:C5"]}
---

# BPR-1334: Line details under the comparison

## Owner request

The owner, in chat on 2026-10-08, verbatim:

> Can we add something akin to the beyond compare text details view as an
> optional on by default view page below the side by side which shows just
> the current cursor's line as a full width single pane diff

and, later the same day:

> The detail pane might be it, I thought we had already built it but I don't
> see it in the app or view menu

Beyond Compare's Text Details pane sits under the side-by-side view. It shows
the current line's left version above its right version, full width, with the
character differences highlighted, so a long line can be read without
scrolling sideways.

## What exists today (inventory, `origin/main` 03ae5aa31)

- **The Detail pane exists and is composed, but hidden.** `diff_workbench`
  has built a `QPlainTextEdit` (`diffSelectedChangeDetail`, "Selected change
  detail") under the panes in its vertical splitter since RFC-BPR-0004.
  [[BPR-0646]] made it a *review detail*. It is off by default and shown only
  by **Review details** (`Alt+Shift+3`), together with the exact epoch
  anchors. That action is in the review bar's **Comparing …** button menu,
  not in the View menu. That is why the owner could not find it.
- **It was specified as this feature.** RFC-BPR-0005 asks for "a full-width
  Beyond Compare-inspired line-over-line lower detail pane", and RFC-BPR-0004
  feeds it from the same line mapping. The owner remembered it correctly.
- **When shown, it is not this feature.** It is at most 96 px tall and does
  not wrap. It holds plain text: a heading ("Change 3 of 11" or "Selected
  line 12"), then "Earlier" with the left text and "Later" with the right
  text. A side with no counterpart shows its whole opposing span, capped at
  64 lines by [[BPR-1264]]. It has no line numbers and no character
  highlights.
- **Core already computes intraline spans.** `diff_compute` runs a bounded
  character LCS over every *similar* line pair. The result is
  `computed_file_diff::Intraline`, given as byte spans relative to each line's
  content. `detail_for_line` and `detail_for_selection` return them in
  `line_detail_input::Intraline`. A pair whose LCS ran out of matrix budget
  stays similar but has no spans ([[BPR-0719]]). The panes already paint these
  spans (`append_intraline_emphasis`).
- **The line cursor.** [[BPR-1212]] slice 1 (a keyboard line cursor) is not on
  `origin/main`. The current line is the semantic selection's
  `PreferredLine`. A click, a change step, a conversation anchor or a restored
  reading position sets it, and `diff_workbench::current_line` reports it. The
  [[BPR-0986]] current-line band marks it in the panes.

## Change

The existing Detail pane becomes **Line details**. No second pane is added.

- It shows the current line twice, full width: the base version above the
  head version. Each row has its line number and a −/+ mark in a gutter, and
  the changed characters are highlighted. Long lines wrap; the strip never
  scrolls sideways.
- A mouse selection that spans lines shows each selected line's pair, capped
  at 16 lines, followed by a line that says how many more were selected.
- Unchanged lines show both versions without highlights. A removed line shows
  its base row; the head row says the line is not in head. An added line is
  the mirror image. A changed line with no paired counterpart that replaces
  one line shows that line as its other row, without emphasis; one that
  replaces several names them.
- A line longer than 4,096 bytes is cut at a character boundary, followed by
  a marker that gives how many more bytes there are. Core decides the cut
  and clips the highlight spans to it. Qt draws what core gives it.
- It is on by default. **View > Line Details** turns it off or on, and the
  choice is a per-profile preference, also on Settings > Reviews and queue
  (a Comparison row).
  Review details no longer shows or hides it.
- Its top edge is the shared resize seam with `hold_resize_cursor`. The height
  the reader drags to is kept per window.

## Acceptance

- **C1 (direct).** In the production workbench, the current line's base and
  head rows appear under the comparison, wrapped, with line numbers and
  intraline highlights from core. They follow a click, a change step and a
  mouse selection across lines. Added, removed and unchanged lines each read
  sensibly.
- **C2 (direct).** The strip is shown in a fresh profile. View > Line Details
  hides it and shows it again, Settings shows the same state, and the choice
  survives a restart of the profile.
- **C3 (direct).** The seam resizes the strip, holds the resize cursor, and the
  dragged height survives a restart for that window.
- **C4 (direct).** A 100,000-byte line is truncated with its marker, a range is
  capped, and the optimized large-review rows keep every gated phase under
  100 ms.
- **C5 (owner).** The owner looks at a frozen build with long changed lines
  and answers Accept or Reject for "line details strip".

## Checkpoint (2026-10-08, bpr-main-mac implementer)

Branch `claude/line-details-strip`, rebased on `origin/main` at `5efe30739`.
Commits: `0e4ef5ba1` (Line details), `c32e467e4` (fixture, course, row bands,
one-line replacement rule), `ed2ab5f7d` (pin reseal over `512c2836b`).
Design: `frontend-design:frontend-design` loaded first; the strip inherits the
panes' code font, gutter roles and diff colours, with nothing added. The
gutter shows line number and −/+ only, every wrapped line of a row wears the
row's band, and the emphasis is the one strong mark.

Verification at `c32e467e4` (dev, macOS arm64 Debug, offscreen), all passing:
`core_diff_semantics`, `qt_diff_workbench_scale_{100,125,200}`,
`qt_workbench_window_scale_100`, `qt_workbench_focus_order_scale_100`,
`qt_settings_window`, `sqlite_workbench_settings_storage`,
`application_workbench_host`, `application_diff_workbench_public_path`,
`application_profile_window_layout_public_path`,
`application_line_details_public_path`, `github_recorded_fixture_contract`,
`github_contract_fixture_security`; format check passes. Failing-before
controls (each mutation reverted after): range ignored, cursor hold removed,
emphasis dropped, strip tied to Review details, profile column not written,
held height dropped, UTF-8 back-off removed, emphasis unclipped, similar pairs
read unchanged, one-line replacement rule off. Each fails its named check.

Disk ran out twice during this card. Other worktrees' builds filled it, so the
Debug tree was deleted after these runs. The optimized rows and the owner look
run from one frozen tests-on Release build:
`artifacts/bpr-1334/line-details-frozen-v001/` (source exported from
`ed2ab5f7d`, archive sha256 `9da95165…`).
