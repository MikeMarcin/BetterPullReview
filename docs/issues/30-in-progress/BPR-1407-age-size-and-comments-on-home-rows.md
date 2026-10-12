---
schema: "implement-rfc.card.v1"
id: "BPR-1407"
title: "Show each Home row's age, line changes and comment count"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1407--BPR-1407--fa077b5c77ea.json","origin":"owner request 2026-10-12"}
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
  unknown; a present malformed one is refused like the read's other fields.
  The facts survive opening the pull request and a restart: the private
  store's facts encoding gains them and still reads the previous version.
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
