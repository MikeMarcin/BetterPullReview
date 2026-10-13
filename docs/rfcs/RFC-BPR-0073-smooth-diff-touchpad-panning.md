---
id: "RFC-BPR-0073"
title: "Smooth diff touchpad panning without accidental vertical steps"
status: "Auto-Verified"
depends_on: ["RFC-BPR-0004", "RFC-BPR-0005"]
metadata: {"rfc_base_sha":"268b87049592ca4643e5303ab77114e116e7b5f3","mandate_revision":"b9a019b63aee9ddcf27e3381564b95f0aa78cef4173b57ce880d216d075eee4d","milestone_plan":{"id":"smooth-diff-touchpad-panning","path":"docs/rfcs/plans/smooth-diff-touchpad-panning/milestone-plan.json"}}
---

# Smooth diff touchpad panning without accidental vertical steps

## Problem and grounded current behavior

On 2026-10-12 the owner reported that two-finger horizontal panning in the
diff repeatedly moves vertically by one line after small finger
perturbations. Sublime Text feels less sensitive to those perturbations;
Sublime Text and VS Code also allow fractional-line vertical view offsets.
These are owner observations. The installed application's physical touchpad
behavior was not independently reproduced during authoring.

Two inspected mechanisms explain the report:

- In [comparison_scroll_controller::handle_wheel_event](../../src/adapters/qt/src/comparison_scroll_controller.cpp),
  a pixel event applies its horizontal delta to the receiving pane and every
  nonzero vertical delta to the comparison position. There is no horizontal
  intent discrimination. The controller already retains high-precision `q`;
  its [precision tests](../../tests/adapters_qt/comparison_scroll_controller_tests.cpp)
  verify accumulation and reversal, but do not establish fractional movement
  of the real text panes.
- In [diff_workbench::apply_frame](../../src/adapters/qt/src/diff_workbench.cpp),
  each projected origin becomes a nearest top line through
  [measured_alignment_axis::top_line_for_origin](../../src/adapters/qt/src/measured_alignment_axis.cpp).
  `source_pane::set_top_line` then sets a native line-count scrollbar. The
  rendered frame reads back that whole-line origin to keep the ribbons
  attached. The unified presentation uses the same `QPlainTextEdit` source
  pane with its native vertical scrollbar.

The same workbench's `handle_line_key` moves the source-line cursor by one
line for Up/Down, with Shift extending selection. `move_pane_line_cursor`
reveals that line through the linked controller only when it is outside the
visible area. Preserve this distinction between line navigation and an actual
viewport scroll.

Together, unfiltered vertical input and whole-line placement can turn a small
unwanted displacement into a conspicuous step. This is an inference from the
code, not a measurement of the owner's gesture stream.

The adopted [scrolling RFC, §14](RFC-BPR-0004-diff-computation-line-mapping-grouping.md#14-shared-scroll-controller)
already requires retained subpixel state, and the
[UX contract, Input details](../ux/diff-link-map-and-alignment-space-scrolling.md#37-input-details)
says smooth trackpad deltas remain smooth. The proposal closes the visible
presentation gap and adds horizontal drift suppression. Its
[proposed amendment](assets/smooth-diff-touchpad-panning/proposed-scrolling-amendment.md) preserves live sources
and historical acceptance during authoring.

## Proposed behavior and boundaries

### Render fractional-line vertical positions

The split and unified source comparisons support continuous vertical view
offsets within a source line. Small accepted movements change the displayed
offset without waiting for a line boundary, and stopping a gesture retains
that offset. Rendering respects device-pixel resolution while retaining
higher-precision accumulated position. Do not add easing, delayed catch-up,
or application-generated inertia; consume the platform's delivered motion.

In split view, the existing comparison position remains the sole vertical
authority. Project it through the measured axis, including pinning at
insertions/deletions and proportional movement through replacements. Any
whole-line positioning needed by the native text control must retain and
apply the remaining within-line displacement. Unified view uses the same
fractional rendering behavior in its own existing source-row coordinates.
Neither presentation gains an independent follower-scroll authority.

Text, syntax and diff washes, line numbers, marks, selection, pointer hit
testing, conversation anchors and link-map endpoints use the same actual
viewport origin, including a partially clipped first row. A painter-only
translation that leaves native selection or hit testing at another position
does not satisfy this requirement. Scroll-frame observers receive the
applied fractional position even when no whole-line scrollbar value changed.

### Suppress minor vertical drift during horizontal panning

The owner explicitly selected **“Suppress minor vertical drift”** on
2026-10-12. For high-resolution touchpad input, clearly horizontal movement
discards small incidental vertical components while preserving the accepted
horizontal movement. Discarded motion is never accumulated into a later
vertical jump. Assess intent before scrollbar clamping: reaching a horizontal
edge must not turn a horizontal gesture's noise into vertical scrolling.

Slow intentional vertical movement remains responsive, including small
vertical-only deltas. Comparable movement on both axes permits diagonal
panning. A deliberate change from horizontal to vertical is recognized
without lifting the fingers; the gesture is not rigidly locked to its first
axis. Maintain enough gesture context to avoid toggling on isolated noisy
events and reset it on a new gesture, device change, comparison replacement
or presentation switch. Where phase information is absent, discrimination
must not depend on an end event that will never arrive.

Preserve input identity and scroll phases through existing forwarding paths,
including `scroll_by_wheel`; platform momentum follows the same decision
without releasing banked noise. Use Qt's already delivered scroll direction
without a second natural-scroll inversion. Pixel input takes precedence over
angle fallback. [Qt 6.11's QWheelEvent contract](https://doc.qt.io/qt-6.11/qwheelevent.html)
defines these input fields and permits zero-delta begin/end events.

The suppression is a default presentation behavior with bounded local
constants, not a new Settings page. Dominance and release thresholds are
implementation tuning choices, constrained by the observations below. VS
Code's [editor option source](https://github.com/microsoft/vscode/blob/main/src/vs/editor/common/config/editorOptions.ts)
documents `scrollPredominantAxis` as enabled by default; this supports the
comparison's plausibility without claiming Sublime's algorithm or requiring
BPR to copy VS Code's policy.

### Preserve the comparison contract

Horizontal movement keeps its existing target and pane-local routing; the
center map gains no horizontal document authority. Traditional angle-only
wheel input retains platform scroll-line policy, Shift-wheel retains its
horizontal behavior, and modified wheel/pinch zoom retains its existing
dispatch. Keyboard scrolling, shared-bar actions and programmatic reveals
continue through their current authority.

The owner's follow-up on 2026-10-12 explicitly keeps Up/Down keyboard
navigation line-by-line. A keyboard reveal scrolls a clipped selected row flush with the nearest
displayed edge; subsequent Up/Down reveals move in discrete one-row steps.
The edge-flush destination may retain a within-line origin, as amended below. A keypress that only moves the
line cursor or extends an in-view selection does not realign the viewport.
This applies in both source presentations, including key repeat and
keyboard-triggered reveals. In split view the reveal edge belongs to the pane
receiving the key; obtain the corresponding shared comparison position and
project the other pane normally. Unequal replacements must not make the
other pane an independently rounded follower. Page scrolling retains its
page-sized movement with a boundary-aligned destination, and Home/End retain
their exact endpoints. Touchpad input resumes smoothly from the applied
position without restoring an earlier fractional residual.

Keep [BPR-1291's range and scroll-past-end behavior](../issues/60-done/BPR-1291-the-shared-diff-scroll-bar-moves-while-the-panes-stand-still-and-there-is-no-scroll-past-end.md):
Home exposes the first lines, End places the last lines at the top, and
interior scrollbar travel moves at least one pane. Keep semantic reading
anchors through zoom, geometry changes and split/unified transitions under
the existing restoration rules. Ordinary frame application must not erase a
fractional offset by snapping back to its containing line.

Gesture state and measured offsets remain transient Qt-local presentation
state. Reuse cached measurements and visible-row rendering; scrolling adds no
semantic diff computation, whole-file remeasurement, provider effects or
persistence writes. The work covers the main diff source presentations and
their existing forwarded input, without redesigning other readers or editors.
The interaction design keeps the existing native surface with direct,
predictable motion.

## Owner correction after candidate v001

On 2026-10-12 the owner rejected the first frozen candidate in session
`e64e44acf84114c0994623f552e9cab1` (decision
`71379576-4036-4951-8090-6d2de4b52ac1`). Keyboard navigation must fully reveal a
partially clipped selected line with the smallest movement compatible with the
existing line-boundary amendment, keeping it at the nearest viewport edge.
Moving it to the center or top as a general keyboard reveal is unacceptable.
The owner also reported excessive movement on the unintended axis and requested
inspection of a prepared Sublime Text long-line document. That scoped inspection
is explicitly authorized; the general prohibition on driving other live desktop
surfaces remains in effect. Stronger axis discrimination must preserve small
vertical-only input, intentional diagonal input and within-gesture reorientation.
This correction refines AC03/AC08 and the required AC07 feel judgment; it does
not waive any acceptance criterion. The new review asks separately about keyboard
reveals and touchpad drift so each correction receives its own answer.

The owner subsequently authorized a scoped native recording and replay diagnostic
in that prepared Sublime window. The completed private wizard recorded 5,596
precise scroll events and 4,113 numeric viewport samples over 37.44 seconds.
One interior horizontal course delivered 353 vertical pixels while viewport Y
remained fixed. In a continuous turn, vertical motion resumed when recent
perpendicular travel became comparable to horizontal travel, although total
perpendicular travel since gesture begin remained much smaller. This supports
recent directional evidence and hysteresis; it does not establish a strict
per-event threshold or Sublime's closed-source algorithm. Reversal alone did
not consistently release preference. Synthetic replay delivered movement but
failed to reproduce the trajectory, so no physical replay equivalence is claimed.
The original trace remains private outside Git; capture has stopped.

## Owner correction after candidate v002

In session `c595e5c955c2cd527f9f4a351d3ab175`, the owner accepted AC07
touchpad behavior (decision `0a0ba98a-3e76-4cb8-b404-fb0196dbd48c`) and
rejected AC08 keyboard reveal (decision `02e54a5d-56d8-439a-b158-ed1bf27b9c9b`):
the highlighted line should have been the bottom line. The accepted adaptive
axis implementation is retained. Candidate v003 diagnosed the mask as shorter than the displayed content and
used `visibleRegion()` before selection projection. Its agent checks passed,
but the owner rejected it. The subsequent diagnostic below falsifies that
region-only interpretation; historical v003 evidence is retained as such.

## Owner amendment after candidate v003

The owner rejected candidate `BPR-1413-AC08-v003` in session
`12930e4afcde5660e890f36745df8f70`, decision
`8751a593-c7dc-45cd-a5a1-92a32375a048`, with before/after screenshots:
“The bottom of the selected row should be flush with the bottom of the view area.”
The chat explicitly handed this correction back for implementation. The durable
verdict was consumed at cursor 4 even though the session hand-back field was null.

A direct diagnostic also found `visibleRegion()` includes overscan behind the
horizontal scrollbar. Reveal must intersect the native viewport mask and use
the actual source edge. Tests independently check that bottom against the
native horizontal scrollbar.

This replaces the prior requirement to round every keyboard reveal to a top
source-line boundary. When the viewport height is not an integer number of rows,
that rounding leaves unwanted space below the selected row. A clipped target
must instead meet the actual displayed bottom (or top when moving upward), within
one device pixel. Fully visible targets stay put. Consecutive arrows still
advance exactly one source line and, once revealing at an edge, scroll one row
per step; Shift and repeat retain their behavior. The receiving pane obtains
that exact origin through the shared comparison authority, and the other pane
projects normally through unequal replacements. Unified view uses its existing
native row authority. Page destinations remain line-aligned and Home/End retain
exact endpoints. Subsequent touchpad input starts at the applied edge-flush
origin. The accepted touchpad discriminator and its acceptance are retained.

The adopted plan's discrete-navigation and receiving-presentation alignment exit
is interpreted as edge alignment under this explicit amendment. Its scope,
grouping and all other exits are unchanged; imported draft bytes and historical
evidence remain intact.

## Owner handback after candidate v004

The owner rejected `BPR-1413-AC08-v004` in session
`a300291de66a8fb8963c182932d117d4` (handback cursor 5):
“The alignment/scroll looks fixed now. But why does scrolling by tapping the down key bring up the horizontal scroll bar?”
This is an observation that alignment is fixed and a remaining scrollbar concern,
not an acceptance verdict. AC07 remains accepted on v002. Preserve the exact-flush
geometry. At that time the remaining attended question concerned preventing
vertical navigation from waking the native horizontal overlay.

## Owner withdrawal after candidate v005

Candidate `BPR-1413-AC08-v005` was rejected in session
`ab24efc9ef4a0f4714e0edb79a4e6977`, decision
`f1590089-458c-4363-ae7c-04cb1933f91d`, consumed cursor 6:
“Instead of a flash it's just staying on permanently now.” That verdict remains
Reject for its exact candidate.

The owner then explicitly withdrew the scrollbar objection: “Okay I take it
back, showing the scroll bar seems to be a normal app behavior, just return it
to what it was before i complained”. This amendment restores the ordinary
native scrollbar behavior from v004, removing only v005's flash-suppression
subclass, axis notification and dedicated suppression test. It retains v004's
edge-flush alignment and the accepted v002 touchpad discriminator.

The separate scrollbar judgment is no longer required. The original AC08
geometry and navigation checks still apply; the owner's v004 observation that
alignment/scroll is fixed and this explicit withdrawal resolve the remaining
owner concern. Neither rejected candidate is relabeled Accept. AC07 retains
its actual v002 Accept. No new native fade guarantee is introduced.

## Acceptance criteria

- **AC01:** In rendered split and unified comparisons, successive vertical
  pixel deltas smaller than a line visibly move text within the line. A
  gesture ending between line boundaries holds that position; reversing
  accepted motion retraces it without cumulative drift.
- **AC02:** From a mid-file position near a former line-snap boundary,
  repeated horizontal touchpad events with approximately 20 pixels of
  horizontal travel and at most 1 pixel of incidental vertical travel pan
  horizontally without vertical displacement. Reversing horizontal direction,
  reaching its edge, and ending the gesture release no delayed vertical step.
- **AC03:** Small vertical-only input remains effective, a sustained
  comparable-axis diagonal gesture moves both available axes, and deliberate
  reorientation works within a gesture. A new gesture, forwarded event or
  comparison/presentation change does not inherit stale suppression.
- **AC04:** At fractional origins, the rendered ribbons still meet the
  exact source boundaries they name, and gutters, marks and conversation
  anchors follow the text. Clicking or selecting a partially clipped row
  resolves its actual source identity. Verify at 100%, 125% and 200% scale
  and after a code-zoom change using the applied geometry.
- **AC05:** Insertion pinning, unequal-replacement motion, Home/End,
  scroll-past-end, pane handoff, wheel fallback, Shift-wheel, modified zoom
  and semantic reveals retain their defined behavior. Movement does not
  create dead scrollbar travel or transfer vertical authority to a pane.
- **AC06:** A large-file composed scrolling course retains existing input
  budgets with no semantic recomputation, unchanged-axis rebuild or
  whole-file text scan per event. Evidence observes the real pane origins
  and rendered geometry as well as controller position.
- **AC07:** After deterministic checks, the owner accepts horizontal
  touchpad panning on a self-contained frozen build in a fresh short
  attended-review session: “Does horizontal panning stay at the intended
  vertical reading position despite minor finger wobble?” Use Accept/Reject
  with a change note; no desktop driving or capture by the implementer.
- **AC08:** From a fractional origin, Up and Down still navigate one source
  line per step and retain Shift selection behavior. Keyboard reveals place a clipped selected row flush with the actual nearest
  displayed edge within one device pixel, retaining discrete one-row subsequent
  steps, repeat and Shift selection. An in-view cursor/selection move leaves the
  fractional origin unchanged. Page destinations remain line-aligned and Home/End
  retain exact endpoints. Subsequent touchpad movement starts at the applied origin. Verify split
  and unified views, including an unequal replacement in split view.

No material product choice remains open. Exact filtering constants and the
native-control technique remain implementation decisions. The owner's v002
touchpad acceptance is retained. The explicit withdrawal above removes the
separate scrollbar judgment; all original deterministic criteria still apply.

## Adoption and execution

Adopted under the recorded [live import](assets/smooth-diff-touchpad-panning.LIVE_IMPORT.md). The original owner amendments and all eight criteria remain authoritative. The revised implementation and deterministic criteria are verified in [the evidence ledger](../verification/RFC-BPR-0073.md). Touchpad behavior has actual owner acceptance on v002. The owner observed v004 alignment fixed and subsequently withdrew the sole remaining scrollbar objection, requesting restoration of the ordinary native behavior. The restoration and closeout retain all earlier Reject verdicts; no human acceptance is inferred from automated checks or the scoped diagnostic.
