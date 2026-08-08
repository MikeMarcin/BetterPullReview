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

## Required direct evidence

- The margin achievable without moving any colour the owner accepted, measured.
- If a colour must move, the candidates rendered and re-reviewed, since the
  high-contrast profiles were accepted with their current values.
- The ratchet in `qt_appearance_profile` updated to whatever is achieved, so
  the new figure is the one a future edit is held to.

## Do not

Do not widen the pair by relaxing the high-contrast minimum. 7.00:1 is the
reason the profile exists.
