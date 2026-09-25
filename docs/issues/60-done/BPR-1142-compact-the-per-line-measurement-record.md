---
schema: "implement-rfc.card.v1"
id: "BPR-1142"
title: "Compact the per-line measurement record: a format key instead of a copied format list, compact width and height, and an in-band reuse marker"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"origin":"owner direction on BPR-1123's per-line record, 2026-09-25, in the coordinating chat","origin_card":"BPR-1123","related_cards":["BPR-1123","BPR-1032","BPR-1043","BPR-0967"],"reservation":"docs/issues/_reservations/BPR-1142--BPR-1142--940688e9ba6f.json"}
acceptance: [{"criterion":"AC1 The card quotes the owner's direction and amendment, and states each design choice with its reasons","outcome":"pass","revision":"b220caad","evidence":{"kind":"observation","detail":"Owner direction (2026-09-25) and the no-flag amendment quoted verbatim. Decisions: a 64-bit hash of each range's position, length and the character properties QTextEngine reads (no generation counter: a format-only change moves neither the document's revision nor the block's revision or user state, the product has no QSyntaxHighlighter, a pane-wide counter would change on every scroll-highlight); collision consequence 2^-64, a misplaced axis line until the next fresh measurement; Qt's 26.6 fixed point for height and width (exact for every layout value; float exact only below 262,144 px on the 1/64 grid; 24.8 caps at 8,388,607 px); format key 0 as the in-band marker; clear() frees storage because both workbench clear sites precede replacing or emptying the pane's text.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#design"}},{"criterion":"AC2 recorded_block is 16 bytes by static_assert, holds no NaN and no flag, and keeps its marker in-band; a marked block is measured again, with a negative control","outcome":"pass","revision":"b220caad","evidence":{"kind":"command","detail":"static_assert(sizeof(recorded_block) == 16U) compiles (Apple clang 21, -Werror). Fields: uint64 FormatKey (0 = not_reusable; a real hash of 0 maps to 1; lookup refuses 0), int32 Height, int32 Width. test_format_key: a block measured while hidden is measured again once shown (59 of 60 reused), then reused. Negative control 'marker matchable' fails at 'a block recorded as not reusable is measured again' at 1.0/1.25/1.5/2.0.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#verification-macos"}},{"criterion":"AC3 Heights and widths are the native layout's, bit for bit, at 100/125/150/200%, with a precision test and negative controls","outcome":"pass","revision":"b220caad","evidence":{"kind":"command","detail":"qt_diff_axis_scale_100/125/150/200 pass (0.67/0.38/0.44/0.51 s; 150 is new). test_recorded_block_precision at each scale: every block reused after a viewport and a width change, boundaries equal the native ones exactly, a width 1/64 px wider than the widest before it is laid out natively across four 1/64 px shifts. Controls: rounding to 1/16 px fails at 'a recorded width 1/64 px wider ... laid out natively'; a refusing 1/16 px grid fails at 'only the block whose formats changed is measured again'.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#verification-macos"}},{"criterion":"AC4 A format-only change measures the block again; a negative control where the key ignores format changes fails","outcome":"pass","revision":"b220caad","evidence":{"kind":"command","detail":"test_format_key: bold keyword on the same text measures that block again and only that block, after asserting document revision, block revision and user state unchanged; a larger size and a moved range each measure the block again to its native height; recolouring and an equal list reuse all 60. Controls: a constant key fails at 'boundary 4 differs from the native one' / 'a block whose formats changed is measured again'; a key ignoring properties fails at 'a block whose keyword turned bold is measured again, and only that block'; keying colour fails at 'recolouring every block reuses every recorded block'.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#verification-macos"}},{"criterion":"AC5 No regression on macOS in qt_diff_workbench_scale_*, qt_diff_axis_scale_* and BPR-1123's and BPR-1032's integration rows","outcome":"pass","revision":"b220caad","evidence":{"kind":"command","detail":"27 of 27 on the candidate build (fa2fc40a plus this card's five files, C++ equal to b220caad), ctest -j1, 2026-09-25T07:24:53Z-07:29:18Z: qt_diff_axis_scale_100/125/150/200, qt_diff_workbench_scale_100/125/200, qt_syntax_pane_composition, qt_comparison_scroll_controller_scale_*, qt_diff_link_map_scale_*, qt_composed_github_scale_*, qt_workbench_window_scale_*, application_diff_workbench_public_path, _shutdown_public_path, application_workbench_composition_contract, large_review_fixture_contract, large_review_replay_contract, architecture_boundaries, architecture_boundary_contract. clang-format 23.1.1 clean; tidy only the pre-existing stale_after; documentation gate DOCUMENTATION_OK.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#verification-macos"}},{"criterion":"AC6 Measured on macOS against current main: live heap at large_file_ready and at cleanup, the large-review budgets, and BPR-1043's refresh-stall median; the saving is reported","outcome":"pass","revision":"b220caad","evidence":{"kind":"command","detail":"A/B from scratch builds, alternating, 3+3 runs each of resources and loaded close: large_file_ready 105.12 -> 103.24 MiB (1,962,416-1,970,304 B saved, the old 2,621,440 B record now 655,360 B for 2x20,001 blocks); after prune, cache deletion and shutdown drain 2.50 MiB saved (2,616,752-2,624,640 B). Budgets all pass in both builds (footprint 160/184 MiB, cleanup 84 MiB, destroyed 32 MiB). Benchmark unchanged. History refresh median 69.2/67.9/70.9 ms (candidate) vs 71.7/69.3/69.7 ms (base), BPR-1043 68.8 ms; the history row fails in both builds, 3 of 3 runs each, in varying phases under the concurrent writer's load, recorded under Found, not changed.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#measured-macos"}},{"criterion":"AC7 The Windows and Linux runs are recorded as owed, with their commands","outcome":"pass","revision":"b220caad","evidence":{"kind":"observation","detail":"The card's 'Owed: Windows and Linux' section lists the exact cmake/ctest commands for bpr-main-win (MSVC Debug) and bpr-main-linux (GCC Debug): the same 27 rows including qt_diff_axis_scale_150. The large-review memory rows are macOS-only, so none is owed elsewhere.","path":"docs/issues/60-done/BPR-1142-compact-the-per-line-measurement-record.md#owed-windows-and-linux"}}]
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

## Implemented (`b220caad`)

`src/adapters/qt/src/measured_alignment_axis.cpp`: `format_key` (with
`layout_properties`, `value_key` and `range_key`), `to_layout_units` and
`from_layout_units`, `recorded_block` with its `static_assert`, `compact`, and
`pane_block_measurer::height` reading the record through them.
`transient_extent` returns the layout's bottom and natural width without the
margin. `pane_block_heights::state::bind` sizes the record to the document's
blocks, and `clear()` frees it. The header's contract and
`docs/architecture/diff-pipeline-and-rendering.md` section 9 say the same.
Behaviour is BPR-1123's except for recolouring, which now reuses.

**Tests** (`tests/adapters_qt/measured_alignment_axis_tests.cpp`):

- `test_format_key`: 60 highlighted lines. Recolouring every block, and giving
  each block an equal list of its own, reuse all 60. Turning one keyword bold
  (the panes' own emphasis) measures that block again, and only that block,
  and the test first asserts that the document's revision and the block's
  revision and user state did not move. A larger size on the same text, and
  the same format over a different range, each measure that block again, to
  its native height. A block measured while hidden is recorded with the
  marker. Once it is shown again, it is measured again (59 reused), then
  reused.
- `test_recorded_block_precision`: 96 lines at a 10.3 pt pane font, with
  11.7 pt unhinted runs, absolute letter spacing (0.37 px) and word spacing
  (1.13 px), superscript, a 13% baseline offset, and blocks of two visual
  lines (U+2028). Every block is reused after a viewport change and after a
  width change, and the boundaries equal the first measurement and the pane's
  native ones bit for bit. The two widest lines differ by exactly 1/64 px, lie
  between the pane's first and last pages, and run past the viewport. The
  width change empties their layouts (asserted), and the widest line must
  still be laid out natively from its recorded width. The pair's widths step
  through four consecutive 1/64 px shifts, so a record that rounded widths to
  any coarser grid would put the pair on one value in at least one shift.
- `qt_diff_axis_scale_150` (`tests/adapters_qt/CMakeLists.txt`) joins 100,
  125 and 200.

## Verification (macOS)

The station is an M4 Pro (14 cores, 48 GiB) with macOS 26.6.2 (25G83), Apple
clang 21.0.0, a Debug build and Qt 6.11.2, running offscreen.

**Isolated builds.** Another implementer was building and testing in
`build/dev` from this checkout throughout. So both builds here come from
`git archive` exports into scratch: the base is `fa2fc40a`, and the candidate
is `fa2fc40a` plus this card's five files. Nothing in `src/` or `tests/`
changed between `fa2fc40a` and `27b1e900`, so the candidate's C++ is
`b220caad`'s. Both were configured with the `dev` preset and the shared CPM
cache. The candidate was built in full and the base only for the benchmark
target.

**The rows pass: 27 of 27** on the candidate, `ctest -j1`, 07:24:53Z to
07:29:18Z:

| Rows | Result |
| --- | --- |
| `qt_diff_axis_scale_100` / `_125` / `_150` / `_200` | pass, 0.67 / 0.38 / 0.44 / 0.51 s |
| `qt_diff_workbench_scale_100` / `_125` / `_200` | pass, 2.42 / 2.18 / 2.45 s |
| `qt_syntax_pane_composition` | pass, 0.59 s |
| `qt_comparison_scroll_controller_scale_*`, `qt_diff_link_map_scale_*` | 6 of 6 |
| `qt_composed_github_scale_100` / `_125` / `_200` | pass, 3.87 / 3.32 / 3.76 s |
| `qt_workbench_window_scale_100` / `_125` / `_200` | pass, 21.44 / 22.19 / 22.18 s |
| `application_diff_workbench_public_path`, `_shutdown_public_path`, `application_workbench_composition_contract` | 3 of 3 |
| `large_review_fixture_contract`, `large_review_replay_contract` | 2 of 2 |
| `architecture_boundaries`, `architecture_boundary_contract` | 2 of 2, the latter 104.16 s |

Run directly at `QT_SCALE_FACTOR` 1.0, 1.25, 1.5 and 2.0, the axis suite
prints `measured-alignment-axis-tests:pass`. At every scale its precision
fixture reports the widest line's natural width as 44828/64 to 44831/64 px
over the four shifts. Heights are whole pixels at every scale, the two-line
blocks included, because Qt rounds each line's height up. Widths are not.

**Negative controls.** Each is a temporary edit to
`measured_alignment_axis.cpp`, run at all four scales and restored byte for
byte (checked with `cmp`, then rebuilt):

| Edit | Fails at |
| --- | --- |
| The key is a constant: every format change ignored | "boundary 4 differs from the native one", then "a block whose formats changed is measured again" |
| The key keeps positions and lengths but ignores every property | "a block whose keyword turned bold is measured again, and only that block" |
| The marker is matchable: a hidden block recorded under its format key | "a block recorded as not reusable is measured again" |
| The record rounds to 1/16 px instead of refusing an inexact value | "a recorded width 1/64 px wider than the widest before it still has its line laid out natively" |
| The record keeps a 1/16 px grid and refuses what is off it | "only the block whose formats changed is measured again" (every fractional width refused) |
| Colour keyed: `ForegroundBrush` added to the layout properties | "recolouring every block reuses every recorded block, because colour is not laid out" |

The first fixture of the rounding control put the widest pair in the pane's
last page, which the pane lays out again after any relayout
(`QPlainTextEdit`'s scroll-bar pass), and that control passed. The pair now
sits mid-document, and the test asserts that the width change emptied its
layouts.

**Gates.** clang-format 23.1.1 is clean on the three C++ files, whose
`fa2fc40a` versions were clean. `_coding-standard/scripts/tidy.py` reports
nothing on a changed line; its one finding, `stale_after`, predates this card.
`python3 scripts/verify_documentation.py --root .` prints
`DOCUMENTATION_OK`.

## Measured (macOS)

The rows ran from the two scratch builds through CTest, `-j1`, alternating
base and candidate, 07:04Z to 08:25Z. Live heap does not depend on load.

**Live-heap growth**, in MiB, with the difference in bytes. The resources
figures are from 3 runs of each build, and so are the loaded-close ones:

| Sample | Base `fa2fc40a` | Candidate `b220caad` | Saved, bytes |
| --- | --- | --- | --- |
| `large_file_ready` (both courses) | 105.12 | 103.24 | 1,962,416 to 1,970,304 |
| `before_loaded_close` | 113.76 | 111.88 | 1,963,008 to 1,970,304 |
| `after_prune`, `after_cache_delete` | 71.54, 71.55 | 69.04, 69.05 | 2,617,776 to 2,623,344 |
| `shutdown_drained` (resources, loaded close) | 69.74, 69.66 | 67.24, 67.16 | 2,616,752 to 2,624,640 |
| `application_constructed`, `review_loaded`, `application_destroyed` | | | the same within 5 KB |

**What the record costs now.** The large review's panes have 20,001 blocks
each (the loaded-close report). At 16 bytes that is 640,032 bytes, which the
allocator holds as 2 × 327,680 = 655,360: exactly a quarter of the
2,621,440 bytes [[BPR-1123]] measured. The difference, 1,966,080 bytes, is
the middle of the measured range. After prune, cache deletion and shutdown
the record holds nothing: the whole 2.5 MiB is gone.

**Against the earlier baselines.** [[BPR-1123]] measured 104.08 MiB at
`large_file_ready` for `fbab68c9`, and 101.58 MiB for its parent `c546bd85`,
which had no record. Current main (`fa2fc40a`) reads 105.12 MiB. It has grown
1.04 MiB since `fbab68c9` through five other commits (BPR-1129, 1131, 1134,
1137 and 1140), none of which touches the record. Taking the old record's
2.50 MiB off gives 102.62 MiB with no record at all. The candidate reads
103.24 MiB, so the record now costs 0.62 MiB there, where it cost 2.50 MiB,
and nothing after cleanup.

**No budget is exceeded.** The highest figure across all runs:

| Budget | Course | Base | Candidate |
| --- | --- | --- | --- |
| Footprint growth, 160 MiB | resources | 127.4 to 128.0 | 125.3 to 126.7 |
| | loaded close | 140.5 to 142.8 | 141.3 to 142.5 |
| | cache entries (1 run each) | 87.3 | 89.9 |
| Footprint growth, 184 MiB | cache bytes (1 run each) | 153.7 | 154.4 |
| Cleanup live heap, 84 MiB | every cleanup sample | 71.55 | 69.05 |
| Destroyed live heap, 32 MiB | every course | 12.21 to 13.74 | 12.21 to 13.74 |

Both cache-byte runs pass, 372.77 s (base) and 352.83 s (candidate); so do both
cache-entry runs, 80.22 and 81.42 s. The cache-byte course's live heap before
its loaded close is 171.06 MiB for the base and 169.18 MiB for the candidate.

**The ordinary benchmark did not slow down.** `application_large_review_benchmark`,
2 runs each, alternating:

| | Candidate | Base |
| --- | --- | --- |
| `paged_review_and_git_load` | 10.81, 10.90 s | 10.71, 10.83 s |
| `large_file_selection` (largest GUI gap) | 1.99 s (113.0 ms), 2.05 s (115.0 ms) | 2.07 s (112.3 ms), 1.97 s (110.6 ms) |
| `rapid_selection_supersession` | 8.25, 8.30 s | 8.88, 8.12 s |
| Measurement input gap, p95 (largest) | 4.17 (6.98), 4.26 (6.39) ms | 4.16 (5.19), 4.19 (7.01) ms |
| Measurement batches | 84, 86 | 85, 84 |

**[[BPR-1043]]'s refresh-stall median is unchanged**, but the history course
fails in both builds on this station today. `application_large_review_history`,
across its 31 epoch refreshes:

| Run | Build | Load at start | Median largest gap | Largest | Refresh time | Result |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | candidate | 14.1 | 69.2 ms | 156.3 ms (refresh 14) | 8.62 to 11.24 s | fails |
| 1 | base | 5.3 | 71.7 ms | 152.0 ms (refresh 14) | 8.79 to 10.86 s | fails |
| 2 | base | 7.4 | 69.3 ms | 166.3 ms (refresh 6); refresh 3 119.8 ms, refresh 5 13.05 s | 8.49 to 13.05 s | fails |
| 2 | candidate | 4.2 | 67.9 ms | 73.4 ms; source selection 30 166.4 ms and 2.05 s | 8.42 to 9.44 s | fails |
| 3 | candidate | 10.5 | 70.9 ms | 100.4 ms (refresh 19); source selection 18 2.39 s | 8.71 to 12.28 s | fails |
| 3 | base | 15.7 | 69.7 ms | 83.0 ms | 8.53 to 12.03 s (refresh 2) | fails |

BPR-1043 left a median of 68.8 ms, and [[BPR-1123]] measured 67.4 ms. The
medians here, 67.9 to 71.7 ms in both builds, match those. Every run has one
to three phases over their bounds (a gap over 100 ms, a refresh over 12 s or a
source selection over 2 s), in a different place each time. Both builds fail
alike, 3 runs each, so this card does not cause the failures. The concurrent
implementer's builds and `-j8` test runs were active over the same 90 minutes,
and the 15-minute load average read 9 to 18. BPR-1123's passing run at
`fbab68c9` was taken with the station quiet.

## Owed: Windows and Linux

The Windows and Linux stations were idle when this card closed on macOS, so
their runs are owed. On each, at `b220caad` or later, in `build/dev`:

```sh
cmake --build --preset dev
ctest --preset dev -j1 --output-on-failure -R "^(qt_diff_axis_scale_(100|125|150|200)|qt_diff_workbench_scale_(100|125|200)|qt_syntax_pane_composition|qt_comparison_scroll_controller_scale_.*|qt_diff_link_map_scale_.*|qt_composed_github_scale_.*|qt_workbench_window_scale_.*|application_diff_workbench_public_path|application_diff_workbench_shutdown_public_path|application_workbench_composition_contract|large_review_fixture_contract|large_review_replay_contract|architecture_boundaries|architecture_boundary_contract)$"
```

- **Windows** (bpr-main-win, MSVC Debug with Qt's debug libraries): the same
  27 rows. They check that the key and the 16-byte `static_assert` compile
  under MSVC's warnings-as-errors. They also run the precision fixture on the
  offscreen FreeType fonts, where the panes' font resolves to Arial
  ([[BPR-0869]]), and they add the new 150 row.
- **Linux** (bpr-main-linux, GCC Debug): the same 27 rows, under GCC's
  warnings.

The large-review memory rows are macOS-only ([[BPR-0891]]), so no memory row
is owed elsewhere.

## Found, not changed

**`application_large_review_history` fails at current main on this
station.** It fails in both builds, in every run above, in a different phase
each time. BPR-1123's passing run at `fbab68c9` was taken with the station
quiet, and today's runs were not. The five source commits since `fbab68c9`
(BPR-1129, 1131, 1134, 1137 and 1140) touch neither the record nor the diff
workbench. Whether the course still passes on a quiet station is open. The
next step is one run of the row at current main with nothing else building,
and if it fails there, a card of its own.

## Closeout checkpoint

- **Candidate:** `b220caad`, the product commit. The ID was allocated in
  `fa2fc40a`, which is also the measurements' base. The card was committed in
  `0b1113da` and claimed in `27b1e900`.
- **Paths:** `src/adapters/qt/src/measured_alignment_axis.cpp`,
  `src/adapters/qt/include/bpr/qt/measured_alignment_axis.hpp`,
  `tests/adapters_qt/measured_alignment_axis_tests.cpp`,
  `tests/adapters_qt/CMakeLists.txt` and
  `docs/architecture/diff-pipeline-and-rendering.md`.
- **Not committed:** the scratch builds, row logs and JSON reports. They were
  scratch, and were pruned when this card closed.
- **Pending:** the Windows and Linux runs above, and a quiet-station run of
  `application_large_review_history` at current main.

