---
id: BPR-0198
title: Moved code, hunk headers, and intraline evidence against the model
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0196]
---

# Moved code, hunk headers, and intraline evidence against the model

BPR-0196 painted three of the reference's seven elements. This card owns the
rest of the palette's diff roles, and the evidence BPR-0196 left short.

## What is missing

- **`diff_moved_background`** is adopted by the surface and never applied.
  `file_alignment::Moves` holds `move_relation` records that nothing reads, so
  moved code is currently indistinguishable from an unrelated
  deletion-plus-addition — which is exactly the distinction move detection
  exists to make.
- **`diff_hunk_header_background` and `diff_hunk_header_text`** are unapplied.
  The reference delimits change blocks; the panes do not.
- **Intraline emphasis has no model-comparing test.** BPR-0196 paints the spans
  from `computed_file_diff::Intraline`, and proves the *line* bands bounded, but
  nothing yet asserts that the painted character ranges are the ranges the core
  computed. A byte-to-character conversion that was quietly wrong would look
  plausible on ASCII and be wrong on the first file with a non-ASCII identifier.

## Required direct evidence

- A moved block carries `diff_moved_background` on both sides, and is
  distinguishable from a deletion paired with an unrelated addition.
- Painted intraline ranges are compared against `computed_file_diff::Intraline`
  resolved through `Pairs`, on a fixture containing at least one multi-byte
  character ahead of an edit, so the UTF-8 to UTF-16 conversion is exercised
  rather than assumed.
- Hunk headers are painted where the model places change-group boundaries.
- The bounded-painting assertion from BPR-0196 still holds with the new roles
  applied; every addition here is inside the same viewport bound.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
