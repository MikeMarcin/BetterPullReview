---
schema: "implement-rfc.card.v1"
id: "BPR-0628"
title: "Yield between bounded batches of native pane measurement"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0627"]
---

# Yield between bounded batches of native pane measurement

BPR-0627's temporary phase instrumentation of the real 20,000-line course on
source `d698b338` measured 164.10 ms for the left pane and 163.25 ms for the
right pane in one GUI callback. Axis construction/adoption were 1.20/0.43 ms.
Native block measurement accounts for the remaining approximately 339 ms pause.
Keep the actual Qt geometry; do not substitute uniform line-height estimates.

## Bounded acceptance and ownership

- Own native measurement under `src/adapters/qt/{include/bpr/qt,src}/measured_alignment_axis.*`,
  its `diff_workbench` integration, and focused Qt/composed tests. Retain a
  bounded amount of GUI-thread measurement per continuation, with input/event
  processing opportunities between batches. Qt widgets remain on the GUI thread.
- Publish only a complete, validated two-pane axis for the exact current
  comparison and presentation generation. Replacement, clear, shutdown, font,
  zoom, width/wrapping, DPI and layout invalidation must cancel/restart stale
  measurement without stale callbacks installing geometry.
- Keep viewport/line boundary values byte-for-byte or numerically equivalent to
  the retained synchronous measurement for native mixed-height/wrapped/Unicode
  cases. Preserve cached scrolling, exact endpoints, selection and source-line
  identity; no partial axis, source truncation or hidden budget increase.
- Prove bounded batches and a real input callback between them, plus cancellation
  and invalidation during measurement. Exercise the actual stored large-review
  comparison and its exact rendered source/scroll/navigation course on this Mac.
- Target short measurement turns (initial design budget 4 ms, checked between
  bounded line groups) and record actual maximum/p95 intervals. One indivisible
  Qt block layout is a stated limit, not an assumed universal time bound.
  BPR-0629 owns the separate approximately 111 ms semantic-index step.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
No new performance subsystem or production logging surface. All tests stay local
with synthetic content and denied network access. No GitHub Actions, push, paid
CI, credentials or production app launch. Native Windows/Linux budgets and
pathological individual-line policy remain separate RFC-0013 acceptance.
