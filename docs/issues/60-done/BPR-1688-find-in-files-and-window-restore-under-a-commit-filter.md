---
schema: "implement-rfc.card.v1"
id: "BPR-1688"
title: "Find in files and window restore under a commit filter"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1686"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1685--BPR-1688--a54310676f44.json","origin":"RFC-BPR-0089 'What a filter drives' and its window rule, which no milestone names","related_cards":["BPR-1686"],"frontend_design":"frontend-design:frontend-design loaded before the search panel's filtered-set wording"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|core_source_search_policy|qt_source_search_panel_scale_(1|2)|application_source_search_host|application_source_pr_find|application_source_pr_search_scope|application_source_pr_search_retry)$'","detail":"Fixture, shipped composition, offscreen (commit-filter-comment-v1, the course filtered_find_and_restore). Whole PR: 'inserted 1 C3' is found in src/notes.txt and 'line 5 C1' only in src/app.cpp, and the pane does not mention a filtered set. Hide C3: Find in files is bound to the filtered set's 99 files (scope FilteredFiles 99); 'inserted 1 C3' is not found (src/notes.txt is read at C1), 'line 5 C1' is found in src/app.cpp and src/notes.txt, and the pane reads '2 matches in 2 files; searched the filtered set'. Core: valid(scope, files) admits a sorted list of the named count with each filtered diff's commits and refuses an unsorted, repeated, empty, miscounted or one-commit list, a filtered document search, and a set past the bound. Qt: idle 'Type to search the filtered set's 12 files on both sides.', the singular, 'The filters leave no file to search. Clear a filter to search more.', and nothing added without a filter. The unfiltered Find in files courses pass unchanged. No GitHub mutation. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|core_window_review_restoration|application_commit_filter_model|application_window_review_restoration|sqlite_window_review_restoration_storage|application_profile_window_review_restoration_public_path|application_document_tabs_restoration_public_path)$'","detail":"Desktop composition, offscreen (filters_restore_with_the_window): with C3 hidden and C1's files hidden by a file filter (96 of 99 files), the reviewer reads a file in Review and quits. Launched again, Resume from Home brings back '2 of 3 commits shown' and the file filter with the same 96 files. With a hidden commit the pull request no longer has written into the kept filters, the next Resume names it ('Hidden commit 0000000 (Rewritten away) is no longer in the pull request') as after a force push while the rest of the filter applies. Core: accepted_filters attaches to the window's destination and its recently viewed entry, refuses another destination, a stale generation, invalid filters and filters that keep a commit's changes; Resume carries them; moving to another pull request drops the window's and keeps the entry's. Model: restore_files, and a restored gone commit is named. Store: version 7 round trip on a record and listed entries, rewrite, refusal without loss, a payload of another version is corrupt, and versions 1 to 6 migrate (the draft migration's cancellation at each new step rolls back). The existing restoration courses pass. macOS."}}]
---

# BPR-1688: Find in files and window restore under a commit filter

RFC-BPR-0089 says a filter drives "Find in files, which says that it searches
the filtered set", and "The window keeps the filter with its comparison and
restores it with the window." Neither is in M1's milestone text (trees,
navigation, counts and the chip), nor in M2 to M4. M1 does neither.

## Before

- Find in files catalogued the comparison's own files and searched the merge
  base and head, also while a commit filter was active. It did not say so.
- A filter lived with the window's review. Reopening the window started at
  Whole PR. `commit_filter_model::restore` existed for the host to call.

## Change

- **Find in files.** The composition binds Find in files to the filtered set
  while a filter leaves files out: each file with its filtered diff's commits.
  The core scope carries only the count (`FilteredFiles`), so the search state
  stays trivially copyable; the epoch host keeps the list, narrows the
  catalogue to it and reads each side at its commits (cache or chain store),
  each one a commit the touches job authorized. The pane says it searches the
  filtered set.
- **Restore.** `window_review_filters` (hidden commits with headline and
  author, and file-filter paths) are kept by the core with the window's
  destination and its recently viewed entry, persisted in the restoration
  store's version 7 `review_filters` table, and carried by Resume. The
  composition retains them whenever they change and gives them to the commit
  filter when the restored review is adopted (`restore`, `restore_files`). A
  kept commit the pull request no longer has is named as after a force push.
  Restoring needs the desktop composition, which owns the restoration store.

## Acceptance

- **AC1.** With a filter active, Find in files searches only the filtered
  files, each at the endpoints its filtered diff reads, and says that it
  searches the filtered set.
- **AC2.** The window keeps the filter with its comparison and restores it
  with the window; a restored filter that names a gone commit is shown as
  after a force push.
