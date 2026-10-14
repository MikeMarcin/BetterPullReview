---
schema: "implement-rfc.card.v1"
id: "BPR-1504"
title: "Find in Files becomes a VS Code-style search view with a side filter"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"redesign","reservation":"docs/issues/_reservations/BPR-1503--BPR-1504--606a47192631.json","origin":"owner feedback on RFC-BPR-0077's Find in Files, design accepted 2026-10-14 for the 0.2.0 release","related_cards":["BPR-1503","BPR-1421","BPR-1422"],"related_rfcs":["RFC-BPR-0077"],"frontend_design":"required"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"qt_source_search_panel_scale_1/2: Left/Right chips change rows, summary counts and file pills; a both-sides match shows under either side; the last enabled side cannot be turned off. application_source_pr_find: F4 under Right only stays on right occurrences and Left only moves a hidden current result."}},{"criterion":"AC1b","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"qt_source_search_panel_scale_1/2: tag, line-number and snippet x positions are identical on L-only, R-only and both-sides rows at 300 and 520 px and at 1x and 2x."}},{"criterion":"AC2","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"qt_source_search_panel: the declared highlight range is exactly the match for every row (core PreviewMatch, core_source_matcher), and the painted wash spans the match advance, unselected and selected."}},{"criterion":"AC3","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"qt_source_search_panel: file names, folders, count pills and added/renamed tags; the '…' toggle shows and hides include/exclude; collapse-all collapses every file."}},{"criterion":"AC4","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"command","detail":"application_source_pr_find (+1.25, 2.0), application_source_pr_search_scope, application_source_pr_search_retry, application_source_pr_search_performance, application_source_pr_search and application_source_search_host pass at eae0e6333c137b1b6e4685682bafb95bcd61fb0b; Return/F4 open the exact occurrence."}},{"criterion":"AC5","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"artifact","detail":"Panel renders in light and dark, narrow and wide, 1x and 2x, and full-window 900 px renders reviewed by the worker and the integrator; the selected-row wash and Search's close button were fixed after review."}},{"criterion":"AC6","outcome":"pass","revision":"eae0e6333c137b1b6e4685682bafb95bcd61fb0b","evidence":{"kind":"freeform","detail":"Owner waived the attended look for this landing on 2026-10-14 (\"skip the review for now just land it on main and accelerate the process\", relayed by the coordinator). Follow-up: the look can be taken on the 0.2.0 release build."}}]
---

# Find in Files becomes a VS Code-style search view with a side filter

The owner's feedback on the shipped Find in Files (RFC-BPR-0077): "find in
files should have quick filter for left, right, both, The match text isnt
highlighted in the find in files results. The find in files pane is just kind
of ugly". Today the results are a plain tree of rows such as
`Left 12:5; Right 13:5  preview`, with no highlighting and no side filter. The
owner accepted this redesign on 2026-10-14; it ships in 0.2.0.

## Design (accepted 2026-10-14)

- The query field carries the same inline Aa / ab / .\* toggles as Find in
  pane ([[BPR-1503]]).
- Directly below the query, a side filter (amended 2026-10-14, see below):
  independent **Left** and **Right** filter chips, both on by default. A
  match shows when any of its sides is on; at least one side stays on. The
  counts and file pills follow it.
- A plain summary line ("14 matches in 3 files"). Skipped and partial
  coverage show only when non-zero, as one short line that expands on demand.
- The include and exclude file filters move behind a "…" toggle, and a
  collapse-all action joins the header.
- File rows: chevron, file name in medium weight, dimmed folder path, a
  right-aligned match-count pill, and an added, removed or renamed tag
  (old → new for a rename).
- Match rows, indented, in fixed columns (amended 2026-10-14): a two-slot
  side-tag column (**L** in the left slot in the diff's removed colour, **R**
  in the right slot in its added colour, both on a match deduplicated across
  sides), a right-aligned line-number column sized by the widest shown line
  number (the Right line on a both-sides match, both named in its tooltip),
  and the snippet in the code font from the same x on every row, with the
  match text highlighted in Find in pane's highlight colour. The selected row
  has an accent background.
- Return and F4 open a result exactly as before. The empty state reads
  "No matches in N files." with a hint; loading and partial states use plain
  words.

## Acceptance

- AC1: The side filter changes the listed results, the summary counts and
  F4 traversal; a both-sides match shows under either side; the last enabled
  side cannot be turned off; a test shows each.
- AC1b: Tag, line-number and snippet x positions are identical on L-only,
  R-only and both-sides rows, at narrow and wide widths and at 1x and 2x.
- AC2: Result rows carry the side tags, line numbers and the exact match
  range highlighted in the find-match colour; a test reads the painted range.
- AC3: File rows show name, folder, count and added/removed/renamed tags; the
  "…" toggle hides and shows include/exclude; collapse-all collapses.
- AC4: Return and F4 still open the exact occurrence; the existing PR search
  courses pass.
- AC5: Offscreen renders in light and dark, at 900 px and full width, match
  the design.
- AC6 (human; waived by the owner for this landing, 2026-10-14): the owner
  accepts the side filter, the result rows and their highlighting, the
  column alignment, the panel's overall look and the narrow width in an
  attended review of a frozen build.

## Owner amendments and decisions (2026-10-14)

- Columns: "Make the results land on a consistent column alignment, so line
  numbers and text starts at the same columns on every row".
- Side filter: "both might be a little unclear if it means OR or AND, maybe
  it should be a filter instead, left right being independently selectable
  and both on by default". The Left | Both | Right segmented control became
  two chips, accessible names "Left side" and "Right side".
- "skip the review for now just land it on main and accelerate the process":
  the attended look (AC6) is waived for this landing. Follow-up: the owner can
  take the look on the 0.2.0 release build.

## Outcome

- Core: `source_match_record::PreviewMatch` is the match's exact bytes within
  its preview; `source_side_filter` and `shown()` decide visibility, and
  `toggled()` refuses to turn off the last side.
- `pr_search_controller` walks F4/Shift+F4 over shown results only and moves
  a hidden current result to a shown one through the existing host.
- `source_search_panel` presents the chips, header (collapse-all, "…"
  details), summary, a delegate with fixed columns, file tags and pills, and
  keeps Search's close button.
- Renders reviewed: `build/dev/source-pr-find-renders/pr-search-900*.png` and
  panel renders in light and dark, narrow and wide, at 1x and 2x.
