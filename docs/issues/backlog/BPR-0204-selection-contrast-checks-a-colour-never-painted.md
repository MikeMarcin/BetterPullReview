---
id: BPR-0204
title: The selection contrast obligation checks a colour the panes never paint
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0194]
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

## Required direct evidence

- The chosen model, applied, with candidate C landing and the measurement taken
  from rendered pixels rather than from the palette.
- An opaque selection at the same colour still failing, so the check is shown
  to have teeth rather than to have been routed around.
- The syntax role that sits closest to the minimum is named in a test, so that
  a future palette edit that erodes the 0.01 fails loudly.
