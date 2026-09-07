---
schema: "implement-rfc.card.v1"
id: "BPR-0661"
title: "Review behaviour preferences on Reviews & queue"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"Each preference round-trips through the store and changes the behaviour it names in a composed course; the page reverts on refusal","outcome":"pass","revision":"2e584708","evidence":{"kind":"observation","detail":"sqlite_workbench_settings_storage: a version-three store migrates to schema 4 keeping every value, the five preferences round-trip, a value past a vocabulary is refused as an integrity failure. application_workbench_host: apply_behaviour persists and publishes the record as its own domain, an unchanged record does nothing, a record past a vocabulary is refused with the held record republished, a restarted host restores and publishes it. qt_workbench_window_scale_{100,125,200}: the rows dispatch the whole record, follow the published record (so a refused change reverts) and reset by group; Continue review enters Review and a pull request opened from Home opens in Read or the last-used modality; the drawer opens on the preferred filter and the preferred named comparison is asked for once offered; when fully seen dispatches the file's mark-read command once. application_review_behaviour_contract (new, github-com and ghes-3.18, seed and reuse): two preferences chosen on the page are stored by the host, Continue review enters Review, and the reuse run over the same store opens the drawer on Unread; with Continue review mutated to ignore the preference the seed run prints honoured=0 and the contract fails. qt_composed_github_scale_100, qt_github_conversation_panel, qt_settings_window, application_review_round_trip_composition_contract (repinned in two passes) and application_composition_contract pass; test_network_isolation_manifest passes with the new pin; check_boundaries, strict tidy on every touched translation unit, clang-format --Werror and verify_documentation.py pass."}}]
---

# Review behaviour preferences on Reviews & queue

RFC-BPR-0028's Reviews & queue page holds the modality rules (which
modality a pull request opens in; which one Continue review enters; other
"when X, switch to Y" rules, none on by default), the reading behaviour
(mark a file read when fully seen or only by hand; the default comparison
on return; the comments drawer's first filter) and the saved repositories.
[BPR-0652](BPR-0652-settings-pages-accounts-appearance-reviews.md)
put the observer rules there and left these, because no preference
authority holds them: the workbench settings store carries the layout
only, and a control the application does not honour is not a live control.

## Scope

- A durable review-behaviour record in the workbench settings store
  (schema version 4): open modality (Overview, Read, last used), Continue
  review modality (Read, Review), mark-read behaviour, default comparison
  on return, comments drawer scope; validated by the host, published with
  the layout.
- The window honours them: the modality on open and on Continue review,
  the cards' scope, the epoch line's selection on return; the ledger's
  mark-read behaviour through the receipt commands.
- The page's rows as segmented controls over those intents; resets act on
  their own group only. Saved repositories are BPR-0659's.
- Owned paths: `src/application` workbench ports and host, `src/adapters/sqlite`
  workbench settings store, `src/adapters/qt` window and page, focused
  tests and the workbench settings contract.

## Acceptance

- Each preference round-trips through the store and changes the behaviour
  it names in a composed course; the page reverts on refusal.

## Notes (2026-09-07)

- **Record.** `application::review_behaviour_settings` (open modality:
  Overview, Read, last used; Continue review: Read, Review; mark read: by
  hand, when fully seen; on return: whole PR, since my last review, since
  last upload; comments drawer: all, unread, unresolved), every default the
  behaviour the window had, validated by `valid()` over the closed
  vocabularies. It travels inside `workbench_layout_settings` so the one
  settings port stores and loads it with the layout; the store's schema is
  version 4 (five defaulted columns on the singleton row, `drawer_filter`
  rather than `comments_filter` because the schema may name no review
  content), migrating from versions 1, 2 and 3. The host's
  `apply_behaviour` validates, persists and publishes the record as its own
  projection domain, and republishes the held record when a change is
  refused so the page reverts.
- **Window.** Two groups of segmented rows on Reviews & queue (one exclusive
  `QButtonGroup` per row: autoExclusive would have made every button under
  the editor one group), each group's Reset returning its own rows; a row
  dispatches the whole record and the page follows what is published. A
  pull request opened from Home opens in the preferred modality (last used
  is the modality chosen while one was open); Continue review enters Read or
  Review; the comments drawer's filter buttons are set once per open; the
  preferred named comparison is asked for through the epoch control's own
  action once the history offers it; "when fully seen" dispatches the file's
  own mark-read command once the shared scrollbar reaches its end, once per
  file and material. Preferences that land after a pull request opened are
  applied to it once.
- **Other "when X, switch to Y" rules.** None are on by default and none
  were invented: the two modality rules the design names are the rows.
- **Evidence.** `sqlite_workbench_settings_storage` (version-three fixture
  migrates, round trip, a value past a vocabulary refused),
  `application_workbench_host` (persist, publish, unchanged, refused and
  restored), `qt_workbench_window_scale_{100,125,200}` (rows, reverts, group
  resets; Continue review and opening; filter and comparison on open;
  mark-read when fully seen), and the new
  `application_review_behaviour_contract`: over github-com and ghes-3.18
  the seed run chooses "Continue review enters Review" and "opens on
  Unread" on the page, the host stores and publishes, Continue review enters
  Review (`honoured=1`); the reuse run opens over the same store and the
  drawer is on Unread. With Continue review mutated to ignore the preference
  the seed run prints `honoured=0` and the contract fails.
