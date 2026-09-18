---
schema: "implement-rfc.card.v1"
id: "BPR-0918"
title: "Investigate a larger overview description for long PR bodies"
status: "In Progress"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
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

Not started. This is investigation/design work; implementation follows the
resulting scoped task and owner direction.
