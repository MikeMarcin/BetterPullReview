---
schema: "implement-rfc.card.v1"
id: "BPR-0096"
title: "Virtualize code rows for large files and high DPI"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0095"]
---

# Virtualize code rows for large files and high DPI

Virtualize code row rendering so widget count is bounded by visible UI structure rather than source-line count, at 1.0x, 1.25x, and 2.0x.

Prove it with an instrumented large-file harness that records widget count, and require the count to stay bounded as file length grows by orders of magnitude. A test that merely completes rendering is not sufficient.

## What is actually true

No virtualization layer was written, and none should have been. The source
panes are `QPlainTextEdit`, which virtualizes its own text layout internally
and creates no widget per line. The RFC's requirement that "widget count
remains bounded by visible UI structure rather than source-line count" is
therefore satisfied by construction rather than by new machinery.

That is a finding, not an excuse to skip the evidence. The criterion had no
direct proof, so this slice supplies one. Inventing a custom row-widget
virtualization layer would have added a second presentation authority the
design never asked for.

## Direct verification

The harness presents 60, 6,000, and 60,000 source lines through the same
composed workbench at 1.0x, 1.25x, and 2.0x, and counts
`findChildren<QWidget*>()` on the window, on the comparison surface, and on the
right source pane.

Recorded on macOS Sequoia 15.7.7 arm64 with Qt 6.11.1:

| Source lines | 60 | 6,000 | 60,000 |
|---|---|---|---|
| Window widgets | 68 | 68 | 68 |
| Surface widgets | 29 | 29 | 29 |
| Right-pane widgets | 5 | 5 | 5 |
| Visible link ribbons | 6 | 6 | 6 |
| Publish milliseconds | 2 | 45 | 500 |

A thousandfold increase in reviewed source produces zero additional widgets
anywhere in the window. The assertions bind the ratio, not the absolute count:
the test also requires the source to have genuinely grown by orders of
magnitude, requires the sixty-thousand-line comparison to exceed its viewport,
requires the whole retained widget tree to stay orders of magnitude smaller
than the source it presents, requires painted correspondences to follow the
canonical change count rather than source length, and requires scrolling to
rebuild no measured geometry.

Elapsed time is recorded and deliberately not asserted. One machine's timing is
not a frame-rate guarantee, and turning it into a threshold would make the
suite fail for reasons unrelated to the claim. The three scale cases complete
in about three seconds.

- Non-vacuity: connecting a handler that creates one hidden child widget per
  thousand source blocks — so the count scales with file length exactly as the
  RFC forbids — fails with "a file a thousand times longer creates no
  additional widget anywhere in the workbench window". Restored, and the source
  tree verified byte-identical to HEAD afterwards.
- One process note worth recording: the first attempt to apply that break
  silently failed to patch, because an earlier slice had changed the anchor
  text. The build then reported `ninja: no work to do` and the suite passed. A
  green run from an edit that never landed looks exactly like a green run from
  a passing implementation, so the patch application was checked before the
  result was believed.
- `qt_workbench_window` passes 3/3, the full Qt suite 24/24, both architecture
  boundary suites, the format gate, and strict `clang-tidy` with
  `--warnings-as-errors`.

## Retained risks

- Widget count is the RFC's stated proxy for virtualization. Peak memory and
  frame time are recorded as context but not asserted; the verification matrix
  names them for the instrumented-application surface, which stays open.
- Publishing 60,000 lines takes about half a second on this machine. That is
  observation, not a budget; RFC-BPR-0013 owns performance budgets.

## Result

The workbench's widget tree is bounded by visible UI structure and does not
grow with source length, proven by ratio across three orders of magnitude
rather than by a single large render completing. BPR-0096 is complete.
