---
schema: "implement-rfc.card.v1"
id: "BPR-0928"
title: "Expand Overview descriptions into a retained central reader"
status: "Backlog"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: ["BPR-0918"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"},"implements":["BPR-0918","RFC-BPR-0037:AC-01"]}
---

# Expand Overview descriptions into a retained central reader

This is the runtime follow-up selected by [[BPR-0918]] for
[RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md) AC-01.
The design evidence is
[`overview-description-layout-study.md`](../../rfcs/plans/rfc-bpr-0037-first-pr-workflow/overview-description-layout-study.md).

## Selected design and scope

Keep a natural compact preview on Overview and open a retained expanded reader
inside the central Overview surface. The expanded reader uses the available
window space, has one vertical scroll owner and keeps a fixed **Return to
Files** control outside the text viewport. It is not a new application mode,
dialog, top-level window or web view.

Preserve the existing accepted body and authority lifecycle. The summary and
expanded reader share one body identity. Files keeps its stable selection and
table position. RFC-BPR-0036 remains the sole owner of Markdown parsing and
rendering; this card must preserve the future body-rendering boundary without
implementing Markdown.

## Acceptance

- Empty and short descriptions remain compact. Synthetic long and very-long
  descriptions, valid Unicode and the 262,144-byte admitted boundary remain
  completely reachable, selectable and copyable; an over-limit body is refused
  rather than silently shortened.
- Mouse and keyboard activation through the composed workbench opens the reader
  without a fixed expanded-height cap. Exercise 800×800 and 1400×800 Overview
  pages, 13/20 point fonts, 1×/1.25×/2× scale and a short-height shipped window.
- Page/document navigation, selection/copy, Tab/Shift+Tab and supported native
  wheel/trackpad input use the single reader viewport. Record target-native
  input observations separately from offscreen layout checks.
- Return to Files and local Escape restore the summary and focus without
  changing file selection/order or emitting file activation, mode or
  publication intent. Reopening restores the logical source-text reading anchor
  and valid selection, not merely a raw scrollbar integer.
- Same-body title, receipt, participant and history refreshes do not replace the
  document or reset place. Body edits clamp the source anchor and announce the
  update. Width/font/scale reflow retains the same visible source anchor within
  one wrapped line. When document-end geometry makes top alignment impossible,
  the exact anchor line remains fully visible at the nearest legal scroll
  position; no synthetic blank content is inserted to force alignment.
- Exact PR/account/host replacement clears old text and reader state before new
  content; stale projections cannot revive it. Inherited appearance, focus and
  accessible names/states remain usable without QSS or a forced style.

## Exclusions

Do not change provider limits, Files ordering, review navigation/publication,
conversation layout, Markdown policy or durable reading-position persistence.

## Status

Allocated by RFC-BPR-0037's overview/conversation design milestone. Runtime
implementation and native evidence remain pending.
