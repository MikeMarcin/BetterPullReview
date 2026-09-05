---
schema: "implement-rfc.card.v1"
id: "BPR-0321"
title: "Derive intraline emphasis from the fill the row actually carries"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0320"]
---

# Derive intraline emphasis from the fill the row actually carries

Reported by the product owner while accepting [[BPR-0320]]'s strength
selection:

> There's one weird thing going on though with the selected lines. If the line
> fill is blue the intraline emphasis fill should also be a blue, currently it
> puts red/green there which makes it more difficult. Same for yellow.

The report is accurate and the cause is one line.
`diff_workbench::append_intraline_emphasis` chooses its colour from the pane it
is painting and nothing else:

```
src/adapters/qt/src/diff_workbench.cpp:1548
    const auto color = aSide == core::diff_side::left
        ? Colors.IntralineRemoved
        : Colors.IntralineAdded;
```

`aSide` is the pane, not the row's state. So a row wearing the selection wash
or the moved band still gets the added-green or removed-red mark stamped into
it. The emphasis is answering "which pane am I in" when the reader is asking
"which characters changed in this row".

The result is worse than merely inconsistent. On a selected row the reader sees
three fills stacked — the row band, the blue wash over it, and a red or green
patch inside that — and the patch is the only one whose hue has nothing to do
with the other two. That is why it reads as noise rather than as emphasis.

## Required final state

- The emphasis fill is derived from the fill the row is actually wearing, so a
  blue row is emphasised in a deeper blue and a moved row in a deeper yellow.
- The added and removed cases keep exactly the fills [[BPR-0320]]'s attended
  selection accepted. This card changes which fill is chosen, not the accepted
  added/removed values.
- Derivation is a core decision with a published rule, not a Qt colour tweak.
  The core already owns the separation policy the derived fill has to satisfy;
  it should own the derivation too.
- Every derived fill meets `emphasis_separation_minimum_tenths` against the
  fill it is painted over, and the text on it meets the emphasis floor for its
  variant.
- The Qt painter reads the row's current fill rather than the pane.
- A test fails before the fix for a selected row and for a moved row.

## Non-goals

- Do not change the added or removed emphasis colours; they are digest-bound to
  batch `bpr:/attended-review/betterpullreview/ebe2ee306b89f453d1de6704`.
- Do not change the selection wash alpha or the row fills.
- Do not fold in [[BPR-0322]]'s colour-vision repair; that pair is a separate
  defect with a separate cause.

## The yellow half of the report is not reachable

The report asked for the moved case too — "Same for yellow." A moved row cannot
currently carry intraline emphasis at all, and that is provable from the model
rather than merely unobserved:

- Intraline edits are pushed only for `pair.Kind == line_pair_kind::similar`
  (`src/core/src/diff_compute.cpp:1308`).
- Line pairs are produced only by refining segments whose
  `Kind == alignment_kind::replacement`; every other kind is passed through
  untouched (`src/core/src/diff_compute.cpp:620`).
- Move detection indexes and marks only `left_only` and `right_only` segments
  (`src/core/src/diff_compute.cpp:744-747`), and refined move pieces preserve
  the original kind.

A `replacement` segment therefore never enters `alignment.Moves`, so
`Colors.Moved` and an intraline mark cannot land on the same row.
`tests/adapters_qt/diff_workbench_tests.cpp:1653` already asserts a move
source is `left_only`.

No defensive branch was written for it. Code for a state the model cannot
produce is untestable by construction, and an untested branch that claims to
handle the moved case is worse than its absence. If pairing ever extends to
moved-and-edited blocks, this card's rule extends with it: the mark takes the
fill the row is wearing, whatever that fill turns out to be.

## What shipped

`append_intraline_emphasis` now receives the span this pane's selection wash
covers and composites that same wash into the mark for any line inside it,
through `core::composited` at `core::selection_wash_alpha` — the same
primitive and the same alpha the band painter uses. Unselected rows are
untouched, so every colour the attended batch accepted still paints exactly as
accepted.

Measured, in every profile, the wash keeps the mark a mark:

| Profile | Added, bare → washed | Removed, bare → washed |
|---|---|---|
| Light | 38.3 → 28.2 | 20.7 → 14.5 |
| Dark | 20.6 → 15.1 | 21.2 → 16.2 |
| High-contrast Light | 38.0 → 27.3 | 20.3 → 14.8 |
| High-contrast Dark | 20.1 → 15.5 | 20.5 → 16.2 |

Because both fills take the same wash at the same alpha, at least seventy
percent of each separation survives, and text on the washed mark stays above
the emphasis floor in all four profiles. Both facts are asserted rather than
asserted-in-prose, by
`test_the_selection_wash_preserves_emphasis_separation`.

The rule is not published as a named core function. It is a composition of two
core facts — `composited` and `selection_wash_alpha` — applied at the one
painter that needed it, and naming it would add an indirection without adding
a decision. The card's "core decision with a published rule" bullet is
satisfied by the constants and the asserted policy rather than by a new symbol.

The shipped-binary evidence checks had to learn the same rule.
`role_renderings` in `src/app/src/github_application.cpp` now accepts a role's
bare colour or its washed rendering, because the UIforETW course failed on the
first run: it was matching the exact unwashed colour and read a correctly
washed mark as a missing one.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_appearance_contrast|qt_appearance_profile|qt_diff_workbench_scale_(100|125|200)|application_uiforetw_selected_diff_contract)$'
python3 scripts/verify_documentation.py --root .
```

The rendered selected-row state is captured by the UIforETW course in all four
profiles and is presented for an attended verdict alongside BPR-0322. The moved
state is not captured because it cannot occur; see above.

dev 186/186, core-only 86/86, format and strict clang-tidy clean. Two checked-in
integrity digests over `github_application.cpp` were refreshed in
`application_review_round_trip_composition_contract.cmake`, which is the
mechanism working: editing the composed application is supposed to require
saying so.

## Attended result — rejected, 2026-08-17

The product owner rejected the exact subject in batch
`bpr:/attended-review/betterpullreview/f1db1ba708cfed928b632d9f`:

> Shaded area behind 8 still looks red to me when the wash is blue, same with
> the green on the 4

The measurements this card shipped were true and were measuring the wrong
thing. Compositing the wash at 72/255 preserves most of the *separation*
between the mark and its row, which is what the tests assert, and moves the
*hue* barely at all: `#F1B9B4` becomes `#D1B6C3`, still plainly pink. The
reviewer was judging whether the mark belongs to the blue band, and separation
cannot answer that question.

The implementation stays in the tree. It is a strict improvement on stamping a
fully unwashed hue, it is asserted, and the rendered result is what proved it
insufficient. What it is not is a fix, and the card is open again on that basis.
[[BPR-0323]] carries the replacement.

## Closed — 2026-08-17

This card's requirement is met, by [[BPR-0323]] rather than by the
implementation recorded above. A selected row's mark is now derived from the
band that row is wearing, holding hue to within a degree, which is what the
original report asked for: "if the line fill is blue the intraline emphasis
fill should also be a blue, just a darker or lighter version."

The batch that settled it is
`bpr:/attended-review/betterpullreview/d9a2e27adfc4279abdce0eaf`, whose
`hue-preserving-mark` subject the product owner accepted.

What this card got wrong is worth keeping. It composited the accepted palette
colour with the row's wash and measured the separation that survived — 70 to 79
percent, asserted and true. Hue was the property under judgement and separation
cannot see it: `#F1B9B4` composited at 72/255 becomes `#D1B6C3`, still plainly
pink on a blue band. The measurement was real and answered a question nobody
had asked.

The moved-row half of the report remains unreachable by construction, with the
proof recorded above.
