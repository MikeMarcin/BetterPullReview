---
id: BPR-0220
title: An appearance change rebuilds comparison geometry twice
status: Ready
rfc: RFC-BPR-0006
depends_on: []
---

# An appearance change rebuilds comparison geometry twice

Recorded inside [[BPR-0114]]'s notes and split out here because it is the one
item in that card's failing list which is **not** a focus artefact. BPR-0114's
own words: *"a product signal, not a rule question... nobody has looked."*

## Measured

The native appearance probe samples the workbench's geometry-rebuild counter
before and after the scheme change, and requires the difference to be one:

```
src/app/main.cpp:841   InitialDiffGeometryRebuilds = workbench->statistics().GeometryRebuilds;
src/app/main.cpp:3168  DiffGeometryRebuildDelta    = final - initial;
src/app/main.cpp:2327  "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:" << delta
src/app/main.cpp:2339  delta == 1U ? ..._OK : ..._FAILED
```

The observed value is `DIFF_GEOMETRY_REBUILD_DELTA:2`. The same `+ 1U`
expectation appears a second time inside the light-state conjunction at
`src/app/main.cpp:728-729`.

## The question, which is not a probe-rule question

Every other term in BPR-0114's failing list roots in `StatePassed`, which is
gated by the window being frontmost — a launch artefact that a foreground run
resolves. This one does not. The counter moved twice, and that is either:

- **two legitimately distinct rebuild causes** — for example the palette change
  and a font-metric or viewport change arriving as separate invalidations, each
  correctly rebuilding once; or
- **one appearance change applied twice** — a genuine defect, in which case
  every appearance change costs a redundant geometry rebuild of the comparison,
  and the probe has been reporting it faithfully all along.

Nothing currently distinguishes those two, because the counter is a single
number with no attribution.

## Required direct evidence

- Instrument the rebuild path so each increment records **why** it happened,
  then run the appearance change and read the two causes. The measurement is
  the deliverable; it decides everything else.
- Fix only what the measurement supports. If the two causes are distinct and
  legitimate, the product is correct and the `== 1U` expectation is wrong. If
  it is one change applied twice, the product is wrong and the expectation is
  right.
- Whichever it is, both sites move together: `main.cpp:728-729` and
  `main.cpp:2339` encode the same expectation and must not be allowed to
  disagree.

## What must not be done instead

Relaxing `== 1U` to `>= 1U`, or to `<= 2U`, so the probe goes green. That
converts a specific claim — *an appearance change rebuilds the comparison's
geometry exactly once* — into one that cannot fail, and it does so before
anyone has established which of the two readings is true. If the measurement
shows the expectation is wrong, change it to the number the product actually
produces and record why that number is correct; do not widen it to a range.

Deferring this into [[BPR-0114]]'s foreground run and treating a still-red
`DIFF_GEOMETRY_REBUILD` term as part of the focus cascade. It is not part of
that cascade. It will still be red after the window is frontmost, and it should
be — that is the signal working.
