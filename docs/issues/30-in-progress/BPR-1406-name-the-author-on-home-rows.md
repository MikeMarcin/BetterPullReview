---
schema: "implement-rfc.card.v1"
id: "BPR-1406"
title: "Name the author on every Home row that is not mine"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1406--BPR-1406--feceff319165.json","origin":"owner request 2026-10-12"}
acceptance: []
---

# Name the author on every Home row that is not mine

The owner, 2026-10-12: "I want to see the author's name on the home when I
look at a pr there." A Home row named its author only when my review was
requested ("Opened by octocat; my review is requested"). A changed review,
an unread reply, a fix to verify and anything I follow said why without
saying whose pull request it was, although the queue carries the author for
every row.

## Acceptance

- AC1: A queue row for someone else's pull request leads its second line with
  "Opened by <login>" in every bucket, and the reason continues the sentence
  ("Opened by octocat; the code moved since my last review"). A failed
  check's name after it keeps its own case. A row without a known author
  says nothing about one.
- AC2: My own rows never name me. "My pull request; nothing about it has been
  read yet" continues its sentence in lower case like the rest.
- AC3: The accessible description follows the drawn sentence. Home, Overview,
  discovery and window suites still pass; format, lint and documentation
  gates pass.

Recently viewed stores no author (`recent_review_entry`), so its rows are
unchanged; giving it one is a store schema change and a separate card.

## Evidence

- AC1, AC2: `qt_review_landing_model` pins every bucket with and without an
  author, a followed row with its rule, a blocked-only row
  ("Opened by octocat; Branch Freshness failed."), and authored rows.
- AC3: the accessible description is built from the same sentence and
  `qt_review_landing_model` pins that equality. 52/52 of
  `overview|landing|home|personal_discovery|composed_github|workbench_window|review_queue`
  pass on the dev build; the format gate passes, clang-tidy on
  `review_landing_model.cpp` reports nothing, `verify_documentation.py`
  passes. The offscreen Home waiting render was inspected.
