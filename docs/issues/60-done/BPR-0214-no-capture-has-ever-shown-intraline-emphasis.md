---
schema: "implement-rfc.card.v1"
id: "BPR-0214"
title: "Intraline emphasis has never appeared in a reviewed capture"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0233"]
---

# Intraline emphasis has never appeared in a reviewed capture

## Final-state correction from BPR-0225

BPR-0187 is done: the application now serves the recorded UIforETW pull
request and the generator/contract drift checks are direct. The remaining gap
is one layer later. `application_github_public_path_uiforetw_74` opens that
recording and saves a window grab, but it does not request a selected diff;
`RENDER_OK` proves only that the PNG save succeeded. BPR-0233 now owns the
selected recorded diff, semantic intraline ranges, exact role pixels, and the
four-profile attended capture. The historical diagnosis below is retained to
show why BPR-0187 existed, not as the current blocker.

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

## Fixture designated and recorded, 2026-08-08

The product owner designated `google/UIforETW#74`. It is recorded at
`tests/fixtures/github/pull-requests/uiforetw-74/`: 5 files, +46 −6, 1 commit,
carrying three modification pairs where the removed and added lines are 97–99%
the same text and differ by one or two characters.

```
-#define _APS_NEXT_RESOURCE_VALUE        136
+#define _APS_NEXT_RESOURCE_VALUE        137
```

That shape is held rather than described: `recorded_pull_request_fixture_tests`
now counts near-identical removed/added pairs and fails if this recording stops
carrying at least three, and separately asserts that `directxmath-336` is a pure
addition so it can never be used to claim intraline emphasis. Shown non-vacuous
by stripping the additions from the recording, which fails the count at zero.

## Measured, and it is the blocker: nothing serves a recording

`grep -rn "recording.json" src/` finds nothing. The recordings are read by one
Python validator and by no part of the application. The `--github-fixture`
vocabulary is `github_com` and `ghes_3_18`, both hand-written payload builders,
so there is currently **no way to point the product at a recorded pull request
at all**.

That is BPR-0187's actual work, and it was mis-stated as done when the recording
landed. Recording a fixture and serving one are different tasks; only the first
happened.

So the remaining order is:

1. BPR-0187 — teach the fixture transport to serve a recording. Measured
   afterwards and larger than it reads: the comparison surface diffs real git
   blobs from embedded packfiles, not API patch text, so the recording has to
   grow file contents at base and head before anything can render it. The card
   carries the detail.
2. This card — take a capture from `uiforetw-74` and put the intraline rows in
   front of a reviewer.

Nothing about the emphasis itself is known to be broken. What is known is that
no capture has ever been able to show it, and until step 1 lands none can.

## Closed by the accepted v006 batch — 2026-08-18

Both remaining steps happened. Done BPR-0187 serves the `uiforetw-74`
recording with real blob contents, done BPR-0233 drives it through the real
selected-file diff path, and the v006 attended batch at
`docs/verification/reviews/uiforetw-intraline-acceptance-v006/` put the
intraline rows in front of the reviewer: its
`criterion:/betterpullreview/bpr-0233/recorded-intraline-emphasis` subject
presents both coordinate replacements' narrow intraline-removed and
intraline-added marks in all four appearance profiles, backed by exact role
pixels in the capture facts, and the product owner accepted it with the
validated verdict recorded beside the captures. The title's fact is finally
false: a reviewed capture has shown intraline emphasis, and a reviewer
accepted what it showed. The near-identical-pair count guard in
`recorded_pull_request_fixture_tests` remains as the regression hold.
