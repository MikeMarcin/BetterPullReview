---
id: BPR-0165
title: Document the review queue
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Document the review queue

`docs/architecture/review-queue.md`, in the manner of the review-ledger and
defect-workflow documents: what each region owns, the typed vocabulary, the
buckets and what puts an item in each, observer-rule grammar and persistence,
the refresh schedule and its limits, notification privacy, offline behaviour,
and what is not proven here.

## Required direct evidence

- The document states the local-only rule and the no-content rule as rules,
  with the reason each exists, not as descriptions of the code.
- Every claim in it is one a reader can check against a named test.
- `verify_documentation.py` and the issue-graph contract pass.

## What was done

`docs/architecture/review-queue.md`, in the manner of the review-ledger and
defect-workflow documents.

Both rules are stated as rules, with the reason each exists and the shape that
enforces it, rather than as descriptions of what the code happens to do:

**Observing is local** — and `enrollment_for` takes no provider while
`queue_membership` has no field a request could occupy, so there is no path to
guard.

**A notification carries no source and no comment body** — and the request type
has no field one could occupy, because redacting on the way out would be a
place a silent mistake could live.

Every number and behaviour in the document was checked against the source
rather than written from memory:

| Claim | Checked against |
|---|---|
| backoff 15 s doubling to a 900 s cap | `InitialBackoffSeconds`, `MaximumBackoffSeconds` |
| a path rule stops at a component boundary | the `'/'` test in `matches_path` |
| an exclusion keeps the role standing | `membership.Rule = rule; return membership;` — the role is never cleared |
| the enumeration order is the order of attention | `initial_review_requested` first, `quiet` last |
| `quiet` and authored do not notify | the two `return false` cases in `warrants_notification` |
| four distinct departures | the `enrollment_departure` enumerators |
| the model has no mutating entry point | no `setData` in `review_landing_model.cpp` |

The three gaps are named in the document rather than left for a reader to
discover: BPR-0168's attended notification delivery, BPR-0169's live reviewer
state, and BPR-0170's durable store — with the reason the third matters, which
is that a queue rebuilt on every launch would re-derive membership and undo
exactly what the retention policy exists to protect.

Filters and sorting are recorded as not built.
