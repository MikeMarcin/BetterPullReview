---
schema: "implement-rfc.card.v1"
id: "BPR-0266"
title: "Restore epoch public-path presentation after focused navigation"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0264"]
---

# Restore epoch public-path presentation after focused navigation

The macOS `dev` continuation after BPR-0264 exposed a distinct unattended
presentation stall. Epoch persistence, maintenance, and appearance-gesture
courses reach the refreshed two-epoch projection with ready status, exact
base/head pair, and completed fixture transport, but wait at the epoch-refresh
gate until their run budgets expire. The aggregate verdict does not identify
whether the current failure is the general pull-request presentation, epoch
cause row, or another exact predicate.

## Required final state

- The epoch fixture reports the exact failed presentation predicate, expected
  value, and observed value without leaking credentials or repository-private
  content.
- Persistence and maintenance courses advance only when the same exact host,
  repository, pull request, OIDs, epoch ordinals, primary cause, and history
  rows required by the existing contract are present.
- The macOS GitHub.com and GHES persistence courses, maintenance course, and
  any appearance course that uses the epoch gate complete reliably against the
  current focused-review projection.
- The repair retains the application/core decision path and does not bypass
  refresh, fabricate history rows, or weaken bounded timeout behavior.

## Non-goals

- Do not alter accepted focused-navigation product behavior merely to satisfy
  a stale fixture expectation.
- Do not execute a live target, use a credential, or claim Windows/Linux
  runtime evidence.
- Do not change diff-interaction geometry evidence or network isolation.

## Verification

- Rebuild the development application and affected epoch/Qt targets.
- Run both epoch persistence variants, epoch maintenance, appearance gesture,
  and their nearest core/application/Qt component selectors.
- Run the architecture boundary pair, repository format/tidy, documentation,
  and diff gates.
- Review the final state-transition and diagnostic evidence independently.

## Closeout

The restored gate preserves two exact presentation states instead of weakening
one row count: initial open requires five review-wide roots and nine preorder
identities; refreshed `review.cpp` focus requires three thread roots and their
four exact comment children. Host, repository, pull request, title, actions,
selected thread, Base-to-HeadTwo OIDs, ordinal-two force-push/rebase cause, and
zero full-model resets remain mandatory. Failure-only testing telemetry reports
each presentation operand and the cause guardrail with record separators
escaped and no comment bodies or source content.

GitHub.com and GHES two-process persistence, the appearance gesture course,
and focused conversation-model evidence passed. The maintenance epoch/prune
path passed; BPR-0269 then removed its independent stale ledger-load wrapper
marker and the complete maintenance contract passed. Scoped format/tidy,
documentation, diff, manifest, and architecture gates passed. Independent
review returned `ACCEPT` with no blocking or high-risk finding.
