---
id: BPR-0196
title: The diff panes paint no diff
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# The diff panes paint no diff

Found by the product owner rejecting all three subjects of the
`selection_contrast_remedy` attended review on 2026-08-06. Their note:

> this looks like a list box not a document view, i'm not sure if that's a
> real issue or just a side effect of the text chosen.

It is not the text.

## What was measured

Against `src/adapters/qt/src/diff_workbench.cpp`:

| Measurement | Result |
|---|---|
| `setBackground` calls in the file | **1** — the selection band |
| `setForeground`, `setFormat`, `mergeCharFormat` calls | **0** |
| `appearance_role::` values referenced | **0** of 45 |

The core's palette declares seven diff-presentation roles —
`diff_added_background`, `diff_removed_background`,
`diff_intraline_added_background`, `diff_intraline_removed_background`,
`diff_moved_background`, `diff_hunk_header_background`,
`diff_hunk_header_text` — and about twenty-three `syntax_*` roles. **The panes
apply none of them.** Even the selection band arrives through
`QPalette::Highlight` rather than through its role.

So the panes render undifferentiated plain text. Added lines, removed lines,
replaced spans, moved code, and hunk headers are all painted identically:
not at all.

## Against the reference the owner supplied

p4merge, comparing two real headers:

| Element | p4merge | Here |
|---|---|---|
| Syntax colouring | yes | none — `packaged_syntax_classifier` has zero symbols in the binary (BPR-0185) |
| Added and removed line tint | yes | none |
| Intraline emphasis on changed spans | yes | none |
| Moved-code indication | — | none |
| Hunk headers | — | none |
| Between-pane connectors | yes | **yes**, `diff_link_map` paints these |
| Selected line | solid band, readable foreground | 1.06:1 wash |

One of seven.

## Why this reopens BPR-0194 rather than sitting beside it

BPR-0194 asked whether the selected line should be an opaque band or a
translucent wash. The whole trade-off in that question is **what the marking
obscures** — a band replaces the line's colouring, a wash tints it.

Right now it obscures nothing, because nothing is painted. The question could
not be answered on the evidence offered, and the owner was right to refuse it.
Decide the selection treatment once a selected line has an added-line tint,
intraline emphasis, and syntax colour underneath it to compete with.

## The reference the owner supplied, read precisely

Two p4merge captures were provided, dark and light. The owner's judgement:
light reads better than dark. What the light capture actually does, in the
order it matters:

| Element | Treatment |
|---|---|
| Unchanged lines | **no tint at all**, plain background — the eye goes straight to what moved |
| Removed / left-only lines | pale lavender across the full pane width |
| Added / right-only lines | pale mint green across the full pane width |
| Changed spans inside a tinted line | a **second, stronger** box within the tint |
| Change blocks | delimited by dashed rules, connectors across the gutter |
| Syntax colouring | full, and legible **on top of** the tints |
| Selection | a blue **tint over a multi-line range**; syntax colouring survives underneath it |

Two properties are load-bearing and easy to lose:

- **The tints are pale because syntax colour has to survive on them.** A
  saturated added-line background that swallows the keyword colouring trades
  one signal for another rather than adding one.
- **Intraline emphasis is a second level on top of the line tint**, not a
  replacement for it. A reviewer sees "this line changed" and "this is the part
  that changed" simultaneously; collapsing them to one level loses the cheaper
  of the two.

### What this settles for BPR-0194, after two corrections

The owner corrected two claims made here from their captures, and both
corrections point the same way.

First: an earlier draft said the selection covered "exactly one line". It does
not — the third capture shows a range of roughly twenty lines.

Second: this section then said the syntax colouring was *lost* under the
selection. It is not. Pointing at
`// We only check for SSE3 instruction set. SSSE3 instructions are not used.`,
the owner observed the colouring survives beneath the band. The selection
**tints** rather than replaces.

So the reference does what the owner's preferred candidate did: a translucent
wash that preserves what is underneath, applied across a range. The opaque band
this card previously read into the screenshots was never there.

**A limit on this kind of evidence, recorded because it produced two wrong
claims.** Structure reads reliably from these captures — tints, change blocks,
connectors, the extent of a selection. Per-glyph foreground colour under a
translucent band does not. Do not assert token-level colour from a downscaled
screenshot; measure it from a capture this repository produced, or ask.

### A capability gap the correction exposed

`set_line_selection` takes a single `std::optional<core::source_line_ref>`.
BetterPullReview can mark **one line**. The reference selects a multi-line
range across a change block.

That is a functional difference rather than a presentation one, and nothing on
the board currently owns it. Decide whether range selection is in scope before
choosing a selection treatment, because a marking that reads well on one line
and badly across twenty is not the same choice.

## What this card must not become

Do not paint colours the core did not decide. The semantic model already
computes alignment kinds, change groups, and intraline spans; the pane's job is
to render what `core::diff_semantics` decided, not to re-derive it. A second
opinion about which lines changed is the defect RFC-BPR-0005 already forbids in
its reuse rules.

Do not hard-code colours. Every one of these is a declared
`core::appearance_role` and must come from the composed palette, which BPR-0183
made reachable.

## Required direct evidence

- Added, removed, replaced, and moved lines each carry their role's background
  on the composed surface, read off the rendered pixels rather than off the
  format that was supposed to be applied.
- Intraline emphasis marks exactly the spans `core::diff_semantics` computed,
  compared against the model rather than eyeballed.
- Each background is the composed palette's colour for its role, so a palette
  change moves it and a hard-coded value fails.
- The pane still shows one shared vertical scrollbar, no synthetic rows, and
  the bounded-widget counts RFC-BPR-0005 recorded at 60, 6,000, and 60,000
  lines are re-measured and either hold or are restated.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`diff_workbench` paints the diff the core decided. It went from one
`setBackground` call and zero appearance roles to five roles applied:

| Element | Source | Role |
|---|---|---|
| Removed lines | `alignment_kind::left_only`, and the left side of a `replacement` | `diff_removed_background` |
| Added lines | `alignment_kind::right_only`, and the right side of a `replacement` | `diff_added_background` |
| Intraline emphasis | `computed_file_diff::Intraline`, via `Pairs` for the line | `diff_intraline_*_background` |
| Unchanged lines | `alignment_kind::equal` | **deliberately none** |

Nothing here decides what changed. Every span is read from
`core::diff_semantics`, and `alignment_kind` is switched over exhaustively so a
new kind is a compile error rather than a silently unpainted line.

`QPalette` carries no role for "an added line", so the surface takes the
semantic palette through `adopt_appearance`. A workbench never given one paints
**no** line backgrounds rather than inventing colours — a composition that
forgets the wiring shows an obviously unpainted diff instead of a plausible but
unowned one.

### Layer order is the design

Qt draws extra selections in the order supplied: line band, then intraline
emphasis, then selection. That is what makes "this line changed", "this is the
part that changed", and "this is where I am" three readable levels instead of
one winning and two vanishing. It matches the two-level structure in the
owner's reference.

### Byte offsets are converted, not assumed

The core reports intraline edits as byte spans because bytes are what it
compared; Qt addresses text in UTF-16. The prefix is decoded and measured. The
current fixture is pure ASCII, so assuming the two were equal would have looked
correct here and shifted every emphasis left by one continuation byte per
non-ASCII character on the first real file.

## A regression this card introduced, and the evidence that could not see it

The first working version emitted one `ExtraSelection` per changed line for the
**whole document**. `QPlainTextEdit` walks that entire list on every paint, so a
large comparison would have cost tens of thousands of list entries per repaint.

**The full suite passed, 123/123, including every bounded-widget assertion.**
Extra selections are text formats rather than widgets, so the 68/68/68 counts
at 60, 6,000, and 60,000 lines were untouched. RFC-BPR-0005's virtualization
evidence held perfectly while the surface acquired an unbounded per-paint cost.
That evidence is structurally incapable of seeing this, and recording it here is
the point: a bounded widget count is not a bounded renderer.

Painting is now bounded to the lines a pane can show plus a screenful either
side, and `apply_frame` repaints on scroll — without that the bound would be a
rendering bug rather than an optimisation, and a reviewer would scroll into a
changed region and find it untinted. The bounded render is **pixel-identical**
to the unbounded one.

## What this card does NOT deliver

- **No syntax colouring.** `packaged_syntax_classifier` still has zero symbols
  in the binary; BPR-0185 owns linking it.
- **No moved-code indication.** `diff_moved_background` is adopted but never
  applied — `file_alignment::Moves` is not yet consulted.
- **No hunk headers.** `diff_hunk_header_*` remain unapplied.
- **No change-block boundaries or intraline evidence against the model.** The
  intraline spans are painted from `Intraline`, but no test yet compares the
  painted spans to the model's; only the line bands are proven.

Three of seven elements from the reference, up from one.

## Evidence

- `ctest --preset dev` — **123/123**. `ctest --preset core-only` — **57/57**.
- `coding-format-check` passed. `DOCUMENTATION_OK`.
- `qt_diff_workbench_scale_100/125/200` and `qt_workbench_window_scale_*` pass,
  so the measured axis and bounded-widget counts are unchanged.
- Captured render shows removed lines tinted on the left, added on the right,
  unchanged untinted, with the connector between them.

### Break

Widening the paint bound back to the whole document fails, with 0 compile
errors, on "a comparison ten times longer paints no more than a constant factor
more line backgrounds".

An earlier break attempt was invalid and is recorded because the pattern has now
recurred three times this session: the edit left a variable unused under
`-Werror`, so the target failed to build and `ctest` reported `Passed` against a
stale binary. A break is evidence only when the build succeeded.

The all-changed 20,000-line case the test first used was **refused by the core**
— that comparison is quadratic and exceeds `MaximumMatrixCells`. The limit
behaved correctly; the test was rescaled to a computable one.
