---
id: "RFC-BPR-0086"
title: "Insights: what BetterPullReview costs, and what you've reviewed"
status: "Auto-Verified"
area: "BPR"
depends_on: []
metadata: {"rfc_base_sha":"0378ab977a7ffc74e0ac5ac2f70ee37a9cf00a66","mandate_revision":"insights-window-v1-r3","pack":"docs/rfcs/assets/insights-window-v1/README.md","milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"},"implementation_support_paths":["scripts/verify_test_network_isolation.py"]}
---

# Insights: what BetterPullReview costs, and what you've reviewed

Adopted for implementation by the owner on 2026-10-18. Immutable RFC_BASE_SHA: `0378ab977a7ffc74e0ac5ac2f70ee37a9cf00a66`. The [design record](assets/insights-window-v1/DECISIONS.md) holds the answers; the [live import](assets/insights-window-v1.LIVE_IMPORT.md) records the mapping.

## Owner amendment r2: official Qt and supported child counters

On 2026-10-18 the owner directed:

> Stop the patched-Qt approach. I don’t want BetterPullReview to build or maintain a Qt fork; keep the official pinned Qt binaries. Revert only the fork-specific changes, preserving the other Insights work and unrelated edits. Amend the RFC and cards so exact child CPU is deferred where the supported Qt/platform APIs don’t provide it. Show “Not reported” there—don’t substitute estimates or process-wide deltas. Continue the remaining Insights implementation and verification under that amended scope. Record the limitation clearly; it should no longer block this pack’s closeout.

This is mandate `insights-window-v1-r2`. It supersedes the unconditional
exact-child-CPU obligation in Energy and AC08; all other acceptance remains.
The existing official pinned Qt binary provider stays authoritative. Exact
child CPU is recorded only where supported Qt/platform APIs provide the
specific child’s final counters. Unsupported Unix gh/git and supervised-Git
counters remain absent and show “Not reported”, including in mixed measured /
unmeasured totals. No launch wrapper, Qt fork, process-wide delta or sampling
estimate fills that gap. Ordinary Windows children retain the existing exact
handle implementation; native Windows/Linux qualification is still unmeasured.
This documented capability limitation does not block Energy or pack closeout.
Historical r1 failures and evidence retain their original meaning.

## Owner amendment r3: defer the idle hour until release qualification

On 2026-10-18 the owner directed:

> dont run the full 3600 seconds test, save long tests for a later release, this is a bypass allowance

This is mandate `insights-window-v1-r3`. Amendment r2 remains in force.
AC12's full macOS signed-in, no-window background hour is bypassed for this
pack closeout and deferred to a later release. No hour was started or measured;
no CPU or wakeup budget pass is claimed. The original course and its exact
2 CPU seconds per background hour / 6 wakeups per minute budgets remain a
[release qualification obligation](../verification/RFC-BPR-0086.md#release-deferred-ac12-idle-hour).
A later measured miss still requires a card naming the responsible operation.
This allowance adds no new human gate and does not waive AC01–AC11.

## Problem and grounded current behavior

The owner, 2026-10-17, after looking at AllTheThings' Insights window: "I
wonder what a BetterPullReview version of that would look like? What useful
information we could show." Then: "So it shows cost, but it also shows some
fun stats. We could track total comments, notes, files accepted/seen. Reviews
completed/loaded."

AllTheThings (`~/Documents/GitHub/AllTheThings`, 0.14.6) has a fixed
900 × 640 Insights window with Summary, Index, Activity and Energy tabs
(`Sources/AllTheThings/InsightsWindowController.swift:8-44`). Its Energy
chart (`InsightsEnergyChartView.swift`) is the quality bar the owner named:
"I think the ATT graphs are a bit better than your mockups here."

BetterPullReview has no equivalent. What exists today:

- **Diagnostics entry.** Help › Diagnostics… in a window opens Settings on
  Storage & privacy (`src/adapters/qt/src/workbench_window.cpp:16196`). The
  resident menu bar's Diagnostics item is created disabled because no window
  owns it (`src/adapters/qt/src/desktop_menu_bar.cpp:247-264`).
- **Rate limits.** `github_rate_limit_state` keeps the latest Core, Search
  and GraphQL `Limit`, `Remaining`, `ResetUnixSeconds` and `SecondaryLimited`
  (`src/application/include/bpr/application/github_read_model.hpp:509-535`).
  Paged reads observe them on every page
  (`src/adapters/github/src/github_paged_read.cpp:886`); the `gh` transport
  keeps the `x-ratelimit-*` headers from `gh api --include`
  (`src/adapters/github/src/gh_cli_transport.cpp:143`). The Accounts page
  shows the current values (`src/adapters/qt/src/server_card.cpp:471-579`).
  Nothing counts BetterPullReview's own requests, keeps a history of
  readings, or reads `X-RateLimit-Used`. There is no ETag or 304 handling.
- **Storage.** Stores live under `state/v1` of the profile root
  (`src/adapters/sqlite/src/state_directory.hpp:61`; names at
  `src/adapters/sqlite/src/release_maintenance_files.cpp:76-92`). Only the Git
  cache sums its bytes, inside the adapter
  (`src/adapters/git/src/installed_git_cache.cpp:1490`). Budgets: Markdown
  images 128 MiB LRU, Review Cycles 768 MiB, logs 5 MiB segments and 50 MiB
  in all (`docs/architecture/system-architecture.md`, diagnostic logging).
- **Review facts.** The ledger keeps `file_receipt`, `conversation_receipt`,
  `local_note` and `local_defect` rows
  (`src/adapters/sqlite/src/review_ledger_database.cpp:167-222`). Successful
  writes arrive as `review_submission_receipt` with a `review_disposition`
  of comment, approve or request changes, `review_mutation_receipt`, and
  `review_thread_resolution_receipt`
  (`src/application/include/bpr/application/review_submission_contract.hpp:40`,
  `review_mutation_port.hpp:252-320`). These rows are live state: pruning or
  deleting a cache or a profile reset removes them, so they cannot hold
  all-time totals.
- **Work.** `computed_file_diff` reports `MoveDetectionWorkLimitReached`,
  `MoveDetectionSegmentLimitReached` and `AlignmentApproximate`
  (`src/core/include/bpr/core/diff_compute.hpp:104-113`); the syntax host
  counts cache hits and evictions; source search reports acquisition and
  matching microseconds. No product code measures input or UI latency. The
  100 ms input target exists only in the BPR-0967 benchmark
  (`docs/verification/BPR-0967-station-budgets-2026-10-01.md:64`).
- **Energy.** No CPU, wakeup or child-process sampling exists. The app keeps
  running with no window (`QApplication::setQuitOnLastWindowClosed(false)`,
  `src/app/main.cpp:8719`) and launches `gh` and `git` children.
- **Failures.** `diagnostic_event` carries a `diagnostic_outcome` that
  includes `failed`
  (`src/application/include/bpr/application/diagnostic_log_port.hpp:65-130`);
  nothing tallies them.

## Proposed behavior and boundaries

### The window

Insights is a per-profile window, one instance per profile, created and
owned like Settings (`github_profile_runtime`). It opens from
**BetterPullReview › Insights…** in the macOS application menu and
**Help › Insights…** on Windows and Linux. The shortcut is ⌘⌥⇧I on macOS and
Ctrl+Alt+Shift+I elsewhere, as in AllTheThings. Help › Diagnostics… is
unchanged (Q1). The title bar carries six tabs: **Summary, Reviews, GitHub,
Storage, Work, Energy**. It has two header actions: **Reveal data folder**,
and **Export anonymized diagnostics…**. The export action opens the existing
frozen-preview export flow, which gains an Insights section (see Privacy).

Owner clarification, 2026-10-18, rejecting the first GitHub chart candidate:
the tabs belong in the actual header title-bar row, as in AllTheThings, to
avoid spending a separate row. The ATT Energy screenshot and source remain
the chart reference. Its event bars, framed lane proportions and taller graph
guide this correction; quantity series retain the area stack, maxima and gap
rules below. Native title-bar placement and chart quality receive separate
Accept/Reject questions in the replacement attended session.

**Fixed size.** The owner: "We had explicit rules in ATT to make sure all
tabs were the same window size so we didn't get that disconcerting resizing
motion."

- The window has one content size, fixed for every tab, every range and
  every refresh, and it isn't user-resizable. AllTheThings uses 900 × 640;
  implementation picks the exact size and records it.
- On a smaller screen, the size is clamped to the available geometry minus
  an 18 px margin. The window opens centred on the screen of the window that
  opened it.
- Every page is laid out to fit that size. Only a bounded table inside a page
  may scroll; the page itself never scrolls.
- The header (title and status line) never absorbs or gives up page slack.
- Escape closes the window.

**Status line.** It reads "Updated *n* sec. ago". The time is when the
snapshot was last built.

**Refresh.**
- While the window is visible, its snapshot refreshes every 5 s when the
  application is active and every 30 s otherwise. Refresh stops when the
  window is hidden or closed.
- A refresh reads only local state. It never issues a GitHub request, a Git
  operation or a process launch.
- The snapshot is built off the UI thread. A refresh is skipped while one is
  in flight.

**Empty states.** Every tile, table and chart with no data yet shows a
one-line empty state that names what will fill it, for example "No energy
samples yet". It never shows a zero that looks like a measurement.

### Charts

Charts match AllTheThings' Energy chart rather than the chat mockups. The
following are required:

- **Lanes.** Rate-based series that share no unit get separate lanes: a short
  lane above for events per minute, a main lane below for the quantity.
  Each lane has its own scale, and the legend sits below.
- **Scale.**
  - A percentage-of-capacity lane is scaled to 100% until it is exceeded.
  - Other lanes autoscale to their peak.
  - Every lane labels its maximum value. AllTheThings has no axis labels;
    this is a deliberate improvement.
- **Lines and fills.** Area fills with a line on top. Foreground and
  background share one stack: background from the baseline, foreground
  above it, and the total is the stack's top edge.
- **Gaps.** Time with no samples (the application wasn't running, or the
  profile was closed) is drawn as a gap in every range.
  - This deliberately differs from AllTheThings, whose 1d view draws
    straight across gaps.
- **Hover.** Hovering shows a marker on the line and a card next to it. The
  card has a title row and label/value rows, flips at the window edges, and
  keeps 8 px from them.
  - In the 1h range, the hovered sample changes only after the pointer
    crosses the midpoint to the next sample.
  - Switching range clears the hover.
- **Calendar.** Long ranges use a calendar grid: weeks across, Monday first,
  with month labels.
  - Dot size encodes the amount: the square root of the value divided by
    the range's 90th percentile, with a minimum of 0.16.
  - Colour encodes the quality being judged: background intensity for
    Energy, review activity for Reviews.
- **Theme.** Colours come from the current appearance profile's roles, not
  hard-coded values. Charts redraw on a Light/Dark or theme change.
- **Accessibility.** Each chart exposes an accessible name and a one-sentence
  description of what it shows over the selected range, for example
  "Background CPU stayed under 0.1% of the system for the last hour, with
  two peaks during inbox refreshes". AllTheThings has none.

### Summary

Summary leads with the **GitHub budget this hour** for the core REST
resource. It's a single bar in three parts:
- BetterPullReview's requests in the current reset window;
- other clients on the same account in that window;
- what's left.

It also shows the reset time.

**Other clients** is derived, never fetched:
- Take the lowest `Remaining` reported by any BetterPullReview response
  carrying the current window's `ResetUnixSeconds`.
- "Used in window" is `Limit` minus that lowest `Remaining`.
- "Other clients" is "used in window" minus BetterPullReview's own core
  requests counted against the same window, clamped at zero.
- BetterPullReview's count for a window is persisted, so a restart within
  the window doesn't move its earlier requests into "other clients".
- With no reading in the current window, the bar says so. It does not fetch
  one: no request is ever issued only for Insights, `GET /rate_limit`
  included.
- The value is shown as approximate ("about 1,240"), because requests that
  carry no rate-limit headers are still counted as BetterPullReview's.

Below the bar, six tiles:
- **Reviews submitted** and **Comments posted**, for 30 days, from Reviews.
- **Cost while idle**: background CPU seconds per background hour, from
  Energy.
- **Local data**: total bytes, with the Git cache share.
- **Background work**: Idle, or the operation currently running, and the
  next scheduled refresh.
- **Problems, 24h**: the count of `failed` diagnostic outcomes, broken down
  by operation category.
  - Problems are tallied whether or not diagnostic recording is On, because
    a tally holds no identity.

### Reviews

The owner asked for "total comments, notes, files accepted/seen. Reviews
completed/loaded." Reviews shows what this profile did, in three ranges:
**This week** (since Monday, local time), **30 days** and **All time**.
Counting starts the first time a build with Insights runs. Nothing is
backfilled from existing ledger rows (Q2). Header copy: "Counted on this
computer since <date>".

**Counts.** Each is counted once, when the event happens, and never goes
down:

| Count | Counted when |
|---|---|
| Reviews opened | A pull request is opened in a workbench; once per pull request per local day |
| Reviews submitted, by approve / request changes / comment | A `review_submission_result` reports `published`. An `indeterminate` result counts only once recovery reports it published |
| Comments posted | A `review_mutation_receipt` with `Duplicate == false` for a new thread |
| Replies posted | The same receipt for a proposal with `ReplyToThread` |
| Suggestions posted | A posted comment or reply whose body contains a GitHub suggestion block |
| Threads resolved | A `review_thread_resolution_receipt` with `Resolved == true`; unresolving doesn't subtract |
| Files seen, files accepted | The first time a file reaches that state for a given pull request, path and head revision |
| Lines read | The changed lines (additions plus deletions) of each accepted file, at acceptance |
| Private notes, defects | A `local_note` or `local_defect` is created |
| Review Cycle rounds | A round is frozen |

**Calendar.** Reviews submitted per day for the last 26 weeks, using the
calendar chart.

There is no streak. The owner cut it because holidays and days off make
any rule wrong (Q4).

**How reviews ended.** Approved, changes requested, commented, threads
resolved and Review Cycle rounds, for the selected range.

**Records**, all time:
- biggest pull request reviewed, by files changed, among pull requests with
  a submitted review;
- most reviews submitted on one pull request;
- busiest day, by reviews submitted;
- lines read in all;
- most reviewed repository.

**Not tracked.** Time spent per review, active time, and anything about
other people's activity. This is settled (D6).

### GitHub

- **Budget tiles.** One for each of Core REST, GraphQL and Search: remaining
  of limit, reset time, and BetterPullReview's requests in the window.
  - The other-clients split appears only for Core REST. GraphQL charges by
    query cost, which BetterPullReview doesn't read, so GraphQL shows a
    request count and no split.
- **Chart.** Remaining Core budget over 1h and 1d, with BetterPullReview's
  requests per minute in the events lane. A reset is drawn as a vertical
  rule.
- **Where requests went.** BetterPullReview's requests in the selected
  range, by a closed set of operation categories: inbox and review queue,
  opening pull requests, conversations, publishing, and other.
  - Categories come from the existing operation identity, not from URLs.
- **`gh` processes.** In the selected range: launched, then failures by
  `github_cli_process_failure` kind (timeout, cancelled, output limit,
  crash, nonzero exit, reap failure). Also quarantined lanes, abandoned
  children, and the slowest call.
- **Secondary limits.** Secondary-limit and `Retry-After` events are listed
  with their time.

### Storage

Storage shows bytes on disk for each of these, against its budget where one
exists:
- the Git cache in all, and by repository;
- Review Cycles (768 MiB);
- Markdown images (128 MiB);
- diagnostic logs (50 MiB);
- every `state/v1` SQLite store, including its `-wal` and `-shm` files;
- the Insights store itself.

The total is the Summary tile.

**Measurement cost.**
- Sizes are measured off the UI thread, at most once per 60 s, and only
  while Summary or Storage is visible.
- Git cache sizes come from the cache adapter's existing inventory, not a
  second directory walk.

**Actions.** Reveal data folder, and **Manage cached repositories…**, which
opens the existing epoch and cache controls. Insights adds no deletion path
of its own. This is the same rule BPR-1602 follows ("No second deletion
path").

### Work

Work shows whether the application is keeping up:

- **Main-thread stalls.**
  - A stall is time the UI thread's event loop was blocked for more than
    100 ms, the BPR-0967 input target.
  - Shown for the selected range: the count, the longest stall, and the
    95th-percentile event-loop latency against the 100 ms target.
  - This is the runtime proxy for input response. It is measured in the Qt
    adapter, where a clock is allowed.
- **Diff.**
  - Files diffed, and the wall time per file, measured in the application
    host around the existing computation.
  - How many files hit `MoveDetectionWorkLimitReached` or
    `MoveDetectionSegmentLimitReached`, and how many were
    `AlignmentApproximate`.
  - The five slowest files in the range, with their paths.
- **Syntax and search.** Syntax cache hit rate and evictions, and the
  median source-search time (acquisition plus matching).

### Energy

The owner: "I think energy shouldn't be as important here as it is in ATT
but if we're going to have it we should keep its foreground and background
split. I probably care more that it's very cheap when I'm not using it than
what it's doing while I'm engaged with it."

**Foreground and background.**
- Foreground means the application is active (`Qt::ApplicationActive`).
- Background is everything else, including running with no window in the
  menu bar.
- On each transition, a sample is recorded and attributed to the old state
  before switching, so the interval is split at the transition.

**Sampling.**
- Intervals: every 30 s in the foreground, every 60 s in the background.
- Measured: process CPU time and, where the platform reports them, wakeups.
- macOS reports wakeups through `proc_pid_rusage` package-idle plus
  interrupt wakeups. Where wakeups aren't available, the window says "Not
  reported on this platform" rather than showing zero.
- Deltas with non-positive duration, negative CPU or decreasing wakeups are
  dropped.
- The sampler runs whether or not Insights is open. It is the only
  always-on cost Insights adds, and its own wakeups count against the
  budget.

**Child processes.**
- Where supported Qt/platform APIs provide exact final counters, the CPU time
  of each `gh` and `git` child is recorded at reap, attributed to the state
  active at launch, and kept apart from the application's own CPU.
- Where those APIs do not provide exact child CPU, retain an absent counter
  and show “Not reported”. Measured zero and no children stay distinct.
  Do not estimate, use process-wide deltas, or build/maintain a Qt fork.
  This capability limitation is deferred under owner amendment r2 and does
  not block this pack's closeout.
- Sampling is a platform adapter behind an application port. The core never
  reads a clock or a process.

**Layout.**
- **Tiles lead with background:** background CPU, background wakeups
  against the budget, and background child-process CPU (`gh`, `git`).
- **Chart and table.** A chart and a paired fact table, with ranges 1h, 1d
  and 3m.
  - The chart stacks background under foreground. The events lane carries
    wakeups.
  - The 3m range uses the calendar, coloured by background intensity.
  - Each fact-table row pairs foreground with background: CPU, wakeups,
    child CPU, and samples.
- **Cost per idle hour.** Background CPU divided by background wall time.
  This is the figure judged against the budget.
- **Idle budget (Q3).**
  - Background CPU: at most 2 s per background hour.
  - Background wakeups: at most 6 per minute, where wakeups are reported.
- **Budget display.** The budget is drawn as a reference line in the
  background lane. A range over budget is marked in the tile.

**Retention**, the same as AllTheThings:
- raw samples for 2 hours, used by 1h;
- 5-minute rollups for 48 hours, used by 1d;
- daily buckets for 93 days, used by 3m.

Each bucket keeps samples, wall time, CPU time, wakeups and peak load, split
by foreground and background.

### Data, privacy and lifetime

- **Store.** Insights keeps one per-profile SQLite store, `insights.sqlite3`
  in `state/v1`, with two kinds of data:
  - **History:** rate-limit readings and request counts, energy samples,
    work timings, problem tallies. History follows the retention above:
    GitHub and Work history 48 hours at 5 minutes and 93 days daily.
  - **Review stats:** lifetime totals, records, and one bucket per local day
    and repository. Review stats are kept until the profile is purged.
- **Purge and resets.**
  - The store is classified as a profile state store, so profile purge
    removes it.
  - Clear local diagnostics doesn't touch it, and neither do cache prune or
    cache delete.
  - BPR-1602's "reset everything" removes it; its narrower resets don't.
  - A separate "Reset review stats" action is deferred (D9).
- **Identity.**
  - Repository names, pull request numbers and file paths appear only in
    the window, on this computer.
  - The anonymized export's Insights section carries counts, durations,
    byte sizes and budget figures only. It has no repository, account, pull
    request, path or body.
  - The identifying export adds nothing from Insights.
  - Both follow `release_diagnostic_field` admission.
- **Clock.** Local day boundaries and time zones are supplied by the host.
  The core rolls up timestamps it is given.

### Architecture

The core decides, the host performs, Qt presents.

- **Core**, pure: rollup and retention policy, the budget-split derivation,
  review-stats counting rules (dedup keys, records), and an
  `insights_snapshot` value for each tab.
- **Application**, an Insights host:
  - observes the existing projections and terminals named above;
  - owns the sampling schedule;
  - persists through an Insights store port;
  - builds snapshots on request off the UI thread.
- **Adapters:**
  - the SQLite Insights store;
  - the process-metrics sampler (process CPU, wakeups, and child CPU at
    reap). The child-CPU part belongs with the shared process supervision,
    which already reaps children.
- **Qt:** the window, tiles, fact tables and charts. Presentation only.
- **App:** one Insights window per profile, created on first open like
  Settings.

### Answered questions

The owner answered all four on 2026-10-18. The answers are written into the
sections above; the [design record](assets/insights-window-v1/DECISIONS.md) quotes them.

- **Q1 Where Insights opens:** the recommendation (see "The window").
- **Q2 Counting from day one:** from the first launch with Insights, with no
  backfill. Owner: "app isnt distributed to anyone but me yet so its no big
  deal".
- **Q3 Idle budget:** the recommendation (see "Energy").
- **Q4 Streaks:** cut. Owner: "too hard to get it right with holidays and
  whatnot".

## Acceptance criteria

- **AC01 Window.**
  - The window opens from its menu entry and shortcut, once per profile.
  - Switching through every tab and range, and letting 3 refreshes pass, never
    changes the window's frame or content size. An automated test enforces
    this.
  - A screen smaller than the size clamps it.
  - With the window hidden, no refresh runs.
  - A refresh issues no GitHub request, Git operation or process launch.
  - Escape closes it.
- **AC02 Charts.** The Charts behaviour holds, checked on a rendered window:
  lanes and maximum labels, the stacked foreground and background, gaps,
  hover hysteresis and edge flipping, the calendar size and colour rules, the
  theme roles, and an accessible description. The owner judges the rendered
  charts against AllTheThings' Energy chart in an attended review, and
  accepts or rejects them.
- **AC03 Summary budget.** With fixture responses from two clients in one reset
  window, the split matches the Summary definition. Each of these shows its
  stated state rather than a number, and no request is issued for it:
  - a restart inside the window;
  - a reading crossing a reset;
  - no reading in the window.
- **AC04 GitHub tab.**
  - Request counts by resource and category match the requests a fixture
    run issued.
  - `gh` failure kinds, quarantines and abandoned children appear as they
    occur.
  - GraphQL shows no split.
- **AC05 Storage.**
  - Every store in `state/v1`, the Git cache by repository, logs, Markdown
    images, Review Cycles and the Insights store report bytes on disk within
    1% of a filesystem walk of the same paths.
  - Measuring runs only while Summary or Storage is visible.
  - Manage cached repositories opens the existing controls.
- **AC06 Review stats.**
  - Each Reviews count changes exactly once for its event, and never for a
    duplicate receipt or a retry.
  - An `indeterminate` submission counts only after it is recovered as
    published.
  - Totals survive cache prune, cache delete, Clear local diagnostics and a
    restart.
  - Profile purge removes them.
- **AC07 Reviews tab.**
  - The three ranges, calendar, "How reviews ended" and records match a
    seeded store.
  - Day buckets follow the local time zone, including across a
    daylight-saving change.
  - No streak is shown.
- **AC08 Energy sampling.**
  - A forced foreground/background switch splits the interval at the
    transition.
  - Exact child CPU available through supported Qt/platform APIs from a `gh`
    launched in the background counts as background even if it finishes in
    the foreground. Unsupported child CPU shows “Not reported”, with no
    estimates or process-wide deltas. Keep official pinned Qt binaries;
    this documented limitation does not block pack closeout (amendment r2).
  - Invalid deltas are dropped.
  - A platform without wakeups shows "Not reported on this platform".
  - Retention matches the 2 h / 48 h / 93 d windows.
- **AC09 Energy tab.**
  - The tiles show background figures only.
  - The fact table pairs foreground and background on every row.
  - Cost per idle hour is judged against the budget line.
  - A seeded over-budget range is marked.
- **AC10 Work tab.**
  - A seeded 250 ms main-thread block appears as one stall with that
    duration.
  - A diff fixture that hits each work limit is counted under it.
  - The five slowest files and the syntax and search figures match their
    host statistics.
- **AC11 Privacy and export.**
  - The anonymized export preview's Insights section contains no
    repository, account, pull request, path or body text. Checked against a
    fixture profile whose names are unique strings.
  - The identifying export adds nothing from Insights.
- **AC12 Idle cost.**
  - **Release-deferred by owner amendment r3:** the following full-hour course
    is bypassed for pack closeout; it remains required for later release qualification.
  - On macOS, with one signed-in profile and no window, one hour in the
    background stays within the idle budget for background CPU and wakeups.
    The run is recorded with the build and the measured figures.
  - A miss is filed as a card naming the operation that caused it.
