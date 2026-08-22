---
id: BPR-0220
title: An appearance change rebuilds comparison geometry twice
status: Parked
rfc: RFC-BPR-0006
depends_on: [BPR-0249]
parked_on: awaiting-owner-verdict
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

## Measured offscreen, 2026-08-09 — partial, and it narrows the question

Instrumented `diff_workbench::schedule_geometry_rebuild` to record the cause
bitmask of every scheduling call and every rebuild that actually happened, then
ran the diff-workbench probe offscreen with and without an appearance change.
Instrumentation reverted afterwards; the numbers below are what it recorded.

| Run | Rebuilds |
|---|---|
| `--diff-workbench-probe` | 4 |
| the same plus `--diff-evidence-appearance dark` | 5 |

**Offscreen, an appearance change costs exactly one rebuild, not two.**

The scheduling causes across a whole run:

```
 35  cause=128   viewport_or_divider
  1  cause=64    screen
  1  cause=1     accepted_diff
```

Thirty-seven scheduling calls coalesced into four rebuilds, which is the
mechanism working as designed: `schedule_geometry_rebuild` returns early while
`GeometryScheduled` is set, so any number of calls inside one event-loop turn
produce one rebuild. **Two rebuilds therefore means two separate turns, not two
calls** — that is the fact worth carrying into the display run.

Two absences are informative. `font` (4) and `tab_width` (16) never fire for a
colour-only profile change, so the second rebuild is not typography. And
`screen` (64) fires exactly once per run in both, so it is startup, not the
appearance change.

## What is still unmeasured, and why

`DIFF_GEOMETRY_REBUILD_DELTA:2` is sampled by the **native appearance probe**,
which refuses to run offscreen — deliberately, and correctly. So the offscreen
delta of one and the probe's delta of two are measurements of different runs,
and the difference between them is now the actual question:

- either the second rebuild is display-dependent — a real screen, device pixel
  ratio, or window-activation effect that offscreen cannot produce;
- or the probe's scheme change drives a path the profile capture does not.

Both are testable in one run, and it is a run somebody is about to do anyway.

**Fold this into [[BPR-0114]]'s foreground run.** The owner's probe is the only
configuration that reaches the code path, and it already reports the delta. Re-
instrumenting `schedule_geometry_rebuild` for that run — recording the cause
mask per rebuild — answers this card from the same sitting. This card remains
open rather than moving to `60-done/`: the product question is not answered, only
narrowed from "why twice" to "which of two named candidates".

The `== 1U` expectation at `main.cpp:728-729` and `:2339` is unchanged, and must
stay unchanged until that measurement exists.

## Attribution implemented, foreground measurement pending

The native probe now retains a bounded record for every successful comparison
geometry rebuild. Each record carries both the coalesced measured-axis change
mask and the exact Qt-facing trigger mask, so two
`viewport_or_divider` rebuilds no longer look identical when one came from a
style event and the other from a resulting pane resize.

`NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_CAUSES` reports records as
`change-mask/trigger-mask` in rebuild order. Trigger bits are:

| bit | trigger |
|---:|---|
| 1 | accepted diff |
| 2 | font event |
| 4 | style event |
| 8 | show event |
| 16 | device-pixel-ratio event |
| 32 | screen-change event |
| 64 | tab-width adoption |
| 128 | pane viewport resize |
| 256 | workbench resize |

`NATIVE_APPEARANCE_DIFF_GEOMETRY_ATTRIBUTION_OK` requires an untruncated,
one-record-per-counter-increment slice since the native probe's settled Light
baseline. The history is fixed at 32 records and fails the attribution marker
closed on overflow. The focused Qt contract proves palette changes add no
record, divider resizes include the pane-resize trigger, and style invalidation
includes the style trigger. The offscreen native contract proves the new
markers remain complete on the configuration-refusal path.

The foreground Light → Dark run is still required before choosing a product
fix or changing either exact `== 1U` expectation. This instrumentation answers
the card's measurement question; it does not predetermine the result.

## Awaiting the owner foreground run

The implementation and focused independent review are complete. The remaining
evidence cannot be collected offscreen or inferred from process exit status:
the application must be frontmost during the native Light → Dark transition,
and the complete output must contain
`NATIVE_APPEARANCE_DIFF_GEOMETRY_ATTRIBUTION_OK` plus the ordered cause line.

Run only the digest-bound procedure in
[`appearance-scheme-foreground-v007`](../../verification/reviews/appearance-scheme-foreground-v007/operator-packet.md).
This card is parked in `40-parked/` until that log is returned. No product fix
and no change to either exact `== 1U` expectation is authorized before the
causes are measured.

### The version this card used to name — corrected 2026-08-19

Until [[BPR-0398]] this paragraph said to run v004, and that instruction had
been wrong for three regenerations. v004 is worse than merely stale: the owner
attempted it twice and could not complete it, because the dark-side measurement
fired before a person returning from System Settings could restore focus.
[[BPR-0391]] found and repaired that, and v005 through v007 followed.

The cause was structural rather than careless. This packet family serves three
cards — [[BPR-0114]], this one, and [[BPR-0391]] — and each regeneration updated
only the card that ordered it. BPR-0398 carries the verifier check that now
fails when a card names a superseded packet.

## The final-head diagnostic packet is v008 — 2026-08-21

The v007 attempt was real but could not separate focus, state, capture, and
persistence causes. [[BPR-0410]] has made those facts independently observable.
Run only
[`appearance-scheme-foreground-v008`](../../verification/reviews/appearance-scheme-foreground-v008/operator-packet.md),
which binds executable
`sha256:10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28`
and requires the complete raw log. This card remains parked until that log
records the geometry delta and ordered causes; no `== 1U` expectation is changed
in advance.

## v008 measured zero rebuilds — 2026-08-21

The owner returned the exact v008 statement and complete unedited native Cocoa
log. The immutable
[`operator response`](../../verification/reviews/appearance-scheme-foreground-v008/operator-response.md),
[`raw log`](../../verification/reviews/appearance-scheme-foreground-v008/foreground-run-attempt-1.log),
and [`validation record`](../../verification/reviews/appearance-scheme-foreground-v008/validation-record.json)
record `NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:0`, no rebuild causes,
and complete attribution after a strict owner return whose three focus operands
are all true.

That measurement supports one narrow repair. The appearance change is
colour-only and the focused Qt contract already proves that palette changes do
not rebuild comparison geometry, so both native-state expectations requiring
exactly one rebuild are wrong together; the supported value is zero. The
failure then prevents changed capture and persistence, which is why v008
reports `not_attempted_state_precondition` and `not_attempted`. The palette,
font, geometry, selection, and changed-render verdicts share that failed state
predicate, so their `FAILED` suffixes are not independent evidence of six more
product defects.

The returned observation authorizes no source edit or executable replacement.
Changing the two expectations in `src/app/main.cpp` would relink the exact
binary still bound by the pending final-head visual and notification packets.
This card therefore remains Parked until the owner authorizes that replacement
or those exact-byte gates are retired or answered.
