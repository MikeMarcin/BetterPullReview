---
schema: "implement-rfc.card.v1"
id: "BPR-2377"
title: "Agent sessions can be renamed and deleted from the Agent tab's and tour chats' History, they live apart from the agent setup, and Settings › Storage & privacy clears any checked kinds of local data at once (Reset agents and workspace Remove among them), saying exactly what each holds"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2377--BPR-2377--a72e1e70821f.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"b71ba7908","origin":"owner request 2026-11-24 (verbatim below), amended three times the same day","related_cards":["BPR-1602","BPR-1858","BPR-2293","BPR-2345","BPR-1974","BPR-2254","BPR-1355"],"related_rfcs":["RFC-BPR-0096"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7","AC8","AC9"],"owner_step":"optional attended look (see Owner judgement)"}
acceptance: []
---

# BPR-2377: Name, delete and clear agent sessions; one checklist clears local data

## Why

Owner, 2026-11-24: "Our agent session management is weak. There's as far as I
can tell no way to set a session name or delete a session. I also can't tell
if any of our storage menu reset options clear sessions or workspaces."

Amendments the same day:

1. "I think the clear in settings menu would be better as check boxes rather
   than separate actions since I often want to clear several things at once
   but not everything"; "Clear/reset I mean... Same thing".
2. Decisions on the first slice's questions: "split sessions from settings"
   (independent boxes, a crash-safe tested migration out of
   `agent-setup.sqlite3`); fold the Agents page's **Reset agents…** and the
   BPR-2293 workspace **Remove…** into the checklist, their entry points
   opening it with the right box checked; add rename and delete to the tour
   side chat's History with the same calls and the same, reusable, session
   list.
3. Agent panel attended review, verbatim: "A but move new session into that
   menu as the top item so it doesn't take up extra space on the drawer and
   maybe put trash can and pencil on the session rows in the menu to delete
   or rename them. Put the cost and turns in a info tooltip to mouse over to
   get space for the trash and pencil and i. Rename or delete sessions can
   then go away as a menu option and you can just have delete all sessions
   or something like that." This card builds the session rows and Delete
   all; the header row (New session into the menu) is the agent panel
   redesign's.

### What existed (main 9cbc5b604)

- Agent-tab and tour sessions (RFC-BPR-0096) lived in
  `state/v1/agent-setup.sqlite3` beside the agent setup: `native_agent_*`,
  the earlier `agent_conversation*` tables, `agent_finding_cards`,
  `prompt_turn_names`. Retained workspaces sit under
  `<data root>-agent-workspaces-v1/`, owned through
  `agent-workspace-ownership-v1/`.
- History titled a session by its first question; no name, no deletion.
  RFC-BPR-0096 records only that New session and Reset tour are not deletion
  and that agent-owned transcripts stay separate from BPR's catalog.
- Troubleshooting: Clear caches, Reset settings, Reset review data, Reset
  connections, Reset everything. **Reset settings and Reset everything
  deleted every agent conversation silently** (shared store) and **left every
  retained workspace on disk with no record**. Settings › Agents held Reset
  agents… and the workspaces' Remove… as separate live actions.

## Change

1. **Core** `agent_session_management`: names are one line, at most 120
   bytes, UTF-8 without NUL; empty restores the default title (first
   question, else "New session"). Deletion is refused when not found or
   running and removes the workspace only when nothing else holds it.
2. **Core** `profile_data_reset`: a request is a selection of nine
   categories (Caches, Settings, Agent setup, Agent sessions, Agent
   workspaces, Review data, Saved repositories and servers, Insights
   statistics, Connections; everything = all). Implied: Agent sessions takes
   Agent workspaces, Settings takes Agent setup; Settings and Agent sessions
   are independent. Plan order: move sessions out of an earlier build's
   agent setup store, agent workspaces, agent setup, one journalled file
   removal, queue display, forget connections. A workspace left on disk
   holds Agent sessions back. `--reset-profile-data-scope` takes a list.
3. **SQLite**: conversations in `agent-sessions.sqlite3` (own application
   id). The move from `agent-setup.sqlite3`: stage 0 copies every row by
   shared columns in one transaction of the new store and records stage 1;
   stage 1 drops the old tables in one transaction of the old file, then
   stage 2. A stop before either commit leaves that stage to run again; the
   old file is untouched until the copy is durable. Names table; `rename`,
   `removal`, `erase` (re-points a PR workspace key), `erase_all`.
4. **Application**: `rename_agent_session`, `delete_agent_session`,
   `clear_agent_sessions`; `agent_invocation_host::rename_native_session`,
   `delete_native_session`, `set_retained_workspace_files` (Agent tab and
   tour hosts). The reset executor runs the plan, does Agent setup (the
   former live Reset agents: skill copy to Downloads, agents, clone links,
   review skill, Claude wrapper/model, which agent each PR last used) and
   Agent workspaces (exact-ownership sweep) at start-up, and reports
   per-category failure.
5. **Qt** (`frontend-design:frontend-design` loaded): Clear local data, one
   checkbox per category with one exact line, the workspaces' measured size,
   implied boxes checked and locked with why, Check all, one Clear…, one
   confirmation. `agent_session_menu` (a `QWidgetAction`, reusable by the
   agent panel redesign) is the History menu's session list in the Agent tab
   and the tour side chat: per row the title (opens it), a pencil (rename in
   place), a trash can (delete after a question naming what stays) and an i
   (turns, revision, cost, replay), with **Delete all sessions…** below
   (owner, agent panel review 2026-11-24). Delete and Delete all wait while a
   turn runs; rename does not. Reset agents… (Agents page and Agent tab
   History), workspace Remove… and the unreadable agent setup's recovery open
   Clear local data with their box checked. New glyphs: rename, trash, info.

The agents' own native session files (Claude Code's `~/.claude`, Codex's
home) are never read, moved or deleted.

## Acceptance

- AC1. A session (Agent tab or tour chat) can be named; persisted; shown in
  History, the list and the Agent tab title; empty restores the default.
- AC2. A session can be deleted after a confirmation; record, name and
  transcript go; a shared workspace stays; an unshared one is removed
  through its ownership record; a refused removal or a running session
  keeps the session. Delete all sessions… does the same for every listed
  session and counts any kept.
- AC3. Each Clear local data checkbox states exactly what it clears; the
  workspaces' size is shown; implied boxes say why.
- AC4. Several categories clear under one confirmation, in core's order; a
  partial failure names the categories that did not clear.
- AC5. Agent workspaces go first; one left keeps the sessions that record it.
- AC6. Settings and Agent sessions clear independently; an earlier build's
  conversations move out of the agent setup store, safe to stop at any point.
- AC7. Reset agents… and Remove… are categories; their entry points open the
  list with their box checked.
- AC8. Public path: the real profile desktop clears through the checkboxes;
  the real window renames and deletes a fixture-agent session.
- AC9. Format, tidy on touched units, documentation gate.

## Evidence

Revision `b71ba7908` (commits `50302bb3b`, `d8b7f3ca6`, `64cbff58c`,
`b71ba7908` on main `e5c04b655`), macOS arm64 Debug,
`ctest --test-dir build/dev -R '^<row>$'`. On `b71ba7908` itself:
`application_native_agent_sessions` (with the marker below),
`application_profile_data_reset_public_path`,
`application_review_round_trip_composition_contract` (manifest resealed over
BPR-2376, full application and mutation-branch digests unchanged),
`application_follow_up_public_path`, `sqlite_agent_session_management`,
`qt_agent_session_menu`, `qt_tour_side_chat` and `architecture_boundaries`
passed, and the format check passed on every C++ file the branch touches.
The other rows below passed on `df28b43fb`, the same code before the rebase
over BPR-2376 and the BPR-2377 renaming of comments.

- AC1, AC2 rules: `core_agent_session_management` passed.
- AC1, AC2, AC5, AC6 effects over the real catalog:
  `sqlite_agent_session_management` passed: rename persists, empty restores
  the first question; deleting one of two sessions on a workspace keeps it
  and re-points the PR key; deleting the last removes it; a held path lease,
  the window's own turn and the PR's turn lease refuse; a refused removal
  keeps the session; Delete all deletes two, keeps and counts the running
  one and removes the shared workspace once; clearing with a workspace in
  use keeps every session; clearing the chats' queued follow-ups leaves
  none; an earlier build's `agent-setup.sqlite3` conversations move out
  (old-format turns get a null `seconds`), a stop at stage 1 finishes without
  duplicates, a retried copy copies once, and removing the agent setup store
  keeps the conversations.
- AC6: `sqlite_agent_setup_storage` passed (build 63 and part-migrated cases
  on the conversations store); `sqlite_profile_data_reset_files` passed.
- AC3, AC4, AC7 rules: `core_profile_data_reset` passed (nine categories,
  implications, step order, held back, admission, list names both ways).
- AC1, AC2 presentation: `qt_agent_session_menu` passed (rows, pencil, trash,
  i with turns and cost, title opens and closes the menu, delete and Delete
  all ask first, delete waits while running, rename does not);
  `qt_tour_side_chat` passed (the tour Session menu holds the same rows;
  rename and Delete all reach its dispatch).
- AC3, AC4, AC7 presentation: `qt_profile_data_reset_views`,
  `qt_agent_setup_view`, `qt_workbench_window_scale_100` passed.
- AC8 Agent tab: `application_native_agent_sessions` passed with
  `AGENT_SESSION_MANAGEMENT:renamed:restored:deleted-shared:workspace-kept:agent-files-kept`
  (fixture agent through the real window: rename shows in the header, an
  empty name restores it, the trash can deletes a session whose workspace
  another uses, the workspace and the fixture's own backend files stay).
  `application_agent_finding_composition_contract` passed on the rows.
- AC8 Clear: `application_profile_data_reset_public_path` passed twice in a
  row (settings, caches, review data, agent sessions alone with agent setup
  the same file, caches + review data together, unacknowledged everything
  refused, startup recovery).
- `application_setup_destination_public_path` passed (UI word rules over the
  new text); `application_review_round_trip_composition_contract` passed with
  the production app manifest resealed (full application and mutation-branch
  digests unchanged); `architecture_boundaries` passed.
- AC9: format check passed on every touched file; tidy with warnings as
  errors is clean on every changed line (remaining findings at lines last
  changed by 798613f0f and 0dcfce828). `scripts/verify_documentation.py`
  cannot run in the worktree (the MASK-deployed implement-rfc `board_model`
  is absent); run it on main at landing.

## Owner judgement

- Downgrade: an earlier build opening a profile after the move finds no
  conversations in `agent-setup.sqlite3` (they are in the new store it does
  not know).
- Queued follow-ups (`agent_queues`, kept with the agent setup) go when Agent sessions is cleared; deleting one session keeps its chat's queue, which belongs to the chat (pull request, kind, thread), not to one session.
- An attended look at Clear local data and both session lists is optional;
  the Qt tests save renders.
