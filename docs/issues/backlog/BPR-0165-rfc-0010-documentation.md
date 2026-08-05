---
id: BPR-0165
title: Document the review queue
status: Backlog
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
