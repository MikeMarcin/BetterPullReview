---
schema: "implement-rfc.card.v1"
id: "BPR-1142"
title: "Compact the per-line measurement record: a format key instead of a copied format list, compact width and height, and an in-band reuse marker"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"origin":"owner direction on BPR-1123's per-line record, 2026-09-25, in the coordinating chat","origin_card":"BPR-1123","related_cards":["BPR-1123","BPR-1032","BPR-1043","BPR-0967"],"reservation":"docs/issues/_reservations/BPR-1142--BPR-1142--940688e9ba6f.json"}
---
# BPR-1142: Compact the per-line measurement record: a format key instead of a copied format list, compact width and height, and an in-band reuse marker

## Owner direction

On 2026-09-25, in the coordinating chat, about [[BPR-1123]]'s per-line record
in `src/adapters/qt/src/measured_alignment_axis.cpp`:

> Looks like you could nearly half or quarter that by not storing width and
> height as qreals which appear to be doubles. And then quiet_NaN is a really
> weird default value to use.

> Yes do the hash for the format key., or a generation counter. Whichever fits
> best. For width and height float or like a 24.8 fixed point, whichever is
> more appropriate.

**Amendment, the same day**, after the first brief asked for an explicit reuse
flag or state field:

> I dunno about adding a flag, that seems like bloat. I just think introducing
> nans is unlikely to be the best tombstone if it's just a marker

So there is no flag and no state field. The "never reuse" marker stays in an
existing field, as a value no real measurement can have, and it is not NaN.

## Finding

[[BPR-1123]] keeps, for each line of an unwrapped source pane, the height and
natural width a transient layout measured and the formats it was measured
with:

```cpp
struct recorded_block { qreal Height = quiet_NaN; qreal Width = 0.0; QList<QTextLayout::FormatRange> Formats; };
```

That is 40 bytes a line. Each pane's `std::vector` grew by `push_back` to
32,768 entries on the large review's 20,000-line panes, and
`std::vector::clear()` keeps capacity, so about 2.5 MiB stayed allocated after
the comparison was cleared, until the application was destroyed. BPR-1123
measured it: 2,621,440 bytes for both panes, at `large_file_ready` and at every
cleanup sample after it.

## Design

### The format key: a 64-bit hash, not a generation counter

A generation counter would need a per-block signal that a block's formats
changed. The document gives none for the change that matters:

- **The review panes highlight by writing the block's own layout.**
  `apply_syntax_format_ranges` (`code_sample_painter.cpp`) calls
  `QTextLayout::setFormats` and then `QTextDocument::markContentsDirty`. The
  product has no `QSyntaxHighlighter`, so no `rehighlight` hook exists.
- **Neither revision moves.** A format-only change leaves the document's
  `revision()` and the block's `QTextBlock::revision()` where they were, and
  the block's `userState()` too. The new test asserts all three.
- **A counter the product kept itself would not fit.** A counter for the whole
  pane would change whenever the highlighter formats newly visible blocks,
  which happens on scroll, and every block would be measured again. A counter
  per block would have to live in the block (its user state or user data), and
  every writer of formats would have to remember to advance it: a writer that
  forgot would get a stale height and nothing would say so.

So the key is a hash of what the layout actually reads. It is a 64-bit key,
folded with splitmix64's finalizer, over:

- the number of ranges, and each range's start and length, in order. The
  positions decide where the layout splits its runs, and later ranges
  override earlier ones;
- for each range, the character format properties `QTextEngine` reads when it
  lays the run out: every font property (`FontCapitalization` through
  `FontVariableAxes`, and the two old letter-spacing and stretch IDs), the
  vertical alignment, the superscript, subscript and baseline offsets, and the
  inline object's type and index. Each is keyed by its ID and its value.

Colours, outlines, underline colours, tool tips and anchors are only painted,
so they are not keyed, and recolouring a block reuses it. That is the one
place where reuse differs from BPR-1123, which compared the whole list and
measured a recoloured block again: a theme change now reuses what it cannot
move. Underline, overline and strike-out are painted too, but Qt reads them
into the font, so they are keyed with it. A property whose value is not a
number, string or string list (font features and variable axes, which the
product never sets) makes the list unkeyable, and such a block is never
reused. A `static_assert` fails the build if Qt's font property range moves,
so a Qt that adds one has its list reviewed.

**Collisions.** Two different lists share a key with probability 2^-64. For
a block whose formats changed, a shared key would reuse its old height and
width. Its line in the comparison axis would be misplaced by the difference,
and every line below it with it, until the pane is next measured afresh (a
text, font, DPI or record change). The panes themselves paint from their own
layouts, so the text is never wrong; the widest-line rule could also miss that
line until Qt lays it out itself. The key is not keyed against an adversary:
colliding formats would take a crafted file against a fixed, public mixer, and
would misplace ribbons, nothing more. Heights stay finite and non-negative,
so the axis stays valid.

### Height and width: Qt's own 26.6 fixed point

The owner offered `float` or a 24.8-style fixed point. The record uses a
fixed point with Qt's own split, 26.6 in an `int32_t` (1/64 px), for both:

- **It is exact for everything the layout produces.** `QTextLayout` computes
  in `QFixed`, 26.6 in 32 bits. `QTextLine::height()` rounds each line up to
  a whole pixel, so a block's height is a whole number of pixels, and a
  natural width is a whole number of 1/64 px. The document margin is the only
  term off that grid, so the record leaves it out and the reader adds it back
  in the same order `QPlainTextDocumentLayout` does. Every value then comes
  back bit for bit.
- **Fractional scales change nothing.** At 125, 150 and 200% the layout still
  works in logical pixels on the same 1/64 px grid; only the font metrics
  differ. The precision test measures all four scales.
- **`float` is exact on that grid only below 262,144 px.** Its 24-bit
  significand holds 1/64 px steps only up to 2^18 px. A minified one-line file
  of 100,000 characters at about 7 px is 700,000 px wide, where a float's step
  is 1/16 px.
- **24.8 spends two bits on precision Qt never produces** and pays for them in
  range: 8,388,607 px signed, against `QFixed`'s 33,554,431.
- **Nothing saturates or rounds.** A value that is not a whole number of
  layout units within 32 bits (beyond `QFixed`'s own range, or not finite) is
  not recorded: the block gets the marker and is measured again next time.
  The record is exact or it is silent.
- **Conversion is a multiply and a compare** each way, nothing beside the
  shaping it saves (about 85 µs a line, [[BPR-1123]]).

### The marker: format key 0

Following the amendment, the marker is in-band: `not_reusable`, format key 0.
A real hash that comes out as 0 is mapped to 1, so no format list has key 0,
and the lookup refuses key 0 outright: the marker matches nothing, itself
included. A zero-initialised entry is therefore "nothing recorded". A block
measured natively (hidden, or in a wrapping pane), a list the key cannot read,
and a value the record cannot hold all record key 0. Height 0 was the other
candidate, but a block's height is only positive in practice, not by
construction, and the key is checked first anyway.

### The struct and its storage

```cpp
struct recorded_block { std::uint64_t FormatKey = not_reusable; std::int32_t Height = 0; std::int32_t Width = 0; };
static_assert(sizeof(recorded_block) == 16U, "a recorded block is 16 bytes a line");
```

16 bytes a line, from 40. The record is sized once, to the document's block
count, when it binds to the pane's facts, instead of growing by `push_back`
to the next power of two: 20,000 lines take 320,000 bytes, not 32,768 × 40.
The separate block-count fact is gone: the record matches the document when
it has as many entries as the document has blocks.

**`clear()` frees the storage.** Both of the workbench's `clear()` calls come
before the pane's text is replaced or emptied (`diff_workbench.cpp`, on a new
publication and when the comparison is cleared), so a record is never reused
across them. Keeping a large file's capacity bought only one allocation on
the next comparison and held the memory while no comparison was open. The
record binds afresh on the next measurement.

## Acceptance

- AC1: The card quotes the owner's direction and amendment, and states each
  design choice with its reasons: the format key against a generation counter
  (with the collision consequence), `float` against a fixed point, the marker,
  and whether `clear()` releases capacity.
- AC2: `recorded_block` is 16 bytes by `static_assert`, holds no NaN and no
  flag, and keeps its marker in-band. A block marked not reusable is measured
  again, with a negative control.
- AC3: Heights and widths are the native layout's, bit for bit, at 100, 125,
  150 and 200% (`qt_diff_axis_scale_*`, with a new 150 row), including a
  precision test, with negative controls.
- AC4: A format-only change (the same text, new highlighting) measures the
  block again; a negative control where the key ignores format changes fails.
- AC5: No regression on macOS: `qt_diff_workbench_scale_*`,
  `qt_diff_axis_scale_*` and [[BPR-1123]]'s and [[BPR-1032]]'s integration
  rows pass.
- AC6: Measured on macOS against current main: live heap at
  `large_file_ready` and at the cleanup samples, the large-review budgets, and
  [[BPR-1043]]'s refresh-stall median. The saving is reported.
- AC7: The Windows and Linux runs are recorded as owed, with their commands.
