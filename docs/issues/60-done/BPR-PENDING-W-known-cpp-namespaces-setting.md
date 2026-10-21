---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-W"
title: "Known C++ namespaces setting"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-1710", "BPR-1711"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner request 2026-10-21 (relayed by the coordinator)","owner_note":"Lets add a setting for a list of well known namespaces and pre populate it with std and then users can just add their own.","related_cards":["BPR-1710","BPR-1711","BPR-1107","BPR-1336"],"related_rfcs":["RFC-BPR-0034","RFC-BPR-0072"],"frontend_design":"frontend-design:frontend-design loaded before the Syntax card, its field and its refusal wording"}
acceptance: [{"criterion":"AC1: the list is an explicit core input; std by default, nested entries, refused entries ignored, a blob's own class wins","outcome":"pass","revision":"ff56e07529a4682e1b6b1292982a096c0d01f826","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_computation_boundaries|syntax_classification_contract)$'","detail":"test_known_namespaces_classify (no Qt, no grammar): an empty list predeclares nothing (std::chrono is two type scopes); the default list equals {std} and equals a call naming no list, and brings std::chrono; {std, Qt, boost::asio} makes Qt, boost and asio namespaces, leaves boost::asio::ip a type scope and boost::filesystem unpredeclared; reordered and redundant lists ({boost::asio, Qt, std}, {Qt, boost, boost::asio, std}) yield equal spans; refused entries (3d, my-lib, int, boost::, ::Qt) are ignored while Qt still applies; struct Qt and a global enum class Qt { AlignLeft } win over a listed Qt, the enumerator resolving. test_known_namespace_rules pins the core judge (empty part, invalid character incl. non-ASCII, leading digit, keyword, 128-byte bound, duplicate, 64-entry bound), the comma and white-space split, join, round trip and canonical form. syntax_classification_contract through the packaged classifier: a key listing Qt paints N13's Qt as a namespace and AlignLeft keeps the grammar's built-in role; an empty list leaves std::ranges built-in; a key naming no list classifies exactly as before. Failing-before control: with the refinement reading a fixed default list (scratch neutralization, restored), both fail ('an empty list predeclares nothing', 'a listed Qt replaces the grammar's built-in role')."}},{"criterion":"AC2: the list keys C++ classifications, persists, and recolours open panes without a restart","outcome":"pass","revision":"ff56e07529a4682e1b6b1292982a096c0d01f826","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_appearance_profile_consumers|sqlite_appearance_settings_storage|qt_syntax_pane_composition)$'","detail":"Host: an applied list fans out to both windows, persists as written and publishes only the Namespaces domain; classification_key puts the canonical list on C++ keys and leaves Python's alone; the old list's C++ spans are not served, C++ is classified again and Python answers from the cache; a reordered list reparses nothing; an earlier list answers from its cached spans; {std, 3d, std} is refused whole naming (1, leading_digit) and (2, duplicate); Reset restores std; a durable list the core refuses is refused on restore and a legal one adopted. Store: schema 10 round-trips every legal list in order, version-9 and version-8 files migrate to std with every other value kept, the CHECK refuses an oversize column and 'std, 3d' fails the load. Pane: the packaged classifier, the appearance host, the Settings panel and a real diff_workbench; typing 'std, Qt' and Return applies and persists the list, reclassifies both sides (two blob reads) and repaints Qt from the built-in to the namespace colour in Light and Dark with AlignLeft still built-in; returning to std answers from the cache. Failing-before controls: without the facade stamping the key, the host test fails at 'C++ is keyed under the canonical list' and the pane at 'applying the list asks again for both sides'; without the store reading the column, 'and loads back exactly, in its order'."}},{"criterion":"AC3: the Settings field edits, validates inline and resets; docs; gates","outcome":"pass","revision":"ff56e07529a4682e1b6b1292982a096c0d01f826","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_appearance_settings_panel|qt_settings_window(_scale_(125|150|200))?|qt_settings_effect_boundary|application_appearance_host|application_markdown_code_syntax_host|syntax_refinement_cancellation|architecture_boundaries|architecture_boundary_contract|application_cpp_identifier_appearance|application_syntax_delivery|application_appearance_(gesture|store_fallback|theme_customization|older_theme)_contract)$'; ctest --preset core-only; cmake --build --preset dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"Panel: Appearance > Syntax holds 'C++ namespaces' showing 'std'; Return offers the whole list split by the core; an adoption shows it joined by commas; 'std, 3d, my-lib, Qt, Qt' is not offered and reads 'Not saved. \"3d\" starts a name with a digit; \"my-lib\" has \"-\", which a namespace name cannot hold; \"Qt\" is listed twice.' under the field in WindowText, keeps the text, is spoken with the field and survives an unrelated adoption; the line goes once the text is legal; five refused entries name three and count two; Reset dispatches known_namespaces and drops uncorrected text; a re-spaced equal list is not offered; a host refusal is named the same way; an empty field offers the empty list. Failing-before control: with the panel not dispatching, 'Return offers the whole list'. Results in the Verification section."}}]
---

# BPR-PENDING-W: Known C++ namespaces setting

## Owner note

The owner, 2026-10-21, verbatim:

> Lets add a setting for a list of well known namespaces and pre populate it
> with std and then users can just add their own.

## Before

[[BPR-1710]] and [[BPR-1711]] made C++ qualifiers read as namespaces with a
fixed list in `cpp_identifier_context.cpp`: `std`, predeclared, and the
namespaces ISO C++ nests in it. Nothing else could be added, so
`Qt::AlignLeft` kept the grammar's built-in role and `boost::asio::` was two
type scopes.

## Change

- **Core.** `core::known_namespaces` is the list as the reviewer wrote it;
  default-constructed it is `std`. The core splits typed text at commas and
  white space, judges each entry (an ASCII identifier, or a nested path of
  them joined by `::`; no keyword; at most 128 bytes; listed once; at most 64
  entries) and gives the canonical form, the legal entries sorted and unique.
  `refine_cpp_identifiers` takes the list as an explicit input and predeclares
  each legal entry and every segment of a nested one in the global scope, as
  if the blob declared it. `std` brings the standard's nested namespaces, so
  the list stays a short list of top-level names. A nested path is also
  accepted (`boost::asio`); a scope under a listed name that the list does not
  name keeps the scope role. A predeclared name gives way to a class or enum
  the blob declares with it, enumerators included, which a global
  `enum class Qt` previously could not have done.
- **Determinism and cache.** `token_span_cache_key` gains the list.
  `classification_key` sets it to the canonical list for the C++ definition
  only, so a reordered list shares one identity and no other language is
  keyed away. The appearance host stamps every `classify` and `cached` key,
  so a changed list asks under a new key: no span cached under another list
  is served, and an earlier list answers from its own cached spans. The
  packaged classifier passes the key's list to the refinement.
- **Persistence and host.** The list is part of `appearance_settings`. The
  preferences host refuses a list with any refused entry whole
  (`invalid_known_namespaces`, naming each), adopts and persists a legal one,
  resets it to `std`, refuses a durable one the core refuses, and publishes a
  `Namespaces` invalidation to every window of the profile. The appearance
  store is schema 10 with a `known_namespaces` column; a version-9 record
  migrates to `std`.
- **Composition.** On a `Namespaces` invalidation the window asks again for
  the C++ spans of the comparison on screen. Its blob reads are bound again,
  and a publication for a list no longer in effect is dropped without
  releasing the binding the new request reads through. The panes keep their
  colours until the new spans arrive and recolour without a restart.
- **Settings.** Appearance gains a **Syntax** card between Colours and Dif,
  with one row, **C++ namespaces**: a field showing `std, Qt, boost::asio`,
  its consequence in the row's info glyph, and Reset in the card's header.
  Return or leaving the field applies it. A refused entry is named under the
  field with its reason in the page's own text role; the text stays for
  correcting and the line goes as soon as the text is legal.
- **Docs.** The Settings guide, the settings authority inventory and
  appearance-and-accessibility describe the setting, its rules and the cache
  identity. The workbench specification lists no Appearance setting and is
  unchanged.

The RFC-BPR-0081 evaluation harness and candidate take the new parameter so
they still define the shipped signature; the candidate ignores the list. That
harness (`BPR_BUILD_SYNTAX_EVALUATION`) was not built.

Not in scope: fenced C++ in comments ([[BPR-1562]]'s Markdown host) and
Review Cycles classify through their own hosts and keys and still use `std`
alone.

## Acceptance

- **AC1, core.** The list is an explicit input to the C++ refinement: an empty
  list predeclares nothing, the default is `std` with its nested namespaces,
  user entries and nested paths are namespaces, refused entries are ignored,
  equal sets classify equally in any order, and a blob's own class or enum of
  a listed name wins. The tests fail before the change.
- **AC2, identity and composition.** The canonical list is part of every C++
  classification key and of no other; a change never serves another list's
  spans, persists, restores and resets, and recolours an open pane without a
  restart. The tests fail before the change.
- **AC3, Settings and gates.** The field edits, refuses with the reason inline
  and resets; docs are updated; the focused suites, core-only, clang-format
  22, clang-tidy on the changed units and the documentation gates pass.

## Verification

Implementation `ff56e0752`, rebased on main `c9d535f62`; macOS (Apple silicon),
2026-10-21, the `dev` preset's Debug configuration built with `-g0` to save
disk.

- **Focused and composed suites pass:** `core_computation_boundaries`,
  `syntax_classification_contract`, `syntax_refinement_cancellation`,
  `application_appearance_profile_consumers`, `application_appearance_host`,
  `application_markdown_code_syntax_host`, `sqlite_appearance_settings_storage`,
  `qt_appearance_settings_panel`, `qt_syntax_pane_composition`,
  `qt_settings_window` at 100, 125, 150 and 200%, `qt_settings_effect_boundary`,
  `architecture_boundaries`, `architecture_boundary_contract`,
  `application_cpp_identifier_appearance`, `application_syntax_delivery`,
  `application_appearance_gesture_contract`,
  `application_appearance_store_fallback_contract`,
  `application_appearance_theme_customization_contract`,
  `application_appearance_older_theme_contract`,
  `command_correctness_registry_contract` and
  `application_review_round_trip_composition_contract`. After the rebase,
  which merged main's `github_application.cpp` changes and resealed both pin
  sets, the app and these suites were rebuilt and 18 of them rerun (all but
  the markdown host, refinement cancellation, the scaled Settings windows and
  the architecture boundary contract, whose sources the rebase did not touch):
  18 of 18 pass.
- **Failing before.** Four product behaviours were neutralized in scratch
  copies and restored: the refinement read a fixed default list, the host
  did not stamp the key, the panel did not dispatch, and the store did not
  read the column back. The core, contract, host, store, panel and pane tests
  then fail at the assertions named in AC1 to AC3.
- **Pane renders.** The 2x offscreen captures were checked by pixel. Line 5's
  `Qt` reads `#953800` (built-in) before and `#1b5e70` (namespace) after in
  Light, and `#6da9db` before and `#b8d7a3` after in Dark. They are kept
  outside the repository.
- **core-only:** builds, and 310 of 313 pass. `documentation_structure`,
  `documentation_issue_graph_contract` and `issue_id_allocation_contract` fail
  only because this worktree has no `.agents/skills/implement-rfc`.
- **Gates.** `coding-format-check` (clang-format 22.1.8) passes on 1,725
  files. clang-tidy (LLVM 22) with warnings as errors passes on the eight
  changed product units and the six changed test units. Both reseal checks
  report nothing left to reseal. `verify_documentation_tests.py` passes 67 of
  67. `verify_documentation.py`, run with `.agents` linked, refuses only this
  card's provisional ID format. Before the rebase it also reported `NEXT_ID`
  rewinding a historical high water, with or without this card; with a
  stand-in ID the card itself raised nothing.
- **Not run.** The RFC-BPR-0081 evaluation harness
  (`BPR_BUILD_SYNTAX_EVALUATION`) was not built, so its signature change is
  unverified.
