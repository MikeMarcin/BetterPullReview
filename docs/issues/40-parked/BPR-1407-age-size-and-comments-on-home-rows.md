---
schema: "implement-rfc.card.v1"
id: "BPR-1407"
title: "Show each Home row's age, line changes and comment count"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1407--BPR-1407--fa077b5c77ea.json","origin":"owner request 2026-10-12"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8cc80c6ed","evidence":{"kind":"observation","detail":"github_paged_read, application_review_queue_enrollment and sqlite_queue_enrollment_storage pin reading, merging, v2/v1 encoding and the schema 10 upgrade; no request added."}},{"criterion":"AC2","outcome":"pass","revision":"8cc80c6ed","evidence":{"kind":"observation","detail":"qt_review_landing_model pins the texts and clock; qt_home_list_scale_{100,125,200} pin the facts column's geometry against the hover action, tag and reason; offscreen renders inspected."}},{"criterion":"AC3","outcome":"pass","revision":"8cc80c6ed","evidence":{"kind":"observation","detail":"Accessible description says the facts in words; 76/76 focused suites pass; format, clang-tidy (pre-existing findings only) and documentation gates pass."}},{"criterion":"AC4","outcome":"pending","revision":"8cc80c6ed","evidence":{"kind":"freeform","detail":"Owner look on frozen root /Users/jaeger/bpr1407-look.5jquigqu in attended session 7cf6171a73b084a2ae161dcd95702e61."}}]
---

# Show each Home row's age, line changes and comment count

The owner, 2026-10-12: "I would like for home to also show me the age of the
changelist (days since created) and overall changes (+/- lines diff counts)
and maybe total number of comments." A queue row says who opened it and why
it is listed, but not how old it is, how big it is or how much has been
said on it, and the right of every queue row is empty: Time and Progress
are projected only for Recently viewed.

The pull request read Home already makes for each row (`GET pulls/{n}`, for
a discovered row's summary and for a retained row's refresh) returns
`created_at`, `additions`, `deletions`, `comments` and `review_comments`.
No new request is needed.

## Design

The frontend-design skill was loaded for this work.

- **Colour.** None added. On Home colour means state and nothing else
  (workbench specification), so the counts are not GitHub's green and red:
  a red "−318" would read as a failure. Every fact is in the row's quiet ink.
- **Type.** The row's secondary face with tabular figures, so right-aligned
  numbers line up from row to row like a ledger; a true minus sign; the
  locale's digit grouping.
- **Layout.** Each fact sits on the line it is about, right-aligned:

  ```
  octo-org/review-repository#4003  Discovered pull request 4003            +1,204 −318
  Opened by contributor-a; my review is requested. Requested   6 days old, 14 comments
  ```

  The size of the change faces the title that names it; the age and the
  conversation face the sentence about the row's state. The hover "Open on
  GitHub" icon keeps its own gutter, so it never covers a figure.

## Acceptance

- AC1: Discovered and retained rows take the creation time, lines added and
  removed, and the conversation plus inline comment count from the pull
  request read Home already makes; no request is added. An absent field is
  unknown. A retained read refuses a malformed one like its other fields; a
  discovered row's summary is optional, so there it stays unknown. A read
  that does not report a fact keeps the last known one, so the facts survive
  opening the pull request and a restart. The private store's facts encoding
  gains them (version 2, still reading version 1) and the queue store moves
  to schema 10 with version nine's tables, so an earlier build refuses the
  file as newer instead of failing on its facts.
- AC2: A queue row shows "+N −M" beside its first line and "<age>, <count>"
  beside its second, right-aligned in the quiet ink with tabular figures.
  Age is whole elapsed days ("Under a day old", "1 day old", "6 days old")
  to the clock the composition supplies; Qt reads no clock. "No comments",
  "1 comment", "14 comments". An unknown fact draws nothing. The hover icon
  never covers a figure. Recently viewed rows are unchanged.
- AC3: The accessible description says the facts in words ("opened 6 days
  ago; 1,204 lines added and 318 removed; 14 comments"). Home, landing,
  discovery, enrollment and window suites pass; format, lint and
  documentation gates pass.
- AC4: The owner accepts the look (attended review on a frozen build).

## Evidence

Candidate `8cc80c6ed`. The frontend-design skill was loaded for this work.

- AC1: `github_paged_read` reads the five fields from the one pull request
  read on both the discovered and retained paths, keeps absent fields
  unknown, needs both comment counts for a total, refuses a malformed fact
  on a retained read and leaves it unknown on a discovered summary, and
  issues no extra request. `application_review_queue_enrollment` pins that a
  read without the facts keeps the last known, a partial read replaces only
  what it reported, the private record keeps them, version 2 round-trips and
  version 1 still reads. `sqlite_queue_enrollment_storage` upgrades a
  version-nine file with first-encoding facts to schema 10 and round-trips
  the change facts, zero kept as zero.
- AC2: `qt_review_landing_model` pins the texts (whole days, singulars,
  "Under a day old", "no comments", capitalised leading clause, true minus,
  locale grouping), no age without a clock, and that `set_now` changes only
  rows whose day count moves. `qt_home_list_scale_{100,125,200}` pins that
  the facts sit inside their row, never meet the hover action, the Reviewing
  tag or the reason word, share one right edge across rows, and that a row
  without facts draws none. Offscreen renders (light, dark, high contrast,
  1100 and 520) of the composed Home course were inspected. Its first 1100
  grab precedes the list's relayout, as BPR-1405 recorded; later grabs show
  the wrapped row sized correctly.
- AC3: the accessible description appends the facts in words, pinned in
  `qt_review_landing_model`. 76/76 of
  `overview|landing|home|personal_discovery|composed_github|workbench_window|review_queue|queue_enrollment|paged_read|sqlite`
  pass on the dev build; the format gate passes; clang-tidy on the changed
  sources reports only pre-existing findings (`queue_enrollment_database.cpp`
  open flags, `github_paged_read.cpp` `note_limit`);
  `verify_documentation.py` passes.
- AC4: pending. Frozen root `/Users/jaeger/bpr1407-look.5jquigqu` (launcher
  check `CHECK_OK`), attended session `7cf6171a73b084a2ae161dcd95702e61`,
  candidate `BPR-1407-home-facts`.
