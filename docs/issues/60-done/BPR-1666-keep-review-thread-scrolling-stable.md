---
schema: "implement-rfc.card.v1"
id: "BPR-1666"
title: "Keep Review scrolling stable after opening a new thread"
status: "Done"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner v0.3.0 report, 2026-10-19; v0.3.1 release blocker","reservation":"docs/issues/_reservations/BPR-1666--BPR-1666--2a12e1d9417b.json","milestone_plan":{"id":"bpr-1666-thread-scroll","path":"docs/rfcs/plans/bpr-1666-thread-scroll/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8964ac489a8ed6ad5021a80f69f86d92fcdf3abd","evidence":{"kind":"observation","detail":"Old product edec63c78 (diff source identical to v0.3.0/b433d515f) fails both regressions: overlay viewport1..583 vs bar584..601, and reported navigation route with overlay visibility transition snaps q7842 to3682 with9 total rebuilds. Baseline CTest exits8; both child exits1. Plain wheel/idle without visibility transitions did not reproduce. Logs /tmp/bpr-qt612-run/scroll-baseline-{ctest.log,result.json}."}},{"criterion":"AC2","outcome":"pass","revision":"8964ac489a8ed6ad5021a80f69f86d92fcdf3abd","evidence":{"kind":"observation","detail":"qt_thread_scroll_stability_scale_{100,125,200} passes through Overview unread status -> Only new Threads -> actual double-click -> Review, wheel to the final line of one100-paragraph comment, including notes overflow after source end; every delayed position is unchanged and the idle state settles. Existing qt_composed_long_thread_scale_{100,125,200} also passes its thread-foot, pick, Tab, width/zoom/one-pane/growth controls. Logs /tmp/bpr-qt612-run/scroll-integration-{ctest,lasttest}.log."}},{"criterion":"AC3","outcome":"pass","revision":"8964ac489a8ed6ad5021a80f69f86d92fcdf3abd","evidence":{"kind":"observation","detail":"qt_diff_workbench_scale_{100,125,200} passes native-overlay full extent and show/hide row/origin invariants plus legacy bar/divider/fractional/correspondence controls. Overlay extent is now1..601 for bar584..601; reported-route rebuild count stays4 for all wheel/visibility/overflow turns and idle. All3 window and thread-move scales pass. Renders inspected, retained at /tmp/bpr-qt612-run/scroll-renders; offscreen forced-style mechanics only."}},{"criterion":"AC4","outcome":"pass","revision":"8964ac489a8ed6ad5021a80f69f86d92fcdf3abd","evidence":{"kind":"observation","detail":"20 distinct focused rows pass (23 executions including initial100% checks and pre-strengthening route rows). Exact application_diff_workbench_public_path6.76s passes real installed-Git/cache composition, conversation anchor and full-window render. architecture_boundaries, clang-format22.1.8, strict clang-tidy22 on all3 changed TUs, DOCUMENTATION_OK, all67 documentation-verifier tests and git diff --check pass. Native Windows/Linux, physical trackpad/native skin and release-package qualification stay with release owner; no such claims made."}}]
---

# Keep Review scrolling stable after opening a new thread

The owner opens Overview's threads-with-new-posts status, enters New Threads,
and double-clicks a thread into Review. Scrolling long comments repeatedly snaps
upward. Clicking sometimes permits scrolling; at rest the left pane shifts or
flickers, and the horizontal scrollbar disappears and reappears as diff text moves.
Repeated reveal and scrollbar/layout feedback are hypotheses pending reproduction.

Repair the existing reading behavior without changing its visual design. Use
sanitized offscreen fixtures; never drive the owner's desktop or personal apps.
Baseline: `b433d515f`. Mandate: `thread-scroll-blocker-r1`.

## Acceptance

- AC1: Reproduce the failing interaction deterministically through the production
  Qt window, identify the cause, and show the regression detects the old behavior.
- AC2: Opening a thread still reveals its selected post; subsequent user scrolling
  reaches long content without delayed snap-back and rests at a stable position.
- AC3: Idle native scrollbar visibility and diff viewport geometry settle; retain
  ordinary native scrolling, selection, font/resize reflow and exact thread placement.
- AC4: Focused composition and adjacent Qt tests, touched-file coding checks and
  documentation gates pass; record platform limits and hand off a committed candidate.

## Execution record

Owner clarification (2026-10-19): "The horiziontal scrollbar used to just draw
transparently over the diff text but now it seems like it has an opaque
background and actually shifts the diff window". Frontend-design was loaded;
this repair restores the native overlap policy without a visual redesign.

The source viewport's fractional-origin placement clipped every visible
horizontal bar into a separate band, including a native overlay. That changed
its measured height and shared synchronization rail. A subsequent rebuild
revealed the selected thread again, undoing the user's reading position.
Commit `897df7ef0` introduced the clipping; this code is identical in v0.3.0
and the Qt6.12 baseline `b433d515f`, so the diagnosis does not attribute the
regression to the upgrade.

The old product fails both new controls: an overlay occupies y584–601 while
the text viewport stops at583; the production window's Overview -> new
Threads -> double-click -> long-comment wheel route, with the overlay's
show/hide transition between turns, snaps shared position7842 back to3682
and rebuilds geometry9 times. Without the visibility transition, plain wheel
and idle did not reproduce the problem. The regression deliberately exercises
that missing transition rather than assuming every wheel must trigger it.

Source fix `3a6d1acbd` retains Qt's full viewport when the native overlap
metric is positive and keeps the legacy bar's separate band. The same source
now extends through y601, retains all18 initial wheel positions and has4
geometry rebuilds total, unchanged through scrolling and idle. Test refinement
`8964ac489` additionally requires reaching the final line of the complete
100-paragraph comment. Detailed verification and command state are in the
[checkpoint handoff](../_checkpoints/BPR-1666/handoff.md).

The change is confined to Qt presentation. Provider, core policy, credentials,
persistence, publication, package versions and data formats have no impact.
Offscreen forced-style evidence qualifies geometry and interaction, not the
native visual skin or physical trackpad behavior. Native Windows/Linux and
release-package qualification remain with the subsequent release milestone.
