---
schema: "implement-rfc.card.v1"
id: "BPR-1355"
title: "User-visible Conversation strings say thread"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1355--BPR-1355--f6844b939e90.json","related_cards":["BPR-1341","BPR-1354"]}
acceptance: [{"criterion":"BPR-1355:rename","outcome":"pass","revision":"f3ccc8df4","evidence":{"kind":"command","detail":"Every user-visible and accessible string that called a review thread a conversation in the Comments drawer, window, Overview, Settings' Overview counts help, the queue's attention reason and the tour CLI's human count text now says thread(s). qt_github_conversation_panel, qt_workbench_window_scale_100/125/200, application_overview_feedback_public_path, application_overview_labels_public_path, application_discuss_top_public_path and application_setup_destination_public_path pass with the new words pinned. qt_pull_request_overview_page and its chips rows reach the renamed breakdown sentence and pass it, then fail at BPR-1094's 'a refresh keeps the scroll position and the chip', which fails on main since c2ea0e158 (recorded on BPR-1331) and fails the same way with this card's overview changes taken out."}},{"criterion":"BPR-1355:scan","outcome":"pass","revision":"f3ccc8df4","evidence":{"kind":"command","detail":"application_thread_words_public_path passes on macOS offscreen: overview-labels-v1 in Overview, Review (renderer.cpp open, both drawers), Discuss, Threads and Settings; widget text, titles, placeholders, tooltips, status tips, accessible names and descriptions, tab and combo items, actions, item-model headers and rows, and the accessibility tree contain no 'conversation' in any case besides 'AI conversations'. It confirms it read 'Threads', 'Thread file scope', the 39-thread count sentence and Settings' Overview counts help. With two old strings temporarily restored it failed and named both."}},{"criterion":"BPR-1355:kept","outcome":"pass","revision":"f3ccc8df4","evidence":{"kind":"command","detail":"Object names, identifiers, stable names (unread_conversation), the tour CLI's JSON unit 'conversation' (tour_cli_labels passes), fixture names and data, and the Notes tab's 'AI conversations' are unchanged. The round-trip composition pins print the committed digests be455223…, 3300a257…, e067befe…; application_review_round_trip_composition_contract and architecture_boundaries pass."}},{"criterion":"BPR-1355:docs","outcome":"pass","revision":"f3ccc8df4","evidence":{"kind":"command","detail":"docs/ux/workbench-specification.md (the BPR-1355 note in Threads beside Discuss, Discuss's band words and accessible names, and the Comments drawer toggle named as the UI names it), docs/ux/conventional-comments-feedback.md and docs/development/settings.md quote the new words; python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK."}}]
---

# BPR-1355: User-visible Conversation strings say thread

## Request

[[BPR-1341]] renamed the Conversations modality to Threads, but other
user-visible and accessible strings still called a review thread a
conversation: the Comments drawer list's accessible name and column header
("Conversations"), the Overview's comment-count tooltip ("Open conversations"
/ "Conversations"), and more. The owner asked (chat, 2026-10-08) for every
user-visible or accessible string, menu, status text, tooltip, announcement
and Settings label that says conversation(s) for this concept to say
thread(s), with tests, the specification, and a test that scans the assembled
window's visible texts and accessible names for "Conversation".

## Scope

Renamed, string literals only:

- The Comments drawer: the cards list's accessible name in Discuss
  (`Conversations` to `Threads`), its model's column header, "Read-only
  GitHub threads", "Selected thread", the scope tooltips, the file choice
  ("Thread file scope", "The file whose threads are listed", "Use the
  selected thread's file"), Clear filters' description, "Show every thread",
  the counts ("n thread(s), n unresolved, n new"; "No threads match these
  filters. Clear filters to show all PR threads."), the drill-down caption and
  its description, the resolve refusal ("Resolution is not available for this
  thread."), the drawer's description, and the file-review-comment note on
  the open card.
- The window: Discuss's Hide Drawers refusal ("In Discuss the thread is the
  page", both forms), Discuss's strip tooltip ("give the thread the room"),
  the central splitter's description ("comments drawer", the drawer's name),
  "Classify selected thread as defect", and the Comments tab tooltip.
- The Overview: every count sentence, chip tooltip and menu ("open threads",
  "Not counted: n threads", "No open threads"), the Comments column's tooltip,
  Open/All's names and tooltips, the files view's description, the loading
  and historical notes, and the Label key.
- Settings: the Overview counts help ("Open counts unresolved threads").
- The review queue's attention reason ("A thread here has unread comments.").
- The tour CLI's human count text ("Counts are threads: ...").

Kept: internal identifiers and object names (`conversationFileScope`,
`github_conversation_panel`, ...), persisted keys and stable names
(`unread_conversation`, the drawer's `conversations_*` settings columns), the
tour CLI's JSON (`"unit": "conversation"`), fixture names and fixture data
(`conversations-v1` and its fixture-only host label "GitHub.com — Constructed
conversations"), GitHub API terms, and the Notes tab's "AI conversations",
which are agent chats rather than review threads.

[[BPR-1354]] is editing tooltip code; this card changes only string literals
there and restructures nothing.

## Acceptance

- `BPR-1355:rename`: no user-visible or accessible string in the window, its
  Settings or the tour CLI's human text calls a review thread a
  conversation; the unit, panel, overview and window tests that pinned the
  old words pin the new ones.
- `BPR-1355:scan`: `application_thread_words_public_path` composes the
  application on overview-labels-v1 and, in Overview, Review (a file open,
  both drawers), Discuss, Threads and Settings, reads every widget's text,
  title, placeholder, tooltip, status tip and accessible name and
  description, every tab and combo-box item, every action's words, every item
  model's headers and rows in display, tooltip and accessible roles, and the
  accessibility tree, and finds no "conversation" in any case (besides "AI
  conversations"). It also proves it read Discuss's list, file scope and
  counts and Settings' Overview counts help.
- `BPR-1355:kept`: internal identifiers, persisted keys, JSON contract values
  and fixture names are unchanged; the composition pins still match.
- `BPR-1355:docs`: the workbench specification, the Conventional Comments
  feedback note and the settings guide quote the new words, and the
  documentation gate passes.

## Verification (macOS, offscreen, 2026-10-08)

Product commit `f3ccc8df4`, over `83263f10e`.

- `application_thread_words_public_path` passes. With the model header and
  the Discuss list's accessible name temporarily put back to
  "Conversations", it fails and names both: `QAbstractItemModel (header):
  Conversations` and the accessibility tree's `Conversations`.
- `ctest --preset dev`, 38 focused rows: architecture, the drawer panel,
  cards, finder at three scales, conversation model, overview page and
  chips, read marks, title row and workbench window at three scales,
  conversation reveal, workbench overview, composed GitHub at three scales,
  queue, label threads, public preview, queue notification, overview
  feedback, the setup-destination courses (main, nested folders, overview
  feedback, overview labels, Discuss top, thread words), attended recent
  replay, the round-trip composition contract, the UIforETW selected-diff
  course and the empty desktop's native menu bar. 35 pass.
- The three failures are `qt_pull_request_overview_page` and
  `qt_pull_request_overview_chips_scale_125/200`, all at "light icons at
  N%: a refresh keeps the scroll position and the chip". That assertion
  ([[BPR-1094]]) fails on `main` since `c2ea0e158`, as [[BPR-1331]]'s card
  records, and fails the same way here with this card's overview page and
  test changes taken out. The assertions before it, including the renamed
  breakdown sentence, pass.
- `application_setup_destination_public_path` (which does not run the new
  course) passed in 28 s twice and in 49 s once, and timed out at its 60 s
  limit twice while other work loaded the machine.
- `tour_cli_composition`, `tour_cli_process_commands`, `tour_cli_labels`
  and `tour_cli_public_entry` pass in the dev tree with
  `BPR_BUILD_TOUR_CLI=ON`. No test reads the human count text.
- The composition pins print the committed values: full application
  `be455223…`, manifest `3300a257…`, mutation `e067befe…`. No reseal.
- `coding-format-check` passes (clang-format 22.1.8). clang-tidy on the
  new test file reports only three findings that predate this card (lines
  727, 1828 and 2038).
- `python3 scripts/verify_documentation.py --root .` prints
  `DOCUMENTATION_OK`.

Windows and Linux were not run. The change is string literals and one test
course.
