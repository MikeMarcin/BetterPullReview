---
schema: "implement-rfc.card.v1"
id: "BPR-0928"
title: "Expand Overview descriptions into a retained central reader"
status: "Parked"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: ["BPR-0918"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"},"implements":["BPR-0918","RFC-BPR-0037:AC-01"]}
parked_on: "human-review"
acceptance: [{"criterion":"Empty and short descriptions remain compact; admitted long, Unicode and 262144-byte bodies are complete and over-limit bodies refuse.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"qt_pull_request_overview_page passes compact empty/short cases, long and very-long Unicode bodies, the admitted boundary, copy/selection and explicit over-limit refusal."}},{"criterion":"Composed activation opens the uncapped reader at required window, font and display-scale combinations.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Overview and workbench tests exercise mouse/keyboard activation, 800x800 and 1400x800, 13/20 point fonts, short-height fallback and 1x/1.25x/2x scale without a fixed expanded-height cap."}},{"criterion":"Page/document navigation, selection/copy, tab traversal and supported native wheel/trackpad input use the single reader viewport.","outcome":"pending","revision":"8939de71","evidence":{"kind":"artifact","detail":"Deterministic keyboard, selection, copy and single-scroll-owner assertions pass. Physical macOS wheel/trackpad and focus traversal remain queued as candidate rfc0037-overview-reader in docs/verification/reviews/rfc-bpr-0037-thread-reading-v001/candidate-manifest.json."}},{"criterion":"Return and Escape restore summary, focus, files and the logical source reading anchor without extra intents.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Overview/workbench regressions preserve stable file indices, selection/order, logical source anchor and selection while emitting no file activation, modality or publication intent on Return/Escape."}},{"criterion":"Same-body refresh and reflow retain place; body edits clamp and announce; document-end geometry remains legal.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Focused tests preserve document identity and source-anchor position through title/receipt/history refresh, width/font/DPR reflow and clamp changed bodies and end-of-document positions without synthetic padding."}},{"criterion":"Authority replacement clears retained text and state; inherited appearance, focus and accessibility remain usable without implementing Markdown.","outcome":"pending","revision":"8939de71","evidence":{"kind":"artifact","detail":"Authority-clear and stale-projection regressions pass and the reader remains plain text, preserving RFC-BPR-0036 ownership. Native VoiceOver and subjective light/dark/high-contrast judgment remain queued as candidate rfc0037-overview-reader."}}]
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
