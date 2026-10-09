---
schema: "implement-rfc.card.v1"
id: "BPR-1355"
title: "User-visible Conversation strings say thread"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1355--BPR-1355--f6844b939e90.json","related_cards":["BPR-1341","BPR-1354"]}
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
