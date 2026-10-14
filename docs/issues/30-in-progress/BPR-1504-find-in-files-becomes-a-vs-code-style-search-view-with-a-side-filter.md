---
schema: "implement-rfc.card.v1"
id: "BPR-1504"
title: "Find in Files becomes a VS Code-style search view with a side filter"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"redesign","reservation":"docs/issues/_reservations/BPR-1503--BPR-1504--606a47192631.json","origin":"owner feedback on RFC-BPR-0077's Find in Files, design accepted 2026-10-14 for the 0.2.0 release","related_cards":["BPR-1503","BPR-1421","BPR-1422"],"related_rfcs":["RFC-BPR-0077"],"frontend_design":"required"}
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
- Directly below the query, a segmented **Left | Both | Right** quick filter,
  defaulting to Both, filters the results by diff side, and the counts
  follow it.
- A plain summary line ("14 matches in 3 files"). Skipped and partial
  coverage show only when non-zero, as one short line that expands on demand.
- The include and exclude file filters move behind a "…" toggle, and a
  collapse-all action joins the header.
- File rows: chevron, file name in medium weight, dimmed folder path, a
  right-aligned match-count pill, and an added, removed or renamed tag
  (old → new for a rename).
- Match rows, indented: an **L** tag in the diff's removed colour or an **R**
  tag in its added colour (both on a match deduplicated across sides), a
  dimmed line number, and the snippet in the code font with the match text
  highlighted in Find in pane's highlight colour. The selected row has an
  accent background.
- Return and F4 open a result exactly as before. The empty state reads
  "No matches in N files." with a hint; loading and partial states use plain
  words.

## Acceptance

- AC1: The side filter changes the listed results, the summary counts and
  F4 traversal; a test shows Left, Both and Right on a fixture.
- AC2: Result rows carry the side tags, line numbers and the exact match
  range highlighted in the find-match colour; a test reads the painted range.
- AC3: File rows show name, folder, count and added/removed/renamed tags; the
  "…" toggle hides and shows include/exclude; collapse-all collapses.
- AC4: Return and F4 still open the exact occurrence; the existing PR search
  courses pass.
- AC5: Offscreen renders in light and dark, at 900 px and full width, match
  the design.
- AC6 (human): the owner accepts the side filter, the result rows and their
  highlighting, the panel's overall look and the narrow width in an attended
  review of a frozen build.
