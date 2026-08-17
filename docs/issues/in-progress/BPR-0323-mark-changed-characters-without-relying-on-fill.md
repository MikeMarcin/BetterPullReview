---
id: BPR-0323
title: Mark changed characters without relying on fill colour alone
status: In Progress
rfc: RFC-BPR-0006
depends_on: [BPR-0320]
---

# Mark changed characters without relying on fill colour alone

This card supersedes the approach [[BPR-0321]] and [[BPR-0322]] were reopened
on, so it depends on the accepted fill selection rather than on either of them.
Both close on this card's verdict, not the other way round.

Three attended results now say the same thing, and it is worth stating once
rather than discovering a fourth time.

| Batch | What was tried | Result |
|---|---|---|
| `intraline-emphasis-strength-v001` | Five fill strengths, unselected rows | C3 accepted |
| `intraline-emphasis-followups-v001` | The accepted fill under the row's wash | Rejected: "still looks red to me when the wash is blue" |
| `intraline-emphasis-followups-v001` | Three high-contrast dark fill pairs | All rejected: "the least contrasted examples" |

A fill tint works on an ordinary unselected row in the two standard profiles.
It fails on a selected row, because the reviewer is asking whether the mark
belongs to the band it sits in and a tint keeps its own hue. It fails in the
high-contrast profiles, because the row fills there are already at the extremes
of the range and a tint has nowhere to travel — the profile a person selects
*because* they need more contrast shows the least of any.

The core has said this since before the first batch.
`pairs_requiring_non_color_indicator` documents that a distinction colour cannot
carry "must be carried with an icon, a line prefix, a gutter mark, an outline,
or an accessible name". Nothing in this project paints one for intraline
emphasis. That absence is the defect.

## Required final state

- Changed characters carry a mark that does not depend on the fill being able to
  differ from its row: a rule, a border, a weight, or some combination the
  attended review selects.
- The mark is visible on an unselected row, on a row wearing the selection wash,
  and in all four appearance profiles, with the high-contrast profiles at least
  as emphatic as the standard ones rather than less.
- The accepted [[BPR-0320]] fills stay exactly as accepted wherever they are
  still used. This card adds a signal; it does not reopen the colour selection.
- The high-contrast dark colour-vision obligation recorded for [[BPR-0322]] is
  discharged by the new signal, or explicitly re-recorded with a measurement
  showing why it is not.
- The signal is a core decision with a published rule and a threshold that can
  fail, not a Qt styling choice.
- A test fails before the change for a selected row and for a high-contrast
  profile.

## Non-goals

- Do not reopen the C3 fill selection.
- Do not weaken any contrast floor to make a fill work; that route is exhausted
  and this card exists because of it.
- Do not add a signal that reflows monospace columns. A mark that moves the code
  under it is worse than a mark that is hard to see.

## What to put in front of the owner

Renders, not hex values or descriptions. The two useful comparisons are a
selected row in the light profile and any row in high-contrast dark, since those
are the two states that have failed. Candidates worth rendering:

- an underline beneath the changed characters, in a palette-derived colour;
- a full outline around the changed run;
- the changed characters in the bold weight of the code font;
- combinations of the above with and without the accepted fill retained.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_appearance_contrast|qt_appearance_profile|qt_diff_workbench_scale_(100|125|200)|application_uiforetw_selected_diff_contract)$'
python3 scripts/verify_documentation.py --root .
```

## Implemented, pending verdict

The mark is a rule under the changed characters, drawn in `syntax_normal`. That
colour is not a new decision: the required-contrast audit already holds every
syntax role to its floor over every canvas background, including both intraline
fills and the selection wash composited over them, so the rule inherits a
legibility guarantee rather than needing a threshold of its own.

The fill keeps doing what it was accepted for and stops doing what it could not.
An unselected row in any profile keeps its accepted C3 colour. A selected row
takes `core::derived_emphasis_fill` of its own washed band — same hue to within
a degree, a separation of 9.0 rather than 20.0, because the rule carries the
mark there and spending more would cost text contrast the band already spent.

`derived_emphasis_fill` moves toward the most vivid rendering of the fill's own
hue, lifted to full brightness on a dark ground so a near-black row has
somewhere to travel, and returns the fill unchanged when the minimum cannot be
reached rather than inventing a colour.

The shipped evidence checks learned the rule twice over: `role_renderings` now
carries three legitimate renderings of one role — bare, washed, and derived —
and a span counts as marked when it carries the rule even with no fill.

dev 186/186, core-only 86/86, format and strict clang-tidy clean, documentation
gate green. Both composed-application digests were refreshed. The batch is
`bpr:/attended-review/betterpullreview/baf053d332b2b9b7008a1c0f`.

## Attended result — rejected, and it corrected the premise

Batch `bpr:/attended-review/betterpullreview/baf053d332b2b9b7008a1c0f` was
rejected:

> I don't like the underline and my note was not that a dark tint couldnt carry
> this, in fact that tint worked very well for the normal light and dark mode
> versions. My note was that it did not work for the high contrast mode. That is
> to say for high contrast we should consider using a DIFFERENT color fill,
> maybe a yellow on red or something high contrast like that, you pick the
> correct color pairings. As for the normal non-high-contrast modes this is the
> wrong direction for the blue selected row in particular. And no underlines, i
> dont like them.

The table at the top of this card drew the wrong conclusion from the second
rejection, and this is the correction. "The least contrasted examples" was
about the high-contrast profiles specifically. It was read here as evidence
that a fill cannot carry the mark anywhere, and generalised into a structural
signal applied to all four profiles and every row. The tint was never the
problem in light and dark; the reviewer says it works well there, and the
accepted C3 palette stands untouched.

Two things follow, and only two:

- The underline is gone. It is rejected as an instrument, not as a parameter,
  so no variant of it is worth rendering.
- The high-contrast profiles need a different and genuinely bolder colour
  pairing rather than a deeper shade of their own rows.

The mechanism built for the wrong conclusion is still worth keeping. A fill
bright enough for a high-contrast profile cannot let that profile's syntax
colours read on it, so `emphasis_foreground` and
`emphasis_needs_own_foreground` let such a fill carry its own text colour, and
the contrast audit measures what is actually painted instead of a combination
the panes never produce. That is what makes the bolder pairings possible at
all.

The selected row in the standard profiles remains open. The reviewer has still
only seen it with an underline drawn over it, so the next batch shows it plain.

Batch `bpr:/attended-review/betterpullreview/8f175d37c3b12a3e17ca6fda` carries
both remaining questions.
