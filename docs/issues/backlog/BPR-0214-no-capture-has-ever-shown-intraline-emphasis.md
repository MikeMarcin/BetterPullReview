---
id: BPR-0214
title: Intraline emphasis has never appeared in a reviewed capture
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0187]
---

# Intraline emphasis has never appeared in a reviewed capture

Found while trying to render candidates for [[BPR-0210]]. Scanning every frame
of the public-path fixture's interaction course, in the light profile and both
high-contrast profiles, for the exact `diff_intraline_added_background` and
`diff_intraline_removed_background` values: **not one pixel, in any frame.**

The fixture's comparison produces no intraline edits, so the painter correctly
draws none. Nothing is broken. What is wrong is what has been concluded on that
basis.

## What was accepted without it

Four attended batches were run between 2026-08-06 and 2026-08-08, and all four
shipped palettes were accepted. **None of those captures contained intraline
emphasis.** Meanwhile those same values were changed twice on measurement alone:

- `#FFE1DE` to `#FFE2DF` in light, and `#95FF95` to `#99FF99` in high-contrast
  light, to clear a contrast failure the old model was hiding (BPR-0204);
- the dark pair rebuilt entirely when the bands were softened to the owner's
  reference (BPR-0199).

So four values were fitted, twice, and signed off in batches where they could not
be seen. That is the same shape as the white scrollbar in [[BPR-0209]]: measured,
compliant, and unobserved — except this time the unobserved thing was *inside*
the accepted evidence rather than outside it.

## It also blocks BPR-0210

Rendering the intraline candidates for that card produced **nineteen
pixel-identical frames** across the current values and the strongest candidate.
A review of those images would compare a thing to itself and read as an
acceptance. Recorded on that card as the reason its unattended half cannot be
completed.

## What this needs

A comparison whose two sides share enough of a line to produce intraline edits.
`representative_text()` in `diff_workbench_tests` already does — the Qt suite
asserts an emphasis range against it — so the shape is known and only the
public-path fixture lacks it.

[[BPR-0187]] was the natural home and **is not**. Checked rather than assumed,
which is what that paragraph asked for: `microsoft/DirectXMath#336` removes zero
lines across both its files. A pull request that only adds has no line present on
both sides, so it cannot contain an intraline edit. Serving that recording would
leave this exactly as unobserved as it is now.

What is needed is a recording with **removals and edits within lines** — a
replacement, not an addition. That is a second recording, and BPR-0187 now
records the same finding from its own side.

## Required direct evidence

- A capture in which intraline emphasis is visible, with its pixels matching the
  profile's intraline roles rather than the row band's.
- That capture reviewed, in every profile whose intraline values have been
  fitted — which is all four.
- An assertion that the fixture's comparison contains at least one intraline
  edit, so a future fixture change cannot silently remove the only evidence
  again.

## Do not

Do not synthesise a fixture whose only purpose is to show emphasis. The reason
this went unnoticed is that the existing fixture's content was written to satisfy
the code reading it; another one written to satisfy a painter would repeat the
mistake one layer up.
