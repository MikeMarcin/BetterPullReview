---
id: BPR-0210
title: High-contrast light sits exactly on its minimum with no margin
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0206]
---

# High-contrast light sits exactly on its minimum with no margin

`syntax_escape_sequence`, read through the selection wash over
`diff_intraline_added_background`, measures **7.00:1** against a published
minimum of 7.00:1. Zero headroom.

Found by the ratchet added for [[BPR-0204]], which measures the tightest pair in
every profile. The other three have 5, 5 and 4 hundredths — thin, but not nil.

## Why zero is different from thin

A margin of five hundredths means an unrelated palette edit gets a warning
before it becomes a violation. Zero means the next edit that touches any of the
three roles in that composite — the syntax colour, the intraline band, or the
selection — is a failure with no intermediate signal, and the person making it
will have no reason to expect it.

It is also the profile where the obligation is strictest and the consequence
worst. High-contrast exists for readers who need it, and it is the one shipping
with no room.

## What is not yet known

Whether this is reachable at all. The profile is already near the extremes:
pure white pane, near-black text, and a yellow selection. Widening the pair may
require moving the selection colour, which would change what the owner accepted
at attended review on 2026-08-08.

Establish the achievable margin before deciding whether to spend a re-review on
it. If the honest answer is that 7.00:1 exactly is the best this profile can do,
that is worth recording as a bound rather than left looking like an oversight.

## Measured, 2026-08-08 — margin is reachable, and the cost is small

Every pair within five hundredths of the minimum is the *same shape*: a syntax
colour read through the selection wash over an **intraline** band.

| ratio | syntax role | background |
|---|---|---|
| **700** | `syntax_escape_sequence` | intraline added + wash |
| 701 | `syntax_import` | intraline added + wash |
| 701 | `syntax_keyword` | intraline added + wash |
| 704 | `syntax_escape_sequence` | intraline removed + wash |

Nothing else in the profile is close. The row bands, the pane and the moved band
all have room; the two intraline emphasis bands are the whole problem, and they
are the two values a reviewer looks at least directly.

Lightening both, and nothing else:

| lighten | intraline added | intraline removed | floor | ΔE of intraline added vs its row band |
|---|---|---|---|---|
| 0% | `#99FF99` | `#FFE1E1` | **700** | 38.0 |
| 8% | `#A4FFA4` | `#FFE4E4` | **710** | 31.4 |
| 12% | `#AAFFAA` | `#FFE5E5` | **716** | 27.7 |
| 16% | `#AFFFAF` | `#FFE6E6` | 718 | 24.6 |

So ten hundredths of margin costs 8%, and the intraline band still sits ΔE 31.4
from the row band it has to be distinguishable from — down from 38.0, and far
above the ~10 that counts as comfortably distinct. Sixteen hundredths costs 12%
and still leaves 27.7.

**The bound is not tight. 7.00:1 exactly is not the best this profile can do**,
which was the possibility this card raised and can now be dismissed.

## Deliberately not applied

This card says any colour that must move needs its candidates rendered and
re-reviewed, because the high-contrast profiles were accepted with their current
values on 2026-08-08. Two of them would move, so that stands.

The numbers above are what a re-review needs: two candidate steps, the margin
each buys, and the separation each costs. Rendering them is a small job now that
`--diff-evidence-appearance high-contrast-light` exists.

**The ratchet in `qt_appearance_profile` records 0 for this profile and should
stay at 0 until the change lands.** Raising it first would assert a margin the
shipped palette does not have.

## Superseded — required direct evidence

- The margin achievable without moving any colour the owner accepted, measured.
- If a colour must move, the candidates rendered and re-reviewed, since the
  high-contrast profiles were accepted with their current values.
- The ratchet in `qt_appearance_profile` updated to whatever is achieved, so
  the new figure is the one a future edit is held to.

## Do not

Do not widen the pair by relaxing the high-contrast minimum. 7.00:1 is the
reason the profile exists.
