---
schema: "implement-rfc.card.v1"
id: "BPR-1289"
title: "Add a Settings search and filter box"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner aside in attended session 8bf444584f9806487f06891db2f5af50 (BPR-1083 mark-style-v001, decision b504ffed, 2026-10-06T00:16Z)","reservation":"docs/issues/_reservations/BPR-1287--BPR-1289--621f5b91f3f1.json","related_cards":["BPR-1083","BPR-1288","BPR-0981"],"owner_reference":"docs/design/owner-refs/BPR-1083-owner-ref-codex-settings-search.png","port_candidates":[{"name":"containers gct","path":"/Users/jaeger/Documents/Personal/containers/source/gct/strings/fuzzy_match.hpp","revision":"a7454c4","licence":"none visible; private repository, every commit by the owner"},{"name":"Entole","path":"/Users/jaeger/Documents/Personal/aito/code/entole_developer_tools/modules/completion/src/Completion.cpp","revision":"962b48774","licence":"none visible; private repository, every commit by the owner"},{"name":"AllTheThings","path":"/Users/jaeger/Documents/GitHub/AllTheThings/Sources/ATTCore/FuzzyMatcher.swift","revision":"f755aeb","licence":"MIT, Copyright (c) 2026 Michael Marcin"},{"name":"FindInMedia (legacy Rust)","path":"/Users/jaeger/Documents/GitHub/FindInMedia/crates/fim-db/src/lib.rs","lines":"6236-6271","revision":"f1943cb","licence":"MIT (workspace Cargo.toml; no LICENSE file), every commit by the owner"}],"acceptance":["BPR-1289:search","BPR-1289:results","BPR-1289:go-to","BPR-1289:matcher","BPR-1289:provenance","BPR-1289:owner-look"]}
---

# BPR-1289: Add a Settings search and filter box

## Origin

Accepting `BPR-1083-mark-style-v001`, the owner added, quoted exactly:

> Aside: I really want one of those full settings search/filter boxes like
> this codex screenshot. I think we had some fuzzy text filtering code in
> SiftThroughLogs we might be able to port over.

Correcting that later the same day, relayed by the coordinator, verbatim:

> The fuzzy search I was remembering was not in SiftThroughLogs it was in
> these 3

naming the three sources listed under "Port candidates" below, and then,
verbatim:

> And also FindInMedia (legacy Rust):
> /Users/jaeger/Documents/GitHub/FindInMedia/crates/fim-db/src/lib.rs:6236 —
> is_caption_spelling_variant; levenshtein_distance at line 6257.

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
- **BPR-1289:matcher.** Matching is a pure core matcher in `src/core`
  (deterministic, no Qt, no clock unless passed in), ported from the best of
  the candidates below, case- and diacritic-insensitive, with a tested
  ranking (exact, then prefix, then word-start and consecutive letters before
  scattered ones). The Qt side builds its index from the settings the pages
  actually show, so a new setting is searchable without a second list;
  withdrawn pages (BPR-1179) never appear.
- **BPR-1289:provenance.** Ported code names its source repository, path,
  revision and licence beside it, and the third-party or provenance notes
  record it.
- **BPR-1289:owner-look.** The owner accepts it in an attended session.

## Port candidates (owner, 2026-10-05)

All four are the owner's own code. The owner's permission, in chat on
2026-10-05: "theyre all mine, as far as your concerned any port just gets
licensed to the BPR license". A port from any of the four is licensed under
BetterPullReview's licence; cite the source path and revision in the port's
provenance note, and no third-party notice is needed. Read on 2026-10-05:

- **containers, `gct::strings::fuzzy_matcher`.**
  `/Users/jaeger/Documents/Personal/containers/source/gct/strings/fuzzy_match.hpp`
  (2,068 lines, header-only C++, private repository
  `code.mikemarcin.com:containers`, `a7454c4`; every commit by Michael Marcin,
  2026-01-17 to 2026-02-10; tests in `test/strings/test_fuzzy_match.cpp`).
  Tiered: exact 200,000, whole-query prefix 160,000, segment prefix 140,000;
  otherwise a subsequence score of 200 per matched character, +300 at a soft
  boundary (`._-/ ` or a camel hump), +500 at a segment start, +180 for each
  adjacent match, −15 per skipped character and −2 per character of late
  start; queries split into terms and segments with optional `..`/`...`
  wildcards; a typo rescue by Damerau–Levenshtein distance (unrestricted
  transpositions, one edit for terms up to four characters, two beyond,
  −2,500 per edit); an optional frequency and recency boost; reusable
  workspaces and source masks. It includes `DirectXMath.h` and `gct` core
  headers, which a port drops. The most complete of the three and already
  C++: the likely base.
- **Entole completion.**
  `/Users/jaeger/Documents/Personal/aito/code/entole_developer_tools/modules/completion/src/Completion.cpp`
  (`score_candidate`, about lines 342 to 419; private repository `aito`,
  `962b48774`; every commit by Michael Marcin). The same tiers and history
  weights in a smaller form: exact 200,000, prefix 160,000, greedy
  left-to-right subsequence 100,000 + 200 per character − 15 per gap
  character − 2 per character of late start, else an edit distance of at most
  two at 50,000 − 5,000 per edit; matched ranges for highlighting; ASCII
  case folding only.
- **AllTheThings `FuzzyMatcher`.**
  `/Users/jaeger/Documents/GitHub/AllTheThings/Sources/ATTCore/FuzzyMatcher.swift`
  (1,807 lines, Swift, `MikeMarcin/AllTheThings` at `f755aeb`, MIT,
  "Copyright (c) 2026 Michael Marcin"). File search: case-, diacritic- and
  width-folding normalisation; exact 10,000, prefix 8,000, substring 6,000,
  then a greedy subsequence at 5,500 with +450 at the start, +70 per character
  of the longest run, −18 per gap character and −8 per character of late
  start; a query language with negation, fields and wildcards, and path,
  extension and kind scoring, which Settings does not need. Worth taking its
  normalisation.
- **FindInMedia `is_caption_spelling_variant`** (legacy Rust).
  `/Users/jaeger/Documents/GitHub/FindInMedia/crates/fim-db/src/lib.rs`, lines
  6236 to 6271 (`MikeMarcin/FindInMedia` at `f1943cb`; `license = "MIT"` in the
  workspace `Cargo.toml`, no licence file; every commit by Michael Marcin). Not
  a ranker but a yes/no spelling-variant test for caption search: a candidate
  of 3 to 12 ASCII letters and digits with at least two consonants is a
  variant of the query when it is not identical, both have the same consonant
  signature (letters other than a, e, i, o, u and y, lowercased), and their
  plain Levenshtein distance (two-row dynamic programme, line 6257) is at most
  two. It fits typo tolerance, the role containers' Damerau–Levenshtein
  rescue plays, not subsequence ranking.

SiftThroughLogs (`55e7e0c`), which the owner first named, has no fuzzy
matcher: only SQLite FTS5 over log events and the query language's
`contains`/`starts_with`/`ends_with`. BetterPullReview's own
`resident_filter_matches` ([[BPR-0981]], RFC-BPR-0025) is a Qt-side,
unranked subsequence test; the drawers may later move to the new core matcher.

## Boundaries

Presentation and a Qt-side index only; no setting's storage changes. Lay the
field out in BPR-1288's design if that lands first.

## Linux implementation — 2026-10-06 (`bpr-main-linux`)

**Matcher (BPR-1289:matcher).** `bpr::core::fuzzy_match` and `fuzzy_rank`
(`src/core/{include/bpr/core,src}/fuzzy_match.*`) are pure standard C++, with
no Qt and no clock.

- **Folding:** case, Latin-1 and Latin Extended-A diacritics (ß → ss, æ → ae),
  full-width forms and combining marks, with white space collapsed.
- **Tiers:** exact 200,000; whole-query prefix 160,000 (shorter first);
  word-start prefix 140,000 (earlier, then shorter, first).
- **Letters:** otherwise every term must match as its best-scoring subsequence:
  200 per letter, +500 at a word start, +300 at punctuation or a camel hump,
  +180 per adjacent letter, −15 per skipped letter, −2 per letter of a late
  start.
- **Typos:** for a term of three or more letters with no subsequence, a
  Damerau-Levenshtein distance of at most one edit (two beyond four letters)
  to a word or word prefix that keeps the term's first letter, at −2,500 per
  edit.
- `fuzzy_rank` lists typo matches only when nothing matched directly, and
  equal scores keep their input order.
- `core_fuzzy_match` holds the ranking corpus. It covers the tiers, word-start
  and camel bonuses, folding, multi-term queries, the typo rescue and stable
  order.

The Mac sources were unreachable from this station, so this is an
implementation of the card's description, not a copied port.

**Provenance (BPR-1289:provenance).** The header names both sources and the
owner's licence grant. `docs/development/ported-code-provenance.md` records
the details.

**Search, results and go-to (BPR-1289:search, :results, :go-to).**
`settings_window` gains a "Search settings" field above the page list. It has
an accessible name, a clear button and `QKeySequence::Find` (Command-F or
Ctrl+F).

- **Esc:** clears the field; with the field empty, Escape still closes
  Settings.
- **Down and Return:** Down moves into the results; Return opens the current
  or first result.
- **Results:** they replace the page list, grouped by page in sidebar order
  and then by score, in a stable order. Each names the setting, then "Page ·
  Section". A matching page ("Settings page") or section is a result of its
  own.
- **No match:** the `filter_empty_notice` says so, with Clear.
- **Go-to:** choosing a result, by pointer or Return, shows its page, makes it
  visible with `ensureWidgetVisible` and focuses the control. It changes
  nothing; the test checks the chosen combo's index.
- **Index:** read from the pages as shown, on each query, so a new setting
  needs no second list and a withdrawn page (BPR-1179) is skipped.
  - Names come from form labels, buddies, appearance `…Row`/`…RowName`
    labels, button text or the accessible name.
  - Sections come from a titled group box, an appearance card or the last
    `…Heading` label.
  - A segmented choice ("Setting: option") is one result, focused on its
    chosen option.

**Refinements, decided here and open to the owner's look:**

- A section or a description counts only where the query starts one of its
  words. A description is a sentence, and scattered letters of almost any
  query fall into one: "notif" listed every Reset button.
- A setting is not listed merely because its page title matches. The page's
  own result stands for the page; otherwise "appear" listed every Appearance
  setting.

**Evidence** (`build/dev`, GCC 15.2, offscreen):

- `qt_settings_window` and its 125, 150 and 200 % rows pass 4/4, with the two
  new courses. One finds "Code font" as "Code font / Appearance · Type", goes
  to it by Return with focus and no change, and checks the page result, Down,
  Escape and Clear. The other covers the withdrawn page and a newly adopted
  setting.
- Control: with `refresh_search` doing nothing, `qt_settings_window` fails
  "results replace the page list while the field holds text".
- The full set passes 147/147: every `qt_` row, `core_fuzzy_match`, the core
  boundary rows, both architecture rows, settings, appearance and diagnostics.
  The `core-only` preset builds, and its three matcher and boundary rows pass.
- `coding-format-check` and `DOCUMENTATION_OK` pass.

**Open:** BPR-1289:owner-look (the integrator's attended review from the Mac),
and the macOS and Windows runs. The frontend-design skill the integrator named
is not installed on this station, so the field follows the existing drawer
filter fields' pattern.
