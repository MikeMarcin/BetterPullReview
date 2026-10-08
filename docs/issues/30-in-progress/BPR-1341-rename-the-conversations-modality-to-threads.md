---
schema: "implement-rfc.card.v1"
id: "BPR-1341"
title: "Rename the Conversations modality to Threads"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1341--BPR-1341--84eb8fdc7d32.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1293","BPR-1295","BPR-1251","BPR-1255","BPR-1324","BPR-1304"]}
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
