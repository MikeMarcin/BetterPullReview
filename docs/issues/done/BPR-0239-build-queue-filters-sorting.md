---
id: BPR-0239
title: Build review-queue filters and sorting presentation
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0231]
---

# Build review-queue filters and sorting presentation

RFC-BPR-0010 allocates filters and sorting to the Qt landing surface. The
existing model presents the core order incrementally, but no filter or sorting
presentation was built.

## Required final state

- Provide explicit filters for the RFC-defined roles, observer reasons,
  attention buckets, repositories, and quiet/current state.
- Preserve the core's deterministic priority order unless the reviewer selects
  a documented alternate sort; never silently reorder by presentation state.
- Keep stable pull-request identity and incremental model signals across
  filtering and sorting changes.
- Persist only reviewer preferences, not derived queue membership.
- Prove accessible controls, empty/filter explanations, focus retention, and
  large-queue bounded updates through the composed landing surface.

## Implemented state

- A Qt filter/sort proxy sits above the unchanged core-ordered landing model.
  The default disables proxy sorting; repository and pull-request-number order
  activate only through explicit reviewer gestures and retain a total identity
  tie-break.
- Accessible role, observer-rule-kind, attention-bucket, repository, and
  quiet/current selectors apply incrementally without resetting the model.
  Empty filtered results explain that the underlying queue still has rows.
- The application host coalesces gestures while a store request is pending and
  publishes the accepted preference only after persistence succeeds. SQLite
  schema v3 stores one bounded preference row and no derived queue membership;
  canonical v2 migration and malformed text are fail-closed.
- The repository selector intentionally groups equal owner/name values across
  host and account authorities. Complete pull-request identity remains on each
  row and is never replaced by that presentation preference.
- The real two-process composition contract drives all filters and an explicit
  alternate sort through the shipped Qt controls, reopens the same store, and
  observes the exact preference with zero provider mutation.

## Verification

- `application_review_queue_enrollment`
- `sqlite_queue_enrollment_storage`
- `qt_review_landing_model`
- `qt_workbench_window_scale_100`
- `application_queue_composition_contract`

The focused matrix passes 5/5. Direct counterexamples switch between alternate
sorts whose repository and pull-request-number orders disagree, inject an
embedded NUL into persisted repository text, and publish 2,048 rows through the
composed window before a non-contiguous filter and explicit sort. The large
path retains the selected pull request and focused control with bounded signals
and no reset. Independent closeout review found no remaining blocking or
high-risk issue. Qt 6.8 compatibility is guarded and code-inspected; the local
environment does not provide an installed Qt 6.8 runtime for target execution.
