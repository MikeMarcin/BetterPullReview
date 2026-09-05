---
schema: "implement-rfc.card.v1"
id: "BPR-0204"
title: "The selection contrast obligation checks a colour the panes never paint"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0194"]
---

# The selection contrast obligation checks a colour the panes never paint

Candidate C was accepted at attended review on 2026-08-07 and **cannot be
applied**, because setting `code_selection_background` to `#7FB0E8` fails
`qt_appearance_profile`: about twenty syntax roles fall to 2.45–3.47:1 over it,
against the 4.50 text minimum.

The failure is real. What it measures is not.

## The discrepancy

`code_selection_background` is in `code_canvas_backgrounds`, so every syntax
role is required to reach 4.50:1 **over it, as an opaque fill**. The diff panes
never paint it that way. `append_selection_band` and `set_line_selection` both
composite it at alpha 72, so the background syntax is actually drawn over is the
28% composite:

| Light, candidate C | worst syntax role |
|---|---|
| as an opaque fill — what the contract checks | 2.46:1 |
| as painted, 28% over `#FFFFFF` | **4.51:1** |

Every shipped profile shows the same gap:

| profile | selection | painted | worst syntax opaque | worst syntax painted |
|---|---|---|---|---|
| Light (current `#D8E9FF`) | `#D8E9FF` | `#F4F9FF` | passes | passes |
| Light (candidate C) | `#7FB0E8` | `#DBE9F9` | 2.46:1 | 4.51:1 |
| Dark | `#193151` | `#101A27` | 4.54:1 | 6.06:1 |
| High contrast light | `#FFE877` | `#FFF9D9` | 7.06:1 | 8.19:1 |
| High contrast dark | `#002D6C` | `#000D1E` | 7.04:1 | 10.41:1 |

So the obligation has always been checking a configuration that does not
render. The current palettes pass it by being pale enough that the question
never arose. Candidate C is the first value where the model and the pixels
disagree about whether the product is accessible.

## Why this is not simply a check to relax

The role is **also used opaquely**. `appearance_profile.cpp` assigns it to
`QPalette::Highlight`, which Qt paints solid for list rows, menu items and
ordinary text selection. Text over those is `HighlightedText`, derived by
`readable_text`, and that pair has its own obligation which passes. But the role
genuinely has two lives, and only one of them is a wash.

Deleting `code_selection_background` from `code_canvas_backgrounds` would make
candidate C pass. **Do not do that as a way to land the colour.** It would
remove the only check standing between the code canvas and an unreadable
selection, on the reasoning that this particular value happens to be fine.

## Two honest routes

1. **Evaluate the pair against the composite.** The contrast model learns that
   this role is applied at an alpha, and checks syntax over the composited
   result. Strictly more accurate, and it would still have failed an opaque
   selection. Cost: the model gains a notion of compositing it does not have,
   and the alpha becomes part of the appearance contract rather than a
   painting detail.
2. **Split the role.** `code_selection_background` stays opaque and keeps its
   present obligation for chrome; a new role — the wash the panes composite —
   carries an obligation written for a wash. This mirrors [[BPR-0202]], where a
   new role was added rather than an existing one bent, and it makes the two
   lives of the value visible instead of implied.

Route 2 is the better architecture and the larger change. Route 1 is smaller
and makes the alpha load-bearing in a contract that currently does not know it
exists.

**Either way the margin is thin and the owner should see it:** candidate C
lands at 4.51:1 against a 4.50 minimum. That is a hundredth of a point of
headroom, and any later change to a syntax colour or to the alpha would break
it. Whichever route is taken should record that number as a constraint rather
than discover it again.

## Route 2 taken, 2026-08-07 — and it changes the answer

The contrast model now composites. `contrast_pair` gained a `Wash` role,
`code_selection_background` left `code_canvas_backgrounds`, and every syntax
role is checked over each row fill twice: bare, and again with the selection
laid over it at `selection_wash_alpha`. The alpha moved from a literal in
`diff_workbench.cpp` into `appearance_model.hpp`, because a number that decides
legibility belongs in the contract that checks legibility.

### It found four real failures in what we ship today

Before any candidate was applied, against the current `#D8E9FF`:

| variant | pair | measured | minimum |
|---|---|---|---|
| Light | `syntax_escape_sequence` over washed `diff_intraline_removed_background` | 449 | 450 |
| High contrast light | `syntax_keyword` over washed `diff_intraline_added_background` | 698 | 700 |
| High contrast light | `syntax_escape_sequence` over washed same | 698 | 700 |
| High contrast light | `syntax_type` over washed same | 696 | 700 |

Selecting an intraline-edited line has been putting text below the published
minimum, in the shipped palettes, unnoticed — because the old model asked about
a fill nobody paints instead. Repaired by lightening two band values by amounts
no one will see: `#FFE1DE` to `#FFE2DF`, and `#95FF95` to `#99FF99`.

### And it says candidate C cannot ship as it stands

With `#7FB0E8` the same sweep reports **53 failing pairs**, across every diff
band — added, removed, both intraline bands, and moved. Not a marginal miss:
the worst sits at 3.82:1 against 4.50.

The cause is structural rather than a bad value. The light syntax colours were
fitted against near-white backgrounds. A wash dark enough to see drops the
composite's luminance to roughly 0.64, which costs about a third of every
ratio. Blending C back toward white does not rescue it — at 24% of the way to
white the worst pair still reads 4.06:1, and by then ΔE against the band it
sits on has fallen to 9.7, below the point the owner rejected.

**So the trade is not selection-visibility against taste. It is
selection-visibility against syntax legibility, and at alpha 72 in the light
profile the two cannot both be had.** Four ways out, none of them free:

1. **Refit the light syntax palette** against washed backgrounds, the way
   BPR-0117 refitted against bare ones. Largest change, and the only one that
   keeps both the owner's colour and the published minimum.
2. **Mark the selection without washing the text background** — a left edge bar
   or outline at full opacity. This is route 2 of [[BPR-0194]]'s original pair,
   which RFC-BPR-0006 already mandates for state anyway. Nothing composites
   over the glyphs, so the marking can be as strong as wanted.
3. **Wash only the gutter or a margin strip**, leaving the code surface alone.
4. **Accept a lower minimum on selected lines.** Rejected: it is a published
   figure, and the whole point of this card was that the old model let an
   unmet obligation look met.

Route 2 is the one with a reference behind it and the one the accessibility
requirement already asks for. It also answers the owner's "eye strain" note
more directly than any wash can, because an edge marker does not compete with
the text at all.

## Required direct evidence

- The chosen model, applied, with candidate C landing and the measurement taken
  from rendered pixels rather than from the palette.
- An opaque selection at the same colour still failing, so the check is shown
  to have teeth rather than to have been routed around.
- The syntax role that sits closest to the minimum is named in a test, so that
  a future palette edit that erodes the 0.01 fails loudly.
