---
schema: "implement-rfc.card.v1"
id: "BPR-0918"
title: "Investigate a larger overview description for long PR bodies"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"Trace where body length and visible height are limited; distinguish provider truncation from viewport constraints and document the evidence.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"artifact","detail":"overview-description-layout-study.md traces the 262,144-byte fail-closed admission through accepted projection to the complete plain-text body and the independent 110-pixel Qt viewport cap; no owner/server truncation is inferred."}},{"criterion":"Compare inline expansion, resizing and a dedicated reading surface across representative widths, fonts, scales, keyboard, selection/copy and scroll ownership.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"observation","detail":"The study compares all three options and records 36 full-body native-library probe cases at 800/1400 widths, 13/20-point fonts and 1x/1.25x/2x scale, including full copy, end visibility, PageDown and keyboard return."}},{"criterion":"Recommend and demonstrate full-body reading, expansion/restoration and Return to Files without losing place; describe short/empty and refresh/replacement behavior.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"observation","detail":"The selected retained central reader reaches the complete synthetic body. Its digest-bound source-anchor correction preserves directed selection and keeps the exact anchor visible in 864/864 Return-to-Files/reopen courses; disabled adjustment fails, and short/empty, same-body refresh, edit clamping and authority replacement rules are explicit."}},{"criterion":"Record a concrete design recommendation and bounded implementation follow-up with runtime acceptance.","outcome":"pass","revision":"fe6ac3633a61bf5fc4abab0057f6546d412c6446","evidence":{"kind":"artifact","detail":"BPR-0928 is canonically allocated and linked to AC-01 with full-body, input, retention, invalidation, accessibility and target-native acceptance; runtime AC-01 remains pending."}}]
---

# Investigate a larger overview description for long PR bodies

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-01. Earlier RFC references retain their governing contracts.

## Owner feedback

After loading the first PR in 0.1.3: “Lots of our PR descriptions are very long
and the current layout can only draw 6 lines.” The owner requested a task to
figure out how to show a larger description on Overview.

## Confirmed starting point

`src/adapters/qt/src/pull_request_overview_page.cpp` creates `overviewBody` as a
read-only `QPlainTextEdit` with `setMaximumHeight(110)` and a Maximum vertical
size policy. Projection applies the body as plain text. This constrains visible
height; it is not evidence that the provider truncated the stored description.
Check the complete provider-to-projection path during investigation before
attributing any missing text to layout alone.

## Investigation and deliverable

Compare at least an inline expand/collapse design, a user-resizable description
region, and a dedicated expanded reading surface. Consider a page-level scroll
model as part of the inline option. Explain the recommended interaction and its
tradeoffs with access to Files, overview actions and the surrounding workbench.
Avoid replacing a six-line box with an equally restrictive arbitrary height.

Produce reviewable native prototypes or layout artifacts using synthetic short,
long and very long descriptions. Include headings, checklists, wide code and
tables for compatibility with [RFC-BPR-0036](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md).
The investigation can proceed on plain text and has no Markdown implementation
dependency. Do not copy the owner's private PR description into fixtures.

## Acceptance

- Trace where body length and visible height are limited; distinguish provider
  truncation from viewport constraints and document the evidence.
- Compare the options with representative narrow/wide windows, larger fonts and
  display scaling, keyboard navigation, selection/copy and nested scrolling.
- Recommend and demonstrate how a reviewer reaches the entire admitted body,
  expands/restores the reading space and returns to Files without losing place.
  Describe short/empty-body behavior and position retention on refresh/PR change.
- Record a concrete design recommendation and bounded implementation follow-up
  with its acceptance checks. Do not mark runtime behavior fixed by this
  investigation or choose a new layout solely from an arbitrary pixel increase.

## Status

Selected design recorded in
[`overview-description-layout-study.md`](../../rfcs/plans/rfc-bpr-0037-first-pr-workflow/overview-description-layout-study.md):
a retained expanded central description reader with one scroll owner and a
fixed Return to Files control. Synthetic Qt evidence distinguishes the
262,144-byte admission boundary from the 110-pixel viewport, reaches the full
body across the width/font/scale matrix and demonstrates why raw scrollbar
replay cannot retain place after reflow. A corrected source-anchor prototype
then preserves exact selection and keeps the captured source anchor visible in
864/864 Return-to-Files/reopen courses across the same geometry, including the
explicit document-end clamp. [[BPR-0928]] is the linked runtime follow-up. This
investigation does not claim AC-01 runtime acceptance.
