---
schema: "implement-rfc.card.v1"
id: "BPR-2351"
title: "Code anchored to a reviewed file is coloured with that file's names"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner question 2026-11-21, relayed by the coordinator","owner_note":"when we syntax highlight for a report finding are we using the context of where the comment is located to find names of symbols? Seems like we should.","governing_rfc":"RFC-BPR-0036","related_cards":["BPR-1562","BPR-1714","BPR-2277"],"branch":"claude/context-aware-code-highlighting","reservation":"docs/issues/_reservations/BPR-2351--BPR-2351--a5ed157fbeea.json","provisional_id":"BPR-XXXX-C","acceptance":["AC1","AC2","AC3","AC4"],"attended_review":{"session":"4708604404524750554d23a4282d7890","candidate":"report-code-file-names","decision":"accepted","date":"2026-11-21"}}
acceptance: []
---

# BPR-2351: Code anchored to a reviewed file is coloured with that file's names

## Owner note

The owner, 2026-11-21, verbatim:

> when we syntax highlight for a report finding are we using the context of
> where the comment is located to find names of symbols? Seems like we should.

## Before

Fenced code in rendered Markdown (report findings, threads, side chats) was
classified by `application::markdown_code_syntax_host` as a standalone
in-memory blob. For C++, `core::refine_cpp_identifiers` saw only the snippet
and the known-namespaces setting, so most names in a short excerpt, such as a
snake_case type the excerpt does not declare, had no type, namespace or
function role.

## Change

- **Core decides** (`core/code_context_roles.*`). `code_context_roles_of`
  reads a file's names and roles from the file view's refined classification
  (its bytes and spans). It reads namespace, type, function, enum member and
  macro. A type that is also called (a constructor) stays a type. Any other
  mix of roles, including a variable or constant, drops the name.
  `borrow_code_context_roles` repaints the block's own identifiers by name.
  Only an identifier the block's grammar left plain or gave a heuristic type,
  function or macro role takes the file's role. After `.`/`->` it borrows only
  a function, and before `::` only a namespace or a type. Strings, comments,
  keywords, built-ins, directive lines and the block's own namespaces and enum
  members stay. A name the file does not have keeps the block's role. Output
  is deterministic and bounded: at most 16,384 names of at most 128 bytes, and
  the block's span ceiling. This adds policy over classifications that already
  exist. It is not a parser, lexer or other general-purpose system, so
  `src/core/AGENTS.md`'s library question does not apply. Identifiers use the
  C++ refinement's byte rule.
- **Host performs** (`application/markdown_code_syntax.*`). A request and a
  publication carry an `Anchor`, which is the reviewed file's repository path.
  A context source supplies `markdown_code_context`: the file's
  classification key and its names. It is a lookup with no fetch. The block
  is still classified once per text and definition. Each anchored asker is
  answered with spans borrowed from its file when that file's context is held
  and its language matches the block's (C and C++ count as one family). The
  borrowed spans are cached by the block key and the file key (128 entries),
  so the same snippet in two files can differ, and a new head is a new entry.
  `context_changed(anchor)` answers again the recent anchored asks for a file
  (at most 256) from the cache.
- **Qt presents.** `markdown_body_view::set_code_context` anchors a body. Its
  store asks with the anchor and takes only answers for that anchor. Report
  finding code boxes anchor to the finding's place. A thread's posts anchor to
  the thread's file. A tour stop's side chat anchors to the stop's file. Other
  readers stay unanchored and unchanged.
- **Composition** (`src/app`). When the panes adopt spans for the right side
  of an unfiltered comparison whose right is the pull request's head, the
  window reads that file's names from the bytes on screen and the adopted
  spans. It keeps the names of 32 files by path and tells the host the file's
  context changed. Nothing is read or classified for this. No new event is
  logged.

## Acceptance

- **AC1, core.** Borrowed roles apply by name. Unknown names and names with
  conflicting roles fall back to the block's own classification, and the
  block's grammar keeps every span. Output is deterministic and bounded.
- **AC2, host.** Anchored and unanchored requests are answered separately.
  The borrowed cache is keyed by context. A file without context lends nothing
  and nothing is fetched for it. Its arrival recolours from the cache.
- **AC3, Qt.** A report finding's `cpp` block colours, as a type, a type that
  only the finding's file declares.
- **AC4, gates.** Before landing: `architecture_boundaries`, format,
  documentation and the resealed composition contract pass.

## Evidence (branch `claude/context-aware-code-highlighting`, macOS, Debug `-g0`)

- `core_code_context_roles` passed. It reads the file's names, keeps a
  constructor-called type as a type and drops `frame` (type and variable). It
  skips spans outside the text, inside a name or across lines. It borrows type,
  namespace and enum member, keeps function, and repaints no comment, keyword
  or directive line. After `.`/`->` and before `::` it borrows only a role
  that fits there. A new name gets no role. Equal inputs give equal results.
  The same block in another file reads differently. The span ceiling and the
  name and byte bounds hold.
- `application_markdown_code_syntax_host` passed. One text with four readers
  was classified once. A type was borrowed from `src/a.cpp`, a namespace from
  `src/b.cpp`, nothing from a Python file and nothing for the unanchored
  reader, which asked for no context. The cache served each file its own
  entry. A file's new key was borrowed afresh. A file with no context lent
  nothing, with nothing read or classified. `context_changed` recoloured from
  the cache, another file's arrival answered nothing, and nothing ran after
  shutdown. Failing-before control: when `answer_for` always returned the
  block's own spans, the test failed at "anchored to a file that declares it,
  the name is a type".
- `qt_markdown_code_{1_0,1_25,2_0}` passed. `check_report_file_context` used
  the composed packaged classifier and host. The file's classification made
  `frame_size` a type. The finding's box was anchored to
  `src/engine/swap_chain.cpp` and left `frame_size` plain until the file's
  names arrived. The names then recoloured it in the type colour, classifying
  nothing. `clamp_size` kept the function colour, and `clamped`/`size` stayed
  as before. Failing-before control: with the report box not anchored, the
  test failed at "the box is anchored to the finding's file".
- These related suites passed: `qt_agent_report_view`,
  `qt_conversation_cards_view`, `qt_tour_side_chat` and
  `qt_tour_chat_in_tab`. The changed composition units
  (`window_construction.cpp`, `window_workbench.cpp`) compile.
- `architecture_boundaries` passed. `coding-format-check` passed with
  clang-format 22.1.8 on 2,430 files.
- `application_review_round_trip_composition_contract`: the production app
  manifest digest is resealed (`3ce3715d…`), and the source pins pass. The
  probe step needs the full application executable, which this disk-limited
  run did not build.

## Owner acceptance

The owner accepted the change in attended review session
`4708604404524750554d23a4282d7890`, candidate `report-code-file-names`, on
2026-11-21.

## Landing evidence (main, macOS, Debug)

- The branch was rebased onto main `2b644c1eb` after the 0.5.0 freeze lifted.
  The provisional `BPR-XXXX-C` became `BPR-2351` in the card, code and test
  comments, `docs/architecture/native-markdown.md`, the contract note and the
  commit subject.
- The production app manifest digest was resealed over the merged tree
  (`1a8ba61a…`). The full dev build (2,645 steps) succeeded in a fresh
  configure, and `application_review_round_trip_composition_contract` passed
  in full, including the probe step.
- `core_code_context_roles`, `application_markdown_code_syntax_host`,
  `qt_markdown_code_{1_0,1_25,2_0}`, `qt_agent_report_view`,
  `qt_conversation_cards_view`, `qt_tour_side_chat`, `qt_tour_chat_in_tab`
  and `architecture_boundaries` passed. `coding-format-check` and the
  documentation gate passed.

## Follow-up scope (not part of this card)

- The draft composer and the Agent tab's item side chat are not anchored.
  Each can call `set_code_context` with its file.
