---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-C"
title: "Ignore pull requests on Home"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner request, 2026-10-20: ignore a PR on Home into a new Ignored category that sorts to the bottom and starts collapsed; unignore by expanding it; an ignored PR that closes stops being tracked on Home","provisional_id":"BPR-PENDING-C is a placeholder; allocate_issue_id.py runs only on main, and the integrator allocates and renames at landing","related_rfcs":["RFC-BPR-0066","RFC-BPR-0067"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2085a4d6e","evidence":{"kind":"command","detail":"core_home_ignore: every bucket of an ignored item is in home_queue_section::ignored and keeps its bucket; ordered_queue and queue_precedes put ignored items after every active item, bucket then identity within; Ignore is accepted only for an open, unignored, usable, in-bound ref; Unignore anywhere; another account's same number is not ignored; a restored list is normalized and bounded."}},{"criterion":"AC2","outcome":"pass","revision":"2085a4d6e","evidence":{"kind":"command","detail":"core_home_ignore: observe prunes closed (pruned_closed) and otherwise-departed (pruned_left) ignores once each, in identity order, closed winning; open and unobserved ignores stay; a pruned PR can be ignored afresh. application_review_queue_enrollment: a closed metadata answer departs the row and prunes its ignore from the store; a restored ignore of a closed record is pruned at restore while one with no record is kept."}},{"criterion":"AC3","outcome":"pass","revision":"2085a4d6e","evidence":{"kind":"command","detail":"sqlite_queue_enrollment_storage: home_ignore round-trips across a reopen in identity order with the other account's row apart; a repeat ignore and an absent unignore succeed; an unusable ref refuses the whole transaction; prune and unignore remove exactly their rows; a version-ten file migrates to schema 11 with the table empty; older downgrade fixtures still migrate. application_review_queue_enrollment: the host writes the ignore before keeping it, a refused write is rolled back, and a restart restores the ignored row."}},{"criterion":"AC4","outcome":"pass","revision":"2085a4d6e","evidence":{"kind":"command","detail":"qt_home_list_scale_100 and _200: Ignored is the last panel after Recently viewed, shown collapsed with 'Ignored', its count and '2 ignored', neutral (no tone); its rows are in no other panel, flat card or group; Right on its header expands it; ignored rows offer Unignore (hover/focus button named 'Unignore octo/app#7'), draw no state colour and describe themselves as ignored; Unignore, Backspace on an ignored row and Delete on a queue row dispatch the row; a queue of only ignored rows shows the all-empty sentence above Ignored. Offscreen renders inspected collapsed and expanded."}},{"criterion":"AC5","outcome":"pass","revision":"2085a4d6e","evidence":{"kind":"command","detail":"qt_workbench_open_review_from_home_scale_100: the row menu is Open Review, Open Review in New Window, Resume, Open on GitHub, Ignore, Remove from Recently Viewed; Ignore is shown on a queue row, hidden on a Recently viewed row, claims no window shortcut, and with Delete dispatches core::home_ignore_intent for the exact ref. qt_workbench_window_scale_100 (no key claimed twice, catalogue matches live bindings) and qt_keyboard_shortcuts_scale_100 pass with the new home_ignore entry."}},{"criterion":"AC6","outcome":"pass","revision":"2085a4d6e","evidence":{"kind":"command","detail":"Candidate is the working tree on 2085a4d6e, committed as this card's slice. Focused dev build and these ctest courses pass: core_home_ignore, core_review_queue(_attention), application_review_queue_enrollment/refresh/notification, application_notification_delivery, sqlite_queue_enrollment_storage, qt_review_landing_model, qt_home_list_scale_100/200, qt_composed_github_scale_100, qt_composed_home_sign_in, qt_repositories_drawer, qt_keyboard_shortcuts_scale_100, qt_workbench_home_discovery/open_review_from_home/window_scale_100, qt_review_queue, qt_settings_effect_boundary, qt_surface_capture_course_scale_100, application_queue_composition_contract, application_home_waiting_public_path, application_home_open_feedback_public_path, test_network_isolation_manifest, architecture_boundaries, documentation_structure; and application_composition_contract plus the review_round_trip (resealed), workbench, review_ledger, defect, uiforetw_selected_diff and native_notification_attended composition contracts. command_correctness_registry_contract passes with this card's re-pinned workbench_window_tests.cpp and github_application.cpp hashes (no case added or reordered). coding-format (clang-format 22) passes; clang-tidy on the touched translation units reports only three findings on lines this card does not change. verify_documentation.py and verify_documentation_tests.py pass with a valid ID substituted; with the provisional ID the board model refuses only the ID format."}}]
---

# Ignore pull requests on Home

The owner asked on 2026-10-20: "I want to be able to ignore a PR on the home
screen, which will put it in a new ignored category that sorts to the bottom
and starts collapsed, you. can unignore something that's in ignored by
expanding the collapsed. If an ignored PR gets closed it stops being tracked
on home." [RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md#owner-amendment-ignored-pull-requests--2026-10-20)
records the amendment. This card builds it.

## Acceptance

- AC1: Categorizing and sorting are core policy. An ignored item is in a new
  last Home section and orders after every other item. Ignore and Unignore
  are typed transitions that return semantic deltas. An ignore is scoped to
  one host, account, repository and number.
- AC2: When the queue observes an ignored pull request closed or merged, or
  off Home for another reason, the core prunes its ignore and it leaves Home.
- AC3: Ignoring persists across restarts in the SQLite queue store, keyed by
  the whole pull request identity, with a forward migration.
- AC4: Home shows Ignored last in every arrangement, collapsed by default,
  neutral, with Unignore on each row. The Qt side only projects the core's
  section.
- AC5: A queue row's menu offers Ignore, and a key does the same, with
  accessible names. Recently viewed rows do not offer it.
- AC6: Focused build and tests, format and tidy, and the documentation gate
  pass.

## Design

**Core** (`bpr/core/home_ignore.hpp`). `home_ignore_list` holds the
reviewer's ignored `pull_request_ref`s in identity order, bounded at
`maximum_home_ignores` (16384, the enrollment bound). `apply(intent,
tracking)` accepts Ignore only for a row Home observes as `open`, and accepts
Unignore anywhere. It returns a `home_ignore_delta` (`ignored` or
`unignored`). `observe(observations)` returns `pruned_closed` or
`pruned_left` for every ignored pull request the queue holds as departed.
`queue_item::Ignored` makes `home_section_for(item)` return
`home_queue_section::ignored`, and `queue_precedes` orders ignored items
last. Attention buckets are unchanged.

**Host** (`review_queue_application_host`). The host maps each retained
enrollment to a tracking value: present is `open`, departed `closed` is
`closed`, and any other departure is `left`. A ref with no record is not
observed, so its ignore is never pruned on absence. `apply(home_ignore_intent)`
asks the core, then writes the delta through
`queue_enrollment_store_port::apply_home_ignores` before keeping it, and rolls
it back on refusal. After every stored departure and after restore, the host
settles: the core prunes, and the store is told. `project` marks items from
the list, and `notification_for` refuses ignored items.

**Storage**. The SQLite queue store gains table `home_ignore(host, account,
repository, pull_request)` with the whole identity as the primary key, in
schema 11. The migration only creates the table. Writes are one transaction
per delta batch, and the bound is enforced in SQL as well.

**Qt**. `home_list` adds one neutral Ignored panel after Recently viewed in
every grouping and State-headers setting. Slices of no one state exclude
ignored rows, as do group keys and the all-empty and filtered sentences. The
panel's rows carry the labelled row action Unignore. The row model returns no
state for an ignored row, so it draws no rail, mark or state ink. The window
row menu gains Ignore, labelled Unignore on an ignored row. Delete or
Backspace on a Home queue row is read by the list and dispatches the row. The
catalogue lists it as "Ignore or unignore the pull request". The action claims
no sequence, because Delete is the tour editor's Remove in this window. This
is the same reason Remove from Recently Viewed claims none. The
`frontend-design` skill was loaded for this work. The panel reuses Recently
viewed's neutral treatment and adds no palette or type.

## Decisions to revisit

- Ignored's collapse is window-session state, so every new window and every
  launch starts it collapsed. Remembering it per window would need the
  workbench settings collapse bound widened in a settings schema migration.
- An ignore is pruned on any observed departure, not only on close. Discovery
  cannot tell a closed pull request from a withdrawn request, so a pull
  request that returns arrives un-ignored.
- An ignored row raises no notification. The repository drawer's counts are
  unchanged.
- The active lists' accessible descriptions do not yet mention Delete.
  Ignored's list says "Delete unignores it".

## Composition tripwire

The only `src/app/src/github_application.cpp` change is one window dispatch.
It sends `core::home_ignore_intent` to the profile queue host, beside Follow.
It adds no provider, credential, grant, mutation, publication or
source-inventory authority. `application_review_round_trip_composition_contract`
is resealed for the full application digest and the combined app manifest
digest. The production mutation-branch pin is unchanged and passes. The three
`github_application.cpp` input pins in `tests/command_correctness_registry.json`
carry the new hash. The `workbench_window_tests.cpp` family pin and
`tests/workbench_case_inventory.json` `source_sha256` carry the edited test's
hash. Its cases are unchanged.

## Execution

The candidate is this card's commit on `2085a4d6e`. The renders were inspected
offscreen only, and no owner desktop was driven. No live GitHub effects
occurred. At landing, allocate the ID on main and rename this file and every
`BPR-PENDING-C` reference. If main has moved again, re-check the pins on
`workbench_window_tests.cpp` and `github_application.cpp`.
