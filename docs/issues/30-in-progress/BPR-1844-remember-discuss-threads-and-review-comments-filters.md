---
schema: "implement-rfc.card.v1"
id: "BPR-1844"
title: "Discuss, Threads and Review Comments open with the filters last chosen, across pull requests and launches"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-03: persistent Discuss, Threads and Review Comments filters across PRs and launches, windows and tabs still free to diverge","reservation":"docs/issues/_reservations/BPR-1842--BPR-1844--749b2d50ac34.json","related_cards":["BPR-0661","BPR-1356","BPR-1808"],"frontend_design":"not loaded: no layout, wording or colour changes; only which filters a view opens with"}
acceptance: []
---

# BPR-1844: Discuss, Threads and Review Comments open with the filters last chosen

## Request

The owner, 2026-11-03: "I want my Discuss and Threads view and Review
Comments view filter settings to be persistent across PRs and launches
(although still allow windows/tabs to diverge active filters)."

## Interpretation

- **What is remembered.** Only new and Hide resolved, separately for each of
  three views: Discuss, Threads (the finder) and Review's Comments beside the
  code. A view's search, the Threads label filter, Discuss's scope and chosen
  file, and an Overview count's listing name things in one pull request and
  are not carried. Overview's Comments ([[BPR-1808]]) keeps its own filters
  as before.
- **Where.** One memory per profile, in its own
  `state/v1/view-filters.sqlite3`: one row with a version and a nullable
  value per view (NULL: never chosen there). It is a separate file so the
  workbench settings schema is untouched and losing it loses nothing else.
  Reset settings removes it; storage diagnostics name it.
- **When it is read.** When a window opens a pull request (a new window, a
  new pull request in a window, or a relaunch's restored one). A refresh of
  the same pull request (`PreserveWindowNavigation`) keeps the window's own.
- **When it is written.** On a reviewer's own press of Only new, Hide
  resolved, Clear filters (Discuss, Review's Comments) or Show all threads
  (Threads). A change the window makes itself (a reveal clearing a hiding
  filter, a count listing, Show new threads) is shown but not remembered.
- **Windows diverge.** The memory changes at once (last writer wins, per
  view), but is never pushed into a window that is already open. Each window
  keeps its own filters until it opens another pull request. Tabs share their
  window's filters as before; per-tab filter state is not added here.
- **The Settings first filter ([[BPR-0661]]).** Until Review's Comments has a
  remembered choice it opens with that preference. Choosing the preference
  in Settings is remembered as Review's Comments' choice, so the next pull
  request opens with it.
- **Damage and versions.** An unknown stored value leaves only its view
  unremembered (reported as recovered) and the next write repairs it; a
  version older than any known reads as nothing remembered and is rewritten;
  a newer build's row reads as nothing and is never overwritten; a file that
  is not a database reads as nothing.

## Layering

- Core, `core/view_filter_memory.hpp`: the window's filter state
  (`view_filter_state`), `open_view_filters`, `choose_view_filter`,
  `present_view_filter`, the per-view `remember` merge, and the stored row's
  encoding and reading.
- Application, `view_filter_memory_host`: one memory for the profile's
  windows; writes on the workbench effect lane, one at a time carrying the
  latest memory, flushing a waiting write at shutdown; reports durability.
- SQLite adapter, `view_filter_memory_store`: the file and its row.
- Qt: the conversation panel and the finder tell only `clicked` gestures; the
  window applies the opened state and forwards choices. The composition loads
  the memory once at profile start, before any window opens a pull request.

## Acceptance

- AC1: Core transitions: opening takes each view's memory or nothing (Review's
  Comments the first-filter preference); a choice changes only its view and
  is remembered; a window's own change is not; the merge is per view, last
  writer wins.
- AC2: Persistence: the memory round-trips through the SQLite store across a
  relaunch; damaged values, an obsolete version, a newer build's row and a
  foreign file degrade as described without overwriting the newer row.
- AC3: Host: one memory for every window, writes off the decision lane with
  at most one waiting, a waiting write flushed at shutdown, a failing store
  keeping the session's memory.
- AC4: Two composed workbench windows over one memory: choices in each are
  remembered and do not reach the other; a pull request opened afterwards
  takes the latest choice of every view, made in either window; the Settings
  first filter seeds Review's Comments and is remembered when chosen.
- AC5: Format, tidy and documentation gates pass.
