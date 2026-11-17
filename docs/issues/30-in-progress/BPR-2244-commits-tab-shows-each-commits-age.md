---
schema: "implement-rfc.card.v1"
id: "BPR-2244"
title: "The Commits tab shows how old each commit is"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"enhancement","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner request, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2244--BPR-2244--3d9221f87624.json","related":["BPR-1407","BPR-1575"],"acceptance":["BPR-2244:read","BPR-2244:row","BPR-2244:look"]}
---

# BPR-2244: The Commits tab shows how old each commit is

## Request

The owner, 2026-11-17: "In the commits tab, it would be nice to see how old
the commit was. Maybe days ago like we have for the home screen on PR age."

## Scope

- The GitHub reads (recorded and paged) keep each commit's committer date
  (`commit.committer.date`) as seconds. The paged (live) read leaves the age
  unknown for a record without one or with one in another shape and refuses
  nothing. A recording may keep exactly `committer: { date: <string> }`; an
  unparsable string leaves the age unknown.
- The commit filter's projection carries it on each row.
- Each Commits tab row says the age on its detail line, in the words the
  window already uses for ages ("just now", "2 min ago", "3 hr ago",
  "2 days ago", `refresh_age_words` over the core's `refresh_age_at`). The
  clock is the host's (the Home clock the window is given); core reads none.
- The row's accessible name includes "committed 2 days ago"; its tooltip
  names the exact local date and time.

## Acceptance

- `BPR-2244:read`: both GitHub readers keep the committer date, and an
  absent or malformed date leaves it unknown without failing the read.
- `BPR-2244:row`: a row shows the age to the host's clock, its accessible
  name and tooltip carry the age and exact date, and a later clock updates
  the words without resetting the model.
- `BPR-2244:look`: the owner judges the row's look in a frozen build
  (attended; pending).

## Evidence

Implementation `293d5d0b5` on the lane, 2026-11-17. Placement chosen with the
frontend-design skill: the age sits at the right of the row's detail line,
so ages read down the list as a column; the short ID, author and file count
stop short of it, and a part that no longer fits ends in an ellipsis.

- `BPR-2244:read`: met. `application_github_contract` (a recording keeps
  the committer's date as seconds; "last Tuesday" leaves it unknown; a
  committer with any key besides `date` is refused) and `github_paged_read`
  (the date is kept; a numeric date leaves it unknown and the read passes).
- `BPR-2244:row`: met. `application_commit_filter_model` (rows carry the
  date) and `qt_commit_filter_view_scale_100/200` (`test_commit_age`: "2 days
  ago" and "1 day ago" to the clock, none undated or without a clock; the
  accessible name says "committed 2 days ago"; the tooltip says
  "Committed <d MMM yyyy>, <time>"; a later clock changes only rows whose
  words change, with no reset; a clock behind GitHub says "just now"; the tab
  measures on present and on show).
- Also passed: `qt_drawer_surfaces_fit_scale_100/200`,
  `qt_composed_github_scale_100`, `application_commit_filter_public_path`,
  `architecture_boundaries`, `github_provider`, `core_commit_filter`,
  `commit_filter_fixture_contract`, `qt_refresh_feedback_window`; the full
  dev build; format and tidy on the changed files; the documentation gate.
  `github_contract_fixture_security` fails on lane HEAD without this change:
  its expected tree omits the tracked `flag-move-v1` and
  `returning-review-v1` fixtures.
- `BPR-2244:look`: pending. No committed fixture's commits carry a committer
  date yet, so a frozen build would show no ages. The look first needs
  `scripts/build_commit_filter_fixture.py` to give commit-filter-v1's
  commits dates and `github_contract_fixture_security`'s `with-parents`
  response fields to admit `commit.committer.date`. Then freeze on that
  fixture and ask Accept or Reject on the row.
