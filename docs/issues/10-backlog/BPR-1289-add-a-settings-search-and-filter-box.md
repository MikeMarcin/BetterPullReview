---
schema: "implement-rfc.card.v1"
id: "BPR-1289"
title: "Add a Settings search and filter box"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner aside in attended session 8bf444584f9806487f06891db2f5af50 (BPR-1083 mark-style-v001, decision b504ffed, 2026-10-06T00:16Z)","reservation":"docs/issues/_reservations/BPR-1287--BPR-1289--621f5b91f3f1.json","related_cards":["BPR-1083","BPR-1288","BPR-0981"],"owner_reference":"docs/design/owner-refs/BPR-1083-owner-ref-codex-settings-search.png","port_source":{"repository":"MikeMarcin/SiftThroughLogs","checkout":"/Users/jaeger/Documents/GitHub/SiftThroughLogs","revision":"55e7e0c","licence":"MIT, Copyright (c) 2026 Sift contributors","fuzzy_matcher_found":false},"acceptance":["BPR-1289:search","BPR-1289:results","BPR-1289:go-to","BPR-1289:matcher","BPR-1289:provenance","BPR-1289:owner-look"]}
---

# BPR-1289: Add a Settings search and filter box

## Origin

Accepting `BPR-1083-mark-style-v001`, the owner added, quoted exactly:

> Aside: I really want one of those full settings search/filter boxes like
> this codex screenshot. I think we had some fuzzy text filtering code in
> SiftThroughLogs we might be able to port over.

Their reference, [`BPR-1083-owner-ref-codex-settings-search.png`](../../design/owner-refs/BPR-1083-owner-ref-codex-settings-search.png),
is the Codex desktop Settings sidebar with `appear` typed in a search field
at the top (a magnifier and a clear button). The page list is replaced by
results, each a setting's name or matching line with an icon and, under it,
where it lives ("Mini & Pets", "Hooks", "Keyboard shortcuts", "Worktrees",
"Browser", "Computer use", "Configuration"); page names that match
("Appearance", "Connections") are results of their own. The image is cropped
to that sidebar: the original also showed the owner's local folder path and
initials, which are left out.

## Outcome

A search field at the top of the Settings sidebar. Typing filters every
setting on every page by its name, its section, its page and its description,
and lists the matches grouped by page and section with the setting's name
first. Choosing a result opens its page and brings the setting into view with
focus. Clearing the field restores the page list.

## Acceptance

- **BPR-1289:search.** The field is reachable by keyboard and by ⌘F in
  Settings, has an accessible name, a clear button and Esc to clear; an empty
  field shows the ordinary page list.
- **BPR-1289:results.** Results name the setting and, under it, its page and
  section, in a stable order; a page or section that matches is a result of
  its own; no match says so with Clear.
- **BPR-1289:go-to.** Choosing a result (pointer or Return) shows its page,
  scrolls the setting into view and focuses it, and changes no setting.
- **BPR-1289:matcher.** Matching is fuzzy and case-insensitive with a tested
  ranking (exact and prefix words before scattered letters); the index is
  built from the settings the pages actually show, so a new setting is
  searchable without a second list; withdrawn pages (BPR-1179) never appear.
- **BPR-1289:provenance.** Ported code names its source repository, path,
  revision and licence beside it, and the third-party or provenance notes
  record it.
- **BPR-1289:owner-look.** The owner accepts it in an attended session.

## Port source, as found on 2026-10-05

The owner's recollection is not confirmed. SiftThroughLogs
(`MikeMarcin/SiftThroughLogs`, checkout
`/Users/jaeger/Documents/GitHub/SiftThroughLogs`, `main` at `55e7e0c`, the
only branch; MIT, "Copyright (c) 2026 Sift contributors"; all 35 commits by
Michael Marcin) has no fuzzy text matcher. A search of every tracked source
and of the history for fuzzy, subsequence and fzf matching found none. The
nearest code is:

- `libraries/store/src/capture_store.cpp` (about line 1077): an SQLite FTS5
  table `event_fts` with the `unicode61 remove_diacritics 2` tokenizer, for
  full-text search over captured log events;
- `libraries/query/src/query.cpp` (about lines 1411 to 1421): the query
  language's `contains`, `starts_with` and `ends_with`, compiled to `LIKE`;
- `libraries/ui/src/facet_filter.cpp` and `libraries/ui/src/inspector_filter.cpp`:
  facet and field predicates for the query bar, not text matching.

BetterPullReview already has a fuzzy filter:
`src/adapters/qt/include/bpr/qt/resident_filter.hpp` and
`src/adapters/qt/src/resident_filter.cpp` (`resident_filter_matches`,
RFC-BPR-0025, [[BPR-0981]]), where every space-separated term must appear as
an ordered, case-folded subsequence. It ranks nothing. Ask the owner first
whether another SiftThroughLogs source (an unpushed branch or another
checkout) was meant; if not, build on `resident_filter` and add the ranking
here.

## Boundaries

Presentation and a Qt-side index only; no setting's storage changes. Lay the
field out in BPR-1288's design if that lands first.
