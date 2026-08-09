---
id: BPR-0219
title: The appearance probe scrolls a widget that is not in a scroll area
status: Done
rfc: RFC-BPR-0006
depends_on: []
---

# The appearance probe scrolls a widget that is not in a scroll area

Recorded inside [[BPR-0114]]'s notes and split out here so it is not lost when
that card closes. BPR-0114's own words: *"Harmless here, wrong everywhere."*

## Measured

Two sites in `src/app/main.cpp` try to scroll the comparison into view before
capturing appearance evidence:

```cpp
src/app/main.cpp:837    if (auto* const scroll = qobject_cast<QScrollArea*>(aWindow.centralWidget())) {
src/app/main.cpp:2724   if (auto* const scroll = qobject_cast<QScrollArea*>(window.centralWidget())) {
```

Both follow a `resize(1'280, 1'000)` and both call
`scroll->ensureWidgetVisible(workbench, 24, 24)`.

The cast returns null. `workbench_window` sets its central widget at
`src/adapters/qt/src/workbench_window.cpp:583`:

```cpp
auto* const host = new QWidget{ this };     // :577
stack->addWidget(HeaderScroll);             // :581
stack->addWidget(CentralSplitter, 1);       // :582
setCentralWidget(host);                     // :583
```

The central widget is a plain `QWidget` holding a `QVBoxLayout`. So the guarded
body never executes, and both calls silently do nothing.

## The part that makes this more than a dead cast

Repairing the cast is the obvious fix and it is probably the wrong one.

There is exactly one `QScrollArea` in the shell — `HeaderScroll`
(`workbench_window.cpp:569`) — and it does not contain the workbench. It holds
the review *header*, is capped at 220 pixels, and exists to stop a long
description taking height from the comparison. The comparison itself lives in
`CentralSplitter`, which is not inside any scroll area at all. It is also
private: `HeaderScroll` is a member at `workbench_window.hpp:359` with no
accessor.

So there is no scroller that could bring the comparison into view, because
since [[BPR-0191]] the comparison is not scrolled into view — it is laid out to
fill the space the header does not take. The call was written against a window
structure that no longer exists.

## Required direct evidence

- Measure whether the comparison can actually be clipped in the shell at the
  sizes the probes use, after `resize(1'280, 1'000)`. That measurement decides
  the fix, and it must be a measurement rather than a reading.
- If it cannot be clipped: delete both calls. Restoring a scroll-into-view that
  reveals something already visible is motion, not correctness.
- If it can be clipped: expose the surface that actually scrolls and use it,
  and say which widget was off screen and by how much.
- Either way, a future widget-tree change must not silently reintroduce this.
  Add an assertion or marker that fails loudly when the probe's assumption
  about the shell's structure stops holding. A `qobject_cast` in an `if` is
  precisely the shape that cannot fail loudly.

## What must not be done instead

Changing the cast to `findChild<QScrollArea*>()` so it finds `HeaderScroll` and
calling `ensureWidgetVisible` on that. It would compile, run, stop being dead
code, and scroll the wrong widget — the header, not the comparison. That is
worse than the current state, because the current state at least does nothing
visibly.

Deleting the calls without the measurement. The conclusion may well be to
delete them, but "the cast was dead so the code was pointless" does not follow;
the cast being dead is why nobody knows whether the intent was needed.

## Measured, then deleted, 2026-08-09

The card said the fix had to be decided by a measurement rather than a reading,
and the measurement says delete. Instrumented at the call site and run
offscreen at the probe's own 1280x1000:

```
BPR0219_CENTRAL_CLASS:QWidget
BPR0219_CENTRAL_RECT:1280x918
BPR0219_WORKBENCH_RECT:260,475 760x443
BPR0219_CONTAINED:1
BPR0219_VISIBLE_COVERS:1
```

The central widget is a plain `QWidget`, confirming the cast fails. And the
comparison is **already wholly visible**: its rect is contained in the central
widget's, and its visible region covers its whole rect. So `ensureWidgetVisible`
had nothing to do even in the world where the cast succeeded. Both calls are
gone.

## What replaced them

Deleting alone would have left the next widget-tree change just as silent, so
the property the call was reaching for is now asserted:
`comparison_is_wholly_visible()`, used at both former cast sites.

- In `prepare_native_appearance_probe` a failure returns false and prints
  `NATIVE_APPEARANCE_COMPARISON_VISIBLE_FAILED`, because an appearance capture
  of a clipped comparison is evidence of the wrong thing.
- In the diff-evidence capture it prints
  `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` and fails the run.

### Shown non-vacuous

Moving the comparison out of view (`workbench->move(x, 5000)`) fails the run
with the marker; removing the move passes with `DIFF_WORKBENCH_OK` and no
marker.

One correction during the work. The first version used `break`, which only
ended the tick and sent it round again — the marker printed several hundred
times across retries. A structural fault does not resolve by retrying, so it
calls `fail_automated_run` instead. Measured after the change: the marker
appears exactly **once**.

That is the difference the card was after. The old code could not fail at all;
the first replacement failed too loudly to read; this one fails once and says
what broke.
