---
schema: "implement-rfc.card.v1"
id: "BPR-1341"
title: "Rename the Conversations modality to Threads"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1341--BPR-1341--84eb8fdc7d32.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1293","BPR-1295","BPR-1251","BPR-1255","BPR-1324","BPR-1304"]}
acceptance: [{"criterion":"BPR-1341:owner-decision","outcome":"pass","revision":"a4100c409","evidence":{"kind":"observation","detail":"Both owner lines of 2026-10-08 ('I'm considering rebranding conversations as threads'; 'Go ahead') are quoted verbatim on the card, in RFC-BPR-0028's dated amendment, the specification and BPR-1293's card."}},{"criterion":"BPR-1341:rename","outcome":"pass","revision":"a4100c409","evidence":{"kind":"command","detail":"qt_workbench_window_scale_100/125/200 assert the segment 'Threads', 'Threads modality', reviewModalityThreadsAction/Button, the tooltip 'Find a thread in the pull request (⌘4, ⌘J)', showWorkbenchThreadsAction, workbenchThreadsPage, the finder button's 'Find a thread in the pull request' and 'In Threads the finder is the page'; qt_conversation_finder_scale_* assert 'Only threads with new replies (3)' and threadFinder* names; qt_profile_management_window asserts View '&Threads'; application_profile_empty_desktop_native_menu_bar asserts the native View item 'Threads' (passed on the 76b4b4f99 base); core_workbench_interaction asserts workbench_modality::threads and threads_finds_a_thread. All pass offscreen on macOS."}},{"criterion":"BPR-1341:kept","outcome":"pass","revision":"a4100c409","evidence":{"kind":"command","detail":"qt_workbench_window asserts the drawer's toggle and panel are named Comments and Discuss's tooltip and names are unchanged; qt_github_conversation_panel and qt_composed_github_scale_* pass unchanged."}},{"criterion":"BPR-1341:title-row","outcome":"pass","revision":"a4100c409","evidence":{"kind":"command","detail":"qt_workbench_title_row_scale_100/125/200 and qt_workbench_window_scale_100/125/200 pass on macOS offscreen with the shorter segment. Windows (BPR-1324's thresholds) was not run here."}},{"criterion":"BPR-1341:rows","outcome":"pass","revision":"a4100c409","evidence":{"kind":"command","detail":"ctest --preset dev, 30 rows, 100% pass: architecture_boundaries, architecture_boundary_contract, core_workbench_interaction, application_overview_feedback, application_profile_management_host, qt finder/title row/window/composed github at 3 scales, qt_menu_bar_status_item(_cocoa), qt_profile_management_window, qt_github_conversation_panel, application_large_review_benchmark, application_uiforetw_selected_diff_contract, application_workbench_composition_contract, application_review_round_trip_composition_contract (resealed full-application and manifest pins; mutation pin unchanged), application_overview_feedback_public_path, application_attended_recent_replay, application_profile_management_public_path, application_profile_empty_desktop_native_menu_bar, qt_hide_drawers_native_menu_key. application_composition_contract (scratch CMake builds, untouched by this change) timed out at 600 s on a nearly full disk and was not counted. Re-run after rebasing onto 45185d154: 28 of the 30 pass; application_uiforetw_selected_diff_contract (title_row_exact_order: the row now holds BPR-1338's workbenchOpenOnGithubAction, 18 actions where the course pins 17) and application_profile_empty_desktop_native_menu_bar (the window's Pull Request menu gained Open on GitHub, the resident bar did not) fail from e8e1567c4 (BPR-1338), not from this rename; both passed here on the 76b4b4f99 base."}},{"criterion":"BPR-1341:docs","outcome":"pass","revision":"a4100c409","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK; the specification's Modalities, shortcut table and layout, RFC-BPR-0028's dated amendment with AC-03, RFC-BPR-0052 AC-06 (and the BPR-1021/BPR-1135 closeout rows), the fixture catalog and BPR-1293's card say Threads."}}]
---

# BPR-1341: Rename the Conversations modality to Threads

## Owner decision

Owner, chat, 2026-10-08, verbatim: "I'm considering rebranding conversations
as threads".

The coordinator proposed the rename below. The owner answered, verbatim:
"Go ahead".

## Scope

- The ⌘4 modality (Ctrl+4 elsewhere) built by [[BPR-1293]] as Conversations is
  named **Threads**: its strip segment, its accessible name ("Threads
  modality") and its action and button object names
  (`reviewModalityThreadsAction`, `reviewModalityThreadsButton`, derived from
  the title).
- ⌘J's tooltip "Find a conversation in the pull request" becomes "Find a
  thread in the pull request", on the window's View action, the Comments
  drawer's finder button and the strip segment ("(⌘4, ⌘J)").
- The finder's own words: its accessible name and description, the search
  placeholder and accessible name ("Find a thread"), the key's tooltip, Only
  new / Hide resolved / label filter names, the empty pages, and its file
  rows' counts ("one thread", "n threads").
- The View menu item, on the window and on the empty desktop's native menu
  bar: "&Threads". The status text that disables Hide Drawers there: "In
  Threads the finder is the page".
- The core's modality `workbench_modality::conversations` and its reason
  `conversations_finds_a_conversation` become `workbench_modality::threads`
  and `threads_finds_a_thread`; the Qt `review_modality` likewise. Object
  names tests key on follow (`workbenchThreadsPage`,
  `workbenchThreadsFinderButton`, `showWorkbenchThreadsAction`,
  `desktopShowThreadsAction`, `threadFinder*`).
- The workbench specification (Modalities, the shortcut table, the primary
  layout), RFC-BPR-0028 (a dated amendment and pointers at its Modalities
  paragraph and AC-03), RFC-BPR-0052 AC-06, the fixture catalog's modality
  table, and [[BPR-1293]]'s card.

Kept as they are, by the owner's decision: the Comments drawer and its
"Comments (n)" toggle, and Discuss. Kept as internal names: the
`conversation_finder` class and its file names, the domain type
`feedback_conversation`, the drawer's `conversations_*` settings columns, the
`conversations-v1` fixture, and the large-review benchmark's
`conversations_*` phase names, which station records and [[BPR-1325]] cite.

## Acceptance

- `BPR-1341:owner-decision`: both owner lines are quoted verbatim above.
- `BPR-1341:rename`: every user-visible name of the modality and its finder
  listed in Scope reads Threads or thread, and the core and Qt enums are
  renamed; no user-visible "Conversations" remains for the modality.
- `BPR-1341:kept`: the Comments drawer, its toggle and Discuss keep their
  names and behaviour.
- `BPR-1341:title-row`: `qt_workbench_window` and the title-row rows pass at
  1.0x, 1.25x and 2.0x with the shorter segment.
- `BPR-1341:rows`: the finder, window, title row, menu bar, composed GitHub,
  application courses that name the modality, and architecture rows pass;
  the composition pin is resealed.
- `BPR-1341:docs`: the specification, RFC-BPR-0028's dated amendment and
  BPR-1293's card say Threads, and the documentation gate passes.

## Verification (macOS, offscreen, 2026-10-08)

`ctest --preset dev`, 30 focused rows: the finder, workbench window and title
row at 1.0x, 1.25x and 2.0x, composed GitHub at three scales, the GitHub
conversation panel, the menu-bar rows, profile management, core workbench
interaction, overview feedback, architecture boundaries, the large-review
benchmark, the UIforETW selected-diff course, the workbench and round-trip
composition contracts, attended recent replay, the empty-desktop native menu
bar and the Hide Drawers native key. All 30 passed on the `76b4b4f99` base.

After rebasing onto `45185d154`, 28 pass. Two fail from `e8e1567c4`
([[BPR-1338]]), not from this rename:

- `application_uiforetw_selected_diff_contract`: `title_row_exact_order`. The
  title row now holds `workbenchOpenOnGithubAction`, 18 actions where the
  course pins 17.
- `application_profile_empty_desktop_native_menu_bar`: "the windowless bar
  retains the full workbench command set". The window's Pull Request menu
  gained Open on GitHub and the resident desktop bar did not.

The round-trip composition pin is resealed over this card and over
`7a35a4bed` ([[BPR-1336]]) and `e8e1567c4` ([[BPR-1338]]), which reached main
without a reseal. `application_composition_contract` (scratch CMake builds,
untouched here) timed out at 600 s on a nearly full disk and is not counted.

Windows was not run. The shorter segment can only give [[BPR-1324]]'s title
row more room; its Windows thresholds are unchanged.
