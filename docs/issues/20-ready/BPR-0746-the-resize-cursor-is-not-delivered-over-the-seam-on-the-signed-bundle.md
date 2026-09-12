---
schema: "implement-rfc.card.v1"
id: "BPR-0746"
title: "The resize cursor is not delivered over the seam on the signed bundle"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"a374890c-b692-4ada-afb9-6ccd278d7645","supersedes_diagnosis_on":"BPR-0739"}
---

# The resize cursor is not delivered over the seam on the signed bundle

The owner, 2026-09-11, rejecting `BPR-0739-splitter-seam` in the v008
attended session (decision `a374890c`, 04:34:54Z), verbatim:

> still fails, same issue, at least it draws the seam now, cursor didnt show
> up in screenshot, but i promise its right over that blue vertical line and
> looks like a normal cursor not a resize cursor.

One screenshot,
[the seam lit between the code and the conversations drawer](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-splitter-seam-highlight-2026-09-11.png):
a saturated one-pixel rule runs the height of the panes between the
comparison's scroll bar and the drawer. macOS omits the pointer from a
screen capture, so the cursor itself is the owner's report, not the image.

## Why this is a new cause, not the old one

[[BPR-0739]]'s d17 diagnosis concluded cause 3: the handle was seven pixels
of window colour with nothing to aim at, so the cursor changed only while
the pointer crossed it. The fix drew the drawer's edge rule in
`QPalette::Mid`, and in `QPalette::Highlight` while the pointer is over it.

That highlight is the refutation. The rule is painted from `underMouse()` —
`Qt::WA_UnderMouse`, which Qt's enter/leave dispatch sets
(`src/adapters/qt/src/workbench_window.cpp:6303-6312`, the pen chosen at
`:6309`) — so at the moment of the capture the handle held Qt's pointer
state and the effective cursor was still the arrow. Hit-zone width is
therefore not the failing variable; the failure is in cursor *delivery*
after Qt's enter, which is outside candidates 1 to 4 on BPR-0739, all of
which are relayout hypotheses.

Nothing in the repository sets a cursor on this path.
`workbench_splitter_handle`
(`src/adapters/qt/include/bpr/qt/workbench_window.hpp:199-208`,
`workbench_window.cpp:6285-6290`) overrides only `paintEvent`: no
`enterEvent`, `leaveEvent`, `event()`, `setCursor`, `unsetCursor`,
`setMouseTracking` or `setAttribute`. The split cursor exists only because
`QSplitterHandle` sets it. There is no `setOverrideCursor` anywhere in
`src/adapters/qt` or `src/app`, no native child view, and the rail's
pointing hand (`workbench_drawer.cpp:280`) is hidden while the drawer is
expanded (`:551`). The shared scroll bar the pointer crosses first
(`diff_workbench.cpp:827`) has no cursor code.

## The hypotheses, in order

1. **Qt sets the NSCursor once on enter and AppKit reverts it on the next
   real pointer motion, and Qt never re-asserts** because from its side
   nothing changed: both `QWindowPrivate::setCursor` and
   `QCocoaWindow::setWindowCursor` early-return while Qt still believes the
   shape is `SplitHCursor`. This fits every observation at once — the brief
   flicker the owner reported in v007 (the one-shot set), the highlight
   (Qt's pointer state is intact) and the arrow that then persists until the
   pointer leaves and re-enters. Its variant: the trigger is this window's
   hidden title bar (`workbench_window.cpp:358-360`,
   `Qt::ExpandedClientAreaHint`, `Qt::NoTitleBarBackgroundHint`,
   `WA_ContentsMarginsRespectsSafeArea` false), the one thing that makes
   this NSWindow unusual.
2. **The one-shot set never reaches AppKit**, because `setWindowCursor`'s
   gate fails (`[m_view hitTest:] != m_view`, or the window is not key at
   that instant), leaving the view's cursor recorded but never applied. This
   predicts no flicker at all.
3. **A stale window-level cursor record**: the pointer earlier left the
   handle by a route that produced no Qt leave — for example into the
   transparent title-bar strip, where AppKit and not Qt owns the pointer
   under `ExpandedClientAreaHint` — so the next entry hits the early return
   and no set happens.

Excluded by the evidence: a sibling or child widget owning the cursor (the
highlight proves the handle received Enter, and there are no native child
views or transparent overlays); an application override cursor; the base
class not setting the cursor at all (the offscreen test reads
`Qt::SplitHCursor` off the handle, `tests/adapters_qt/workbench_window_tests.cpp:1976-1978`).

The Qt-internal chain above is read from the shipped `libqcocoa.dylib`'s
symbols and strings (`invalidateCursorRectsForView:`, `hitTest:`,
`keyWindow`, "Setting … cursor to", "Synthesizing cursor update") plus
knowledge of qtbase; Qt sources are not on disk here. The discriminating
check below is written so its outcome does not depend on that reading being
exact.

## The check that comes first

Before any product change, run the **real windowed application** — the
tests-disabled bundle's executable or the dev app, from a terminal — with
`QT_LOGGING_RULES='qt.qpa.input.mouse*=true'`. Move the pointer from the
comparison onto the seam, hold it with slight motion for about three
seconds, then move off, and read the sequence:

- On entry `Setting … cursor to <… resizeLeftRight>` followed by
  `Synthesizing cursor update`, then nothing further while the on-screen
  cursor is an arrow: hypothesis 1. Follow it by re-running the plain
  `QSplitter` probe with continuous motion instead of one warp; if the plain
  window also loses the cursor the behaviour is platform-wide, and if it
  holds, add the three chrome flags from `workbench_window.cpp:358-360` to
  the probe and re-run.
- `Setting … cursor to <resizeLeftRight>` with no `Synthesizing cursor
  update` after it: hypothesis 2.
- No `Setting …` line at all on that entry, with an earlier
  `resizeLeftRight` set never followed by a set back to the arrow:
  hypothesis 3.
- A `Setting … cursor to <arrow>` logged while the pointer is on the lit
  seam: all three are wrong and the Qt widget layer changed the owner; the
  lines around it are the next lead.

## Scope

- The horizontal-resize cursor is shown and held while the pointer rests
  anywhere on a central splitter handle, on the signed bundle on macOS, in
  every modality, with and without a drag.
- Run the discriminating check before changing product source, and record
  which hypothesis it selected on this card. If the cause is platform-wide
  rather than this window's, say so and bring the scope back before writing
  a workaround: the handle alone may not be where the fix belongs.
- The seam itself is accepted work and stays: the owner's "at least it draws
  the seam now" is the one thing that improved. Whether the `Highlight`
  hover state survives is still the open question BPR-0739 asked and the
  owner has not answered.
- Owned paths: `src/adapters/qt/src/workbench_window.cpp` (the splitter and
  handle at `:6270-6312`),
  `src/adapters/qt/include/bpr/qt/workbench_window.hpp:199-208`,
  `tests/adapters_qt/workbench_window_tests.cpp`. The window chrome at
  `workbench_window.cpp:358-360` is owner-accepted BPR-0646 surface; touching
  it needs the integrator's decision first.

## Acceptance

- The discriminating check is run on the real windowed application and its
  output recorded here with the hypothesis it selected; a repeat of the
  offscreen pass is not evidence, for the reason below.
- A check that can actually fail when this defect is present. The existing
  offscreen test reads only Qt's own bookkeeping — `underMouse()`,
  `rect().contains(...)`, `cursor().shape()`
  (`tests/adapters_qt/workbench_window_tests.cpp:1969-1997`,
  `:2009-2049`) — under a platform with no cursor at all, so it verifies the
  layer that is not in dispute. Name what the new check observes and why it
  is red before the fix.
- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`
  and `architecture_boundaries` pass.
- The owner resizes the conversations drawer on the signed bundle with the
  cursor visible throughout, in an attended session. That criterion lives on
  [[BPR-0739]] and is asked again from a bundle carrying this card.

## Correction to BPR-0739's recorded evidence

BPR-0739's AC1 evidence says the "cocoa probe held
NSCursor.resizeLeftRightCursor for 1.1 s at rest". `build/logs/bpr-0739-cocoa-diag-1.log`
does not support that: it records `pointer at local x=-1164` — the real
pointer 1164 pixels left of the handle, over the files region — while
reporting `underMouse=1` and `nscursor=resizeLeftRight`, and the run printed
`FAILED`. The Enter came from an event sent directly to the window and the
cursor value from Qt's own synchronous set with the pointer elsewhere. The
log proves that Qt calls `[NSCursor set]`, which nothing disputed, and says
nothing about what AppKit does on the next real motion with the pointer on
the handle. The correction is recorded on BPR-0739; do not reuse that
sentence as passing evidence.
