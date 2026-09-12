---
schema: "implement-rfc.card.v1"
id: "BPR-0746"
title: "The resize cursor is not delivered over the seam on the signed bundle"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"a374890c-b692-4ada-afb9-6ccd278d7645","supersedes_diagnosis_on":"BPR-0739"}
parked_on: "human-review"
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
  the seam now" is the one thing that improved. The `Highlight` hover state
  did not survive — the owner dropped it in v009 (below) — so the rule is one
  constant `Mid` weight and the seam no longer announces the hot zone at all.
  That makes the hot zone's width and the rule's boundary column part of this
  card's problem rather than a separate cosmetic question.
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

## The check cannot be run unattended, 2026-09-12 (mandate r11)

This card says the discriminating check runs before any product change. It has
not been run, and no product source was changed. The reason is not that it was
skipped; it is that the check is not an agent's to run on this machine.

Every hypothesis on this card is about what AppKit does **on the next real
pointer motion** after Qt's enter. Hypothesis 1 says it reverts the cursor
then; hypothesis 2 says nothing reached AppKit at all; hypothesis 3 says an
earlier exit left a stale record. None of them can be told apart without a
real pointer crossing onto the seam and moving slightly there. Synthesising
that motion means warping the pointer on the owner's desktop, which this
dispatch is forbidden to do — and it is also what made
`build/logs/bpr-0739-cocoa-diag-1.log` worthless: that run sent an event
directly to the window with the real pointer 1164 pixels away, which is the
correction recorded on [[BPR-0739]].

Positioning the window under a stationary pointer instead does not help
either. It would produce an enter without motion, and motion is the variable
under test.

So the check is a human step, and it is prepared rather than deferred. It
rides [[BPR-0739]]'s v009 candidate, because that candidate already asks the
owner to put the pointer on the seam: one trip to the seam yields both his
report and a log this dispatch can read. The command is one line, run from a
terminal instead of Finder:

    QT_LOGGING_RULES='qt.qpa.input.mouse*=true' \
      build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview \
      2>&1 | tee build/logs/bpr-0746-cursor-probe.log

Then Read, pointer from the comparison onto the seam, hold it there with
slight motion for about three seconds, off again, and quit.

The category and the strings were checked against the shipped plugin rather
than assumed: `build/.cpm-cache/bprqt/6.10.3/6.10.3/macos/plugins/platforms/libqcocoa.dylib`
declares `qt.qpa.input.mouse` among its logging categories and carries the
literals `cursor to`, `Synthesizing cursor update`, `Cursor update for` and
`resulted in new cursor`. If the log comes back with no `qt.qpa` lines at all,
the fallback is `QT_LOGGING_RULES='qt.qpa*=true'`, which is noisier and
certain.

**This is not a re-ask of AC3.** Nothing about the cursor changed in this
round, so asking the owner to judge whether it is fixed would be asking him to
confirm a failure already recorded. The v009 candidate asks him for the log
and for the hover-highlight question, and says plainly that the cursor is
unchanged.

Reading the result, once it exists:

- `Setting … cursor to <… resizeLeftRight>` then `Synthesizing cursor update`,
  then nothing further while the on-screen cursor is an arrow → hypothesis 1.
- `Setting … cursor to <resizeLeftRight>` with no `Synthesizing cursor update`
  after it → hypothesis 2.
- No `Setting …` line on that entry, with an earlier `resizeLeftRight` set
  never followed by a set back to the arrow → hypothesis 3.
- `Setting … cursor to <arrow>` logged while the pointer is on the lit seam →
  all three are wrong; the lines around it are the next lead.

Parked on human-review until that log exists. The scope's instruction to bring
the scope back if the cause turns out to be platform-wide rather than this
window's still stands and has not been reached.

## Packet pointer, 2026-09-12 (v009)

The discriminating check is candidate `BPR-0746-cursor-probe` in
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md), on the bundle built from the source at
aff9fc64 (executable sha256 `c2381f32…`, tests disabled, codesign clean),
candidate revision `app-release-c2381f32-2026-09-12.5`; prepared and protected
at sequence 357, undecided, no host running until the owner says so. Accepting
it means the probe was run; the evidence is the log, not the verdict.

## Attended verdict, 2026-09-12 (v009): accepted, and the log does not exist

The owner accepted `BPR-0746-cursor-probe` (revision
`app-release-c2381f32-2026-09-12.5`, fingerprint `8e91c0e8…` equal to the
prepared candidate's, decision `3e06fb9d-e8d8-4abd-901e-b5dab8cc4d1c`, actor
human, 2026-09-12T16:13:53Z), note in full:

> The pointer stuff works correctly. Dunno about the test you had me run.

The candidate's criterion was "the bundle is run from a terminal with the
logging line, the pointer rests on the seam for about three seconds and moves
off, and `build/logs/bpr-0746-cursor-probe.log` exists". **The objective half of
that criterion is not satisfied and the card does not close on this accept.**

### Why there is no log, established on the live processes

He ran the probe. The process was still up when this was written: pid 72017,
the bundle's own executable, started 2026-09-12T16:06:37Z from `ttys001` under
Ghostty, with pid 72018 `tee build/logs/bpr-0746-cursor-probe.log` on the other
end of its pipe. But `lsof` on 72018 shows its working directory as
`/Users/jaeger/Documents/GitHub` — one level above the repository — and its
only open descriptors as fd 0 on the pipe and fds 1 and 2 on the tty. **There
is no output file.** `/Users/jaeger/Documents/GitHub/build/logs` does not
exist, so `tee` failed to create the file and passed its input through to the
terminal alone. No `bpr-0746-cursor-probe.log` exists anywhere on disk. The
output survives only in that terminal's scrollback.

This is a defect in the packet, not in anything he did. The copy button was
labelled "The cursor probe: run this in a terminal, in the repository" and the
value it copied used an **absolute** path to the executable and a **relative**
path for `tee`. A label is not a working directory. His own note, "Dunno about
the test you had me run", is what a run that printed to a terminal and wrote
nothing looks like from his side.

The command, corrected, with no relative path anywhere in it:

    QT_LOGGING_RULES='qt.qpa.input.mouse*=true' \
      /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview \
      2>&1 | tee /Users/jaeger/Documents/GitHub/BetterPullReview/build/logs/bpr-0746-cursor-probe.log

Any packet that asks for a file on disk asks for it by absolute path, and its
criterion is the file, not the click.

## Reconciling "the pointer stuff works correctly" with this card

He says the pointer works. This card exists because it does not, and he
rejected the same seam eight hours earlier. Both statements are his, both are
first-hand, and neither is being discarded.

**It is not the binary.** The v008 bundle was built at `35438244` and the v009
bundle at `aff9fc64`. `git diff --stat 35438244 aff9fc64 -- src tests` is six
files: `appearance_profile.cpp`, `packaged_syntax_classifier.{hpp,cpp}` and
three test or fixture files — all of it [[BPR-0744]]. Nothing in that range
touches the splitter, the handle, cursors, event handling or the window chrome;
`git diff 35438244 aff9fc64 -- src/adapters/qt` contains no line matching
cursor, splitter, handle, `underMouse` or `paintEvent`. The executable he ran
is `c2381f32…`, recomputed on disk and equal to the prepared candidate's. On
this path the two bundles are the same program.

**It is not stale paint either, and that was measured rather than assumed.**
The obvious escape — that the lit rule in his v008 screenshot was left over
from an earlier repaint, so it never proved the pointer was on the handle — is
**false**. `test_the_split_cursor_stays_over_a_resting_handle` now measures it:
`Qt::WA_Hover` is set on the handle, the pointer's arrival costs one repaint and
its departure another, and `underMouse()` follows both, at scale 100 and 200.
So the highlight tracked the pointer, the screenshot does place his pointer on
the handle, and **this card's refutation of BPR-0739's cause 3 keeps its
foundation.** That check is in the test suite precisely so the foundation
cannot rot unnoticed: if it ever goes red, the v008 reading stops being
evidence and this diagnosis reopens.

**What is left is the gesture, and it fits hypothesis 1 exactly.** The two
reports describe two different things being done with the pointer:

- v007, [[BPR-0728]]: "the mouse cursor for the horizontal resize flickers only
  for a brief moment and the returns back to the normal cursor". A pointer that
  arrives and then **rests**.
- v008, BPR-0739: he put the pointer on the line and photographed it. A pointer
  that **rests**. Arrow.
- v009, `BPR-0739-seam-hover`: the candidate's own instruction was "Open the
  build, go to Read, and move the pointer **on and off** that rule to see both
  states. Then pick." A pointer that keeps **re-entering**. Works.

Hypothesis 1 on this card predicts precisely that: Qt sets the cursor once on
enter, AppKit reverts it on the next real motion, and Qt never re-asserts
because from its side nothing changed. Under that hypothesis every fresh entry
shows the resize cursor and every rest decays to the arrow — which is the
flicker he reported in v007, the arrow he photographed in v008, and the working
cursor he felt in v009, with no code change between any of them. The timing
supports it: he judged the hover question at 16:05:46Z, two minutes into a
session spent moving the pointer on and off the rule on purpose.

**This is an account, not a diagnosis, and it is not being recorded as one.**
It is consistent with every observation and it is what the missing log was
written to discriminate — the log's first reading is "`Setting … cursor to
<… resizeLeftRight>` followed by `Synthesizing cursor update`, then nothing
further while the on-screen cursor is an arrow: hypothesis 1". Hypotheses 2 and
3 are not excluded by any of this. Nothing is implemented on the strength of it.

### Consequences, stated plainly

- This card stays **Parked on human-review**. Its criteria are unchanged: the
  discriminating check has still not been run to a readable result, and no
  product source has been changed on this path.
- The accept is recorded against the candidate it was given for, and it is
  recorded as **not satisfying the candidate's own criterion**. It is not
  reworded to fit what happened.
- [[BPR-0739]]'s AC3 stays `fail`. It was not asked in v009, and an
  unsolicited remark about a resting-pointer criterion made while deliberately
  not resting the pointer is not a verdict on it.
- The exact remaining ask is one of two things, and the owner chooses which:
  either the terminal that is still open is saved to
  `/Users/jaeger/Documents/GitHub/BetterPullReview/build/logs/bpr-0746-cursor-probe.log`,
  or the corrected command above is run once more with the gesture the card
  asks for — **pointer onto the seam, then held still with only slight motion
  for about three seconds**, then off, then quit. The gesture is the variable
  under test, and resting is the half of it that fails.
- One number worth carrying: `qt_workbench_window` reports `handleWidth=4` on
  the offscreen style, against the "seven pixels" this card and [[BPR-0739]]
  both quote from the platform default. Four logical pixels with the rule in
  the boundary column is the owner's "it wasn't drawn in the right location".
  The macOS style's own value is still unmeasured.
