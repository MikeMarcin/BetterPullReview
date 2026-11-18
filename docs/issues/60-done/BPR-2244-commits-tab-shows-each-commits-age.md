---
schema: "implement-rfc.card.v1"
id: "BPR-2244"
title: "The Commits tab shows how old each commit is"
status: "Done"
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
- `BPR-2244:look`: pending, at the owner. Prepared 2026-11-17:
  - `1619c2c51`: `scripts/build_commit_filter_fixture.py` dates every
    variant's commits in the served commit list (`commit.committer.date`,
    from 2026-11-03 to 2026-11-17 07:42Z, so the rows read weeks, days, a
    day and hours); the packs and commit IDs are unchanged.
    `build_recorded_pull_request_contract.py` serves the date when a
    construction gives one. `github_contract_fixture_security` admits it in
    the `with-parents` shape and now lists `commit-selection-v1`,
    `flag-move-v1` and `returning-review-v1`; it passes (18 tests), as do
    `commit_filter_fixture_contract`, `github_recorded_fixture_contract`,
    `application_commit_filter_public_path`,
    `application_commit_asks_public_path`, `application_commit_filter_model`,
    `qt_commit_filter_view_scale_100`, `github_provider` and
    `application_github_contract`.
  - Frozen with `scripts/freeze_attended_build.py freeze` on
    `commit-selection-v1` (four commits, a merge among them) opening
    `src/app.cpp`, Fixture agent: root `/Users/jaeger/commits-age-v001.ek943_0s`,
    frozen from `9a94946b5` (HEAD; `--not-head-because` only because the
    shared worktree held another implementer's uncommitted files), executable
    SHA-256 `3e9f1a80e486…`. Its own check passed (file shown, nine Fixture
    agent asks answered, no real agent started).
  - Attended session `c2dbde022242d5a915700ae619d56553`, state
    `/Users/jaeger/.mask/attended-review/owner-review-20261117-bpr2244-commits-age-v001`,
    one Accept/Reject question `commits-age-v001` on the row's age and its
    tooltip; ready, served. The Now images are the view test's offscreen
    renders (`qt_commit_filter_view`, `BPR_RFC0089_RENDER_DIR`).

## Owner decision (2026-11-17)

The owner accepted look commits-age-v001 (session c2dbde02…).
