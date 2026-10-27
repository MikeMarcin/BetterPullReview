---
schema: "implement-rfc.card.v1"
id: "BPR-1781"
title: "Overview gets the conversation drawer: Comments, Notes, Tour and Agent, with the pull request's conversation on its Comments tab"
status: "Done"
rfcs: []
depends_on: ["BPR-1779"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation": "docs/issues/_reservations/BPR-1781--BPR-1781--4d335607ec69.json", "kind": "feature", "found_by": "owner chat, 2026-10-27", "related_rfcs": ["RFC-BPR-0085", "RFC-BPR-0039", "RFC-BPR-0040"], "related_cards": ["BPR-1588"], "frontend_design": "frontend-design:frontend-design loaded before the Qt surfaces"}
acceptance: [{"criterion": "OD01 six tabs on Overview", "outcome": "pass", "revision": "f67ba1ffd", "evidence": {"kind": "command", "command": "ctest -R '^(qt_drawer_parts_scale_(100|125|200)|qt_workbench_overview|application_commit_filter_public_path)$'", "detail": "drawer_parts test_overview_drawer_has_the_conversation_tabs: Status, Commits, Comments, Notes, Tour, Agent; Comments, Notes and Agent are holders; show_tab expands a collapsed drawer; a hidden Tour tab is not opened and hiding it while open goes to Status; at 340 pixels the row fits with the open tab named and the others as glyphs, each glyph-only tab's tooltip naming it; the principles hold. The window course and the composed Commits course see six tabs."}}, {"criterion": "OD02 Overview Comments is the pull request's conversation with its composer", "outcome": "pass", "revision": "f67ba1ffd", "evidence": {"kind": "command", "command": "ctest -R '^(qt_workbench_overview|qt_github_conversation_panel|core_review_submission|application_conversation_read_public_path)$'", "detail": "Window: Overview's Comments holds the one conversation panel in the pull request view (comments in time order, a comment opens in place) with the new-comment composer. Panel: the first words open a private draft, edits are kept, only Comment sends, another draft's presentation leaves its words, posting clears it. Core: a new_pull_request_comment draft renders as its text alone and never carries a quote. Composed: after the Review link the quoted reply is written and sent on Overview; then a new comment is saved before Comment is enabled, typing posts nothing, Comment posts it once, and the fixture server receives the words alone."}}, {"criterion": "OD03 Review Comments has no pull request mode; its link opens Overview's Comments", "outcome": "pass", "revision": "f67ba1ffd", "evidence": {"kind": "command", "command": "ctest -R '^(qt_conversation_cards_view|qt_drawer_surfaces_fit_scale_(100|125|200)|qt_workbench_overview|application_conversation_read_public_path)$'", "detail": "The chip that flipped Review's drawer into the pull request view is one quiet 'Pull request conversation' link (count of new comments in its accessible name), offered only with a window to go to; it and opening a pull request comment beside the code ask the window, which opens Overview's Comments; there is no Back to threads at their lines. Next new comment on a pathless comment goes there too. Discuss's own Pull request | File scope is unchanged (coordinator decision: Discuss modality unchanged)."}}, {"criterion": "OD04 Notes and Agent are the same objects, state kept across switching", "outcome": "pass", "revision": "f67ba1ffd", "evidence": {"kind": "command", "command": "ctest -R '^(qt_workbench_overview|qt_workbench_window_scale_(100|125|200)|application_agent_chat_.*)$'", "detail": "test_overview_drawer_borrows_the_conversation_tabs: the window's private_notes(), agent_tab() and conversation_panel() are one object each, in Overview's holders on Overview and Review's tray in Review; an open note stays open across the switch; the conversation's open card is open again when the link returns; on Overview N writes a note on the pull request and Ctrl+I opens Overview's Agent tab and stays on Overview. Composed agent chat courses pass; the conversation course checks its asked-conversation chip in the question because the Agent tab now really shows on Overview and its first showing may put the setup in front."}}, {"criterion": "OD05 Tour tab cases and routes", "outcome": "pass", "revision": "f67ba1ffd", "evidence": {"kind": "command", "command": "ctest -R '^(core_overview_tour|qt_drawer_parts_scale_100|qt_workbench_overview|application_review_tour_cold_import_public_path)$'", "detail": "core decide_overview_tour: no tour and not the author hides it; the author with none gets Prepare; a draft I can edit is a writer's (private or published) with every stop and readiness; a published tour on offer or imported read-only is a reader's; my own published tour not yet loaded is Prepare. Qt summary: Prepare review tour, Continue in Review and Start tour report gestures only; writer readiness words; reader titles only. Window: reader offer shows Start tour (to the composition); the tab goes when the offer goes and not the author; the author with none sees Prepare; the author's draft shows its stops and Continue in Review opens Review's Tour tab at its stop. Composed (cold import): the imported own route shows as mine, published, three stops; Continue in Review opens Review's Tour tab at the stop it was on; Start tour opens Review at the first stop and navigates; the tour is unchanged and no remote write is made."}}, {"criterion": "OD06 drawer principles and renders", "outcome": "pass", "revision": "f67ba1ffd", "evidence": {"kind": "command", "command": "BPR_DRAWER_FIT_RENDER_ROOT=DIR drawer_surfaces_fit_tests; BPR_DRAWER_RENDER_ROOT=DIR drawer_parts_tests; BPR_CONVERSATION_RENDER_ROOT=DIR conversation_read_public_path", "detail": "test_the_overview_conversation_tabs_fit: Overview's Comments, Notes, Tour (author with draft, author with none, reader) and Agent, whole in light, dark and both high-contrast themes at 340 and 560 pixels, header and Tour summary audited for the principles. Renders kept in the session scratchpad (not committed)."}}, {"criterion": "OD07 owner look", "outcome": "pending", "revision": "f67ba1ffd", "evidence": {"kind": "observation", "detail": "Deferred by the owner; not a blocker. No attended-review session was opened."}}, {"criterion": "OD07", "outcome": "pass", "revision": "aaa24c11c865068cc24b918dd9e4e7f7d61b2c99", "evidence": {"kind": "observation", "detail": "Attended review session 41f51349bc37e6c092d31dbf1739c5b1 (state ~/.attended-review-state/bpr-looks-2026-10-27, served over Tailscale, handed back 2026-10-27T21:11Z): candidate BPR-1781-drawer, -link, -composer, -tour-tab, owner verdict accept."}}]
---

# BPR-1781: Overview gets the conversation drawer

## Request

The owner, 2026-10-27, verbatim:

> Why isn't the tour tab on overview for an authored PR, or possibly any PR

and, on the proposal of a Tour tab: "Yeah do that". Then, widening it:

> Honestly comments, notes and agents probably also make sense on overview.
> Probably makes more sense than how we did the poll request comments tucked
> away as a mode in the top of file"s comments.

Revision r2 (2026-10-27) widens the card from a Tour tab to the whole
drawer. The Tour tab outcome below is kept as one of the four tabs.

## Grounded current behavior

- The Tour tab is the third tab of Review's right drawer (Comments, Notes,
  Tour, Agent; `workbench_window.cpp`, `tray_tour_tab`), by RFC-BPR-0085 D4:
  a tour is written and followed beside the code, whose lines wash amber.
- Overview's right side is its own drawer, **Status | Commits**
  (`overview_status_drawer.cpp`). The only tour control there is
  [[BPR-1588]]'s small **Prepare review tour** icon in that drawer's tab row
  (`overviewPrepareTour`), following `PrepareTourAction`, which switches to
  Review and opens the Tour tab; it is disabled until the composition
  enables it.
- So Overview says nothing about whether an authored pull request has a tour
  draft or how far along it is, and a reader sees no sign of a published
  tour.
- The pull request's own conversation (pull-request comments and reviews) is
  a mode of Review's Comments tab: a **Pull request | File** switch at its
  top (`github_conversation_panel.cpp`, `ScopeWholePr`,
  `cards_scope::whole_pull_request`). Overview, where a reviewer reads the
  description and decides how to start, shows none of it; notes and the
  agent are not on Overview either.

## Outcome

Overview's right drawer holds the same conversation tabs Review does,
**Comments, Notes, Tour, Agent**, beside Status and Commits (whether Status
and Commits stay tabs of the same drawer or a separate column is the look's
question; one drawer with six tabs is the starting proposal).

- **Comments on Overview is the pull request's conversation:** pull-request
  comments and reviews, shown comments-first as [[BPR-1779]] defines, with
  the composer for a new pull-request comment. It moves here from Review.
- **Review's Comments tab is about code:** the threads of the file shown, at
  their lines. The **Pull request | File** switch goes; in its place one quiet
  link, "Pull request conversation", opens Overview's Comments tab. (Keeping a
  whole-pull-request mode in Review is the alternative for the look.)
- **Notes on Overview:** the pull-request notes (not on a line), with New
  note; line notes stay with the code in Review, and the list can show them
  all with their file and line.
- **Agent on Overview:** the same Agent tab and the same conversation per
  pull request and agent ([[BPR-1753]]); a question asked on Overview has the
  pull request as its scope, not a selection.
- **Tour on Overview** as below.
- One instance of each surface's state: a note, a draft comment or an agent
  turn is the same object whichever window part shows it; switching between
  Overview and Review keeps each tab's place.

### The Tour tab

- **My pull request:** the brief (Problem, Solution) and the stops in order,
  each with its readiness (Ready, Editing, Needs attention, Suggested) as the
  route shows it; whether the tour is a private draft or published; and
  **Continue in Review**, which opens Review's Tour tab at the current stop.
  With no tour yet, **Prepare review tour** as a plain labelled action (the
  icon goes, or stays only as a shortcut).
- **Any pull request with a published tour:** the brief and the stop titles,
  read-only, with **Start tour**, which opens Review at the first stop as
  following a tour does today.
- **No tour, and not the author:** no tab.
- Writing and following stay in Review beside the code; this tab presents and
  routes, and performs no effect (the composition decides every gesture).
- Uses the drawer kit (RFC-BPR-0085) and the shared buttons (RFC-BPR-0087);
  load `frontend-design:frontend-design` first.

## Acceptance (draft)

- Overview shows Comments, Notes, Tour and Agent; Overview's Comments holds
  the pull-request conversation with its composer; Review's Comments has no
  Pull request mode and links to Overview's; Notes and Agent show the same
  objects in both places (Qt and composed tests).
- Tour: the tab exists for an author (with and without a draft) and for a
  published tour, and not otherwise; Continue in Review and Start tour open
  Review's Tour tab at the right stop; nothing is published or changed.
- Offscreen renders, light and dark: author with a draft, author with none,
  reader of a published tour.
- The owner accepts the look on a frozen build (attended review).

## Decisions (coordinator, 2026-10-27, not reopened)

- One drawer, six tabs. Existing widgets are reused, not duplicated.
- Overview's Comments is the pull request's conversation with a composer for
  a new pull request comment; Review's Comments is the file's threads with one
  quiet link to it. Discuss is unchanged.
- Notes on Overview are the pull request's notes; Agent is the same tab and
  conversation, asked about the pull request.
- The Tour tab presents and routes only.

## Built (2026-10-27)

- Core: `bpr/core/overview_tour.hpp` (`decide_overview_tour`);
  `conversation_reply.hpp` gains the `new_pull_request_comment` key, a draft
  that answers no post and renders as its text alone.
- Application: `tour_presentation::Offered`, the first published tour on
  offer for import, so a reader sees its brief and stops before importing.
- Qt: `overview_status_drawer` has six tabs and holders, fitted with the tray's
  tab bar (moved to the shared `drawer_tab_bar.hpp`); the tab-row Prepare
  review tour icon is gone. `overview_tour_summary` is the Tour tab. The
  conversation cards' pull request chip is the quiet link, and each mode keeps
  its open card and scroll. `private_notes_panel::set_pull_request_only`
  (the list of notes on the pull request, N for a new one, no way back to
  lines). `review_tour_editor::go_to_stop` and a presented observer.
  `github_conversation_panel` has the new-comment composer.
- Re-parenting: the window owns one conversation panel, one notes panel and
  one agent page (now a body in a home widget). `lend_conversation_tabs`, run
  from `apply_modality_surface`, moves the three into Overview's holders
  whenever Overview is the page shown and back into Review's tray otherwise,
  as Discuss already borrows the conversation panel; Discuss still takes the
  panel first when it shows. The same objects move, so drafts, open notes and
  agent turns are kept.
- Composition: Start tour opens a tour already in the window at its first
  stop, or imports the offered one through the tour host's `import_selected`
  and opens it when it arrives; on Overview the agent's suggestions name no
  file or lines; the conversation-reply workflow says what Comment does for a
  new comment. Composition digests resealed.
- Docs: `docs/ux/workbench-specification.md`, the Overview drawer and the
  Conversation drawer.

## Look questions (starting proposals built)

1. Six tabs in one drawer, or Status and Commits apart from the
   conversation tabs? Built: one drawer, six tabs, compact glyphs when narrow.
2. Should Overview's drawer open wider than 340 pixels when a conversation tab
   is open? Built: one width for all tabs, as now.
3. Review's Comments: the quiet "Pull request conversation" link, or keep a
   pull request mode in Review? Built: the link only, with the new-comment
   count in its accessible name and badge.
4. The new comment's composer: always under the conversation, or opened by a
   "New comment" action? Built: always there, four lines, Comment at its foot.
5. The Tour tab: the route of numbered rings with readiness words for a
   writer, plain rings for a reader, one button. Built as described.
6. Prepare review tour: the labelled action in the Tour tab only (the icon on
   the tab row is gone; Alt+T stays). Built.
7. Overview's Notes: the list of notes on the pull request, whole files and
   other files, with New note; line notes stay in Review. Built.

## Known gaps

- Start tour's import-then-open path (a published tour not yet in the window)
  is covered by the window and summary courses and the core decision; the
  composed course runs Start tour on an imported tour only, because the
  fixture's published tour is the viewer's own.
- The Agent tab now really shows on Overview, so its first showing may run
  the agent setup's detection there (it did not when the tray stayed shut).
- The owner's look is pending (deferred).

