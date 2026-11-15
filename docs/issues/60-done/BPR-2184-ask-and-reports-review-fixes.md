---
schema: "implement-rfc.card.v1"
id: "BPR-2184"
title: "Ask and reports: the critical review's fixes"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2168"]
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2184--BPR-2184--241b88a88736.json","origin":"critical review of Ask and reports at 5260527d6, relayed by the coordinator 2026-11-14"}
acceptance: [{"criterion": "Each item has its course; architecture_boundaries is green (B1, N1-N3, N6-N8, N10 recorded in Progress, 2026-11-14)", "outcome": "pass", "revision": "c0bdc7297", "evidence": {"kind": "command", "command": "scratch worktree of the change at c0bdc7297's tree, built from nothing: ctest -R 'architecture_boundaries|core_returning_review|application_ask_addressed_public_path|application_ask_menu_public_path|qt_agent_report_view'", "detail": "All pass. N4: quick_ask_facts takes Changed and CommitsSince from core::thread_baseline and core::thread_progress_since. N5: reports keyed by the Agent tab turn's stable serial (agent_report_document::Turn), the byline as data (agent_report_byline), a report dropped once its tab closes; qt_agent_report_view says the byline from its parts and application_ask_menu_public_path checks the report names its turn by identity and its byline is that turn's ending."}}]
---

# BPR-2184: Ask and reports: the critical review's fixes

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md),
after its close by [[BPR-2168]].

## Scope

- **B1 (blocking):** a wide Was this addressed? row counts every changed
  thread, but a press keeps only 32 and only those the window holds. Core
  applies the cap and returns the exact identities and count; the press sends
  exactly those. Test with 33 or more changed threads.
- **N1:** an agent-written `bpr-draft:` link triggers Add as draft comment.
  Actions leave the hrefs. Test that a hostile anchor drafts nothing.
- **N2:** the architecture gate gathers every violation before it fails;
  QTextBrowser and rich-text setters outside the denying document join the Qt
  rules, Qt file APIs the Qt effect rules; the fixture agent is pinned to
  BUILD_TESTING where there is a clean place. It passes at HEAD and catches a
  seeded QTextBrowser.
- **N3:** a merge's commit count is asked for when What changed is composed,
  with the bound decided in core, not inside every pull request read.
- **N4:** the Q6 baseline choice moves into core (the window's call is the
  queue implementer's to switch).
- **N5:** a report is a host-owned record keyed by a stable turn identity, not
  Qt content keyed by a turn index.
- **N6:** the report page's derived colours (dim, faint, strong, link) are
  derived in core and held to the contrast floor.
- **N7:** the commit listing is given the pull request's whole commit set
  apart from the slice it lists.
- **N8, N10:** the report view's tests feed hostile hrefs and follow links by
  click and Return; Tab reaches the page's links.

## Acceptance

Each item above has its course; `architecture_boundaries` is green. Nothing
the owner sees changes except the faint text's contrast, so no look.

## Progress (2026-11-14)

- **B1:** core caps a wide Was this addressed? row at `maximum_addressed_threads`
  (the first changed threads in the pull request's order). `Each` and `Count`
  are exactly what a press sends, and `Omitted` says how many more were left
  out. The press sends exactly those threads. If the window no longer holds
  one of them, it refuses and sends nothing, rather than sending fewer.
  `core_quick_ask` tests 40 threads, 36 of them changed.
- **N1, N8:** Add as draft comment's link is a random token minted per render
  and held by the view. A link the report's text writes, the old
  `bpr-draft:` scheme or a lookalike token included, is only ever followed as
  a link. `qt_agent_report_view` clicks hostile anchors and checks they draft
  nothing, then checks the minted link drafts its own item. It also checks
  that a page written with no minted links offers no action.
- **N10:** Tab and Shift+Tab walk the page's links, and Return follows the
  link reached. A click follows a place by its text. Both are tested by key
  and mouse events, not `follow()`.
- **N2:** the gate records every violation and fails at the end, naming
  them all. New rules:
  - No `QTextBrowser` in the Qt adapter. The tour transcript moved to a
    read-only reader over the resource-denying document, and the window's
    focus check now reads a `bprReader` property.
  - Rich text is set only in files that read over that document; the
    Markdown import helper is the one named exception.
  - The Qt adapter makes no filesystem effect: no `QFile`, no `QDir` or
    `QFileInfo` object, no `std::filesystem`, no `fopen`. Path-text helpers
    and file dialogs are allowed.
  - The fixture agent is defined only inside `if(BUILD_TESTING)`.
  Checked on an export of HEAD with these changes, the gate passes. With a
  seeded `QTextBrowser` include and a seeded `QFile`, it fails and names both.
- **N3:** a pull request read no longer counts merges. The port gains
  `count_merges`, which the provider answers with one compare per merge,
  remembered for the session, on the account's own transport only.
  - The window read host asks it on the last read's authority and answers on
    the decision lane.
  - Core's `merges_to_count` picks the merges: the newest first, at most
    eight.
  - What changed asks for them when its listing is first made for a head,
    and lists again when the counts arrive.
  - `github_provider` checks that the read asks nothing, that a count is
    asked once and kept, and that a failed compare leaves the count unknown.
- **N4 (core side):** `core::thread_baseline` and `core::thread_progress_since`
  decide Q6. The returning review's thread offers use them. The queue
  implementer owns `window_quick_ask.cpp`'s `quick_ask_facts`; it must build
  each thread's `Changed` and `CommitsSince` from the same two calls, so the
  menu and the press agree.
- **N6:** core derives the report's dim, faint and strong shades, its rule
  and its link from the paper and ink (`core::derive_report_shades`), and
  `core::measure_report_shades` holds them to their floors. The faint shade
  in Light is now at the text floor (it was 3.84:1). Every built-in theme is
  checked in `qt_appearance_profile`.
- **N7:** `fold_commit_listing` takes the pull request's whole commit set
  apart from the slice it lists.
- **N4 (window side, BPR-2181 fourth round, 2026-11-15):**
  `window_quick_ask.cpp`'s `quick_ask_facts` builds each thread's `Changed`
  and `CommitsSince` from `core::thread_baseline` over the thread's comments
  and `core::thread_progress_since` over the pull request's commits, the
  calls the returning review's offers make, so the menu and the press agree.
  The window's own first-comment baseline is gone. Courses:
  `application_ask_menu_public_path`, `application_ask_addressed_public_path`
  and `core_returning_review`.
- **N5 (BPR-2181 fourth round, 2026-11-15):** a report is keyed by its
  turn's stable identity, the Agent tab turn's serial
  (`agent_report_document::Turn`), never its place in the list. A thread
  drawn again keeps each turn's identity in its place, so a report opened
  earlier still finds its turn; Show in conversation asks the tab where that
  turn is now (`agent_tab_view::turn_of`), and the card's selected row
  follows the identity. The byline is data
  (`agent_report_byline`: the turn's ending as its footer first said it,
  when, and the head), joined by the report view; nothing parses the
  footer's label. The window drops a report once its tab closes (the last
  tab projection showed it and the new one does not; a report just opened
  stays until its tab shows). Courses: `qt_agent_report_view` (the byline
  said from its parts) and `application_ask_menu_public_path` (the report
  names its turn by identity, which the tab finds, and its byline is that
  turn's own ending).
