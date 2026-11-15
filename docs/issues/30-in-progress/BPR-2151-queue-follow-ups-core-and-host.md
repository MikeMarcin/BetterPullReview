---
schema: "implement-rfc.card.v1"
id: "BPR-2151"
title: "BPR queues follow-ups and releases them only after real completion"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC02","AC03","AC04"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2151: BPR queues follow-ups and releases them only after real completion

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Core: the bounded pending-input state of one conversation (stable entry IDs, displayed order, generation and queue revision, captured subject and attachments, Queued/Sending/Sent/Paused/Delivery unknown, limits), its legal transitions, and refusal on a fresh path (`session_mode_of`). Host: the profile-owned durable queue, the single dispatch owner and stale-revision refusal across windows, release of one next entry only after durable result handling, tour proposal processing, settled helpers and host retirement (no timer retry), and the pause rules: Stop, failure, usage limit, restart, unknown delivery, contention, changed target or configuration (not window closure: the queue is the session's, Q10); the edit hold (Q9); Stop all across sessions (AC09). Revalidation and admission before every release; nothing is retargeted. Queue maintenance makes no inference and no workspace.

## Acceptance

- **AC02:** Queue maintenance and restored paused queues produce no inference or workspace creation. Refused persistence/capacity leaves prior entries and user text intact, under the defined limits.
- **AC03:** A successful originating run dispatches known-unsent entries in displayed order only after durable application completion and host retirement, with the preceding tour proposal represented correctly. Disposable first drafts gain neither implicit promotion nor these actions.
- **AC04:** Stop, failure, usage limits, restart and unknown delivery produce the defined retained paused state; closing or switching a window neither pauses nor hands over a queue, which belongs to its session. Changed authority, revision, configuration and context cannot retarget queued input or bypass admission; competing chats retain busy refusal and there is no automatic contention retry. Every window showing a session shows and edits its one queue; stale edits are refused, and a queued message held while it is edited is never sent nor skipped, and its hold lifts on Save or Cancel.

## Notes

Owner answers 2026-11-13 (on the RFC): Q1 fold the BPR-1930 whole-tour feedback buffer into the queue (first queued message, sent with the Ask more seed; the timer poll goes); Q2 the local draft's discussion is in scope; Q9 a message being edited is held, not sent or skipped, and the queue continues on Save or Cancel; Q10 the queue belongs to the session and is drained by the host, closing or switching windows does not pause it; Stop all (AC09, host side: stop every active turn and pause every queue).

## Progress (2026-11-14, for RFC-BPR-0128's queued Ask)

The owner, refining look ask-v001: "I would modify this to a plain click
queues it. We might not have set up the agent yet or we might be running other
things already." Built as the queue's core and a first host:

- Core `bpr/core/agent_queue.hpp`: stable entry ids in displayed order, a
  revision refusing stale edits, 16 inputs and 256 KiB, the 16 KiB question
  limit, Queued/Sending/Sent/Delivery unknown, the edit hold (Q9: never sent,
  never skipped; Save or Cancel lifts it and the queue stays paused), pauses
  with reasons (reviewer, Stop, failure, usage limit, restart, unknown
  delivery, changed target), release of the next entry only while idle and
  unpaused, restore opening paused with an interrupted send uncertain.
  `core_agent_queue`.
- Host `application::agent_queue_host`: queues by session (Q10: the session's,
  not the window's), stored whole per session in `agent-setup.sqlite3`
  (`agent_queues`, `agent_queue_entries`), restored paused; releases one entry
  when told the agent is idle and settles it when its turn has run and the run
  retired. The window's Ask uses it (`application_ask_queue_public_path`:
  queued before setup and run after; a second waits its turn and runs next).

Not yet: the composer's Queue/Steer, the pending list's Edit/Remove/reorder UI,
Pause/Resume controls, Stop all (AC09), tour proposals, revalidation of private
selections at release, the multi-window stale-write path through a shared
store, and AC02 to AC04's full evidence. The card stays In Progress.

## Progress (2026-11-14): the queue's controls

Following the owner's answers on RFC-BPR-0126 (Q4/Q5, Q9 refined, Q10,
AC09):

- **Q9 as refined** (`1441cd92c`, `core_agent_queue`): an edited queued
  message is held without pausing the queue; nothing is sent past it; Save or
  Cancel lets it continue unless paused for another reason.
- **Stop all** (`153a597cc`, `qt_agent_footer`, AC09): one Stop all in the
  agent footer beside the picker, in a fixed slot shown while any turn runs or
  any message waits; it stops every run the footer shows and pauses every
  session's queue (`agent_queue_host::pause_all`).
- **Follow-ups and the pending list** (`application_ask_queue_public_path`):
  while a turn runs or messages wait, a typed message is queued on the session
  (Send says Queue; Cmd+Return, Ctrl+Return off macOS, the other for one
  message). Steer is not built ([[BPR-2153]]), so a steered message is queued
  and the Agent tab says Steer isn't available (Q17). The pending list shows
  each waiting message with Edit (a typed one, in place, held while edited),
  Move up, Move down and Remove, the queue's state (why it is paused) and
  Pause queue or Resume queue; on Resume they run in the list's order. A
  typed message keeps the conversation it was asked about.
- **The setting** (`sqlite_agent_setup_storage`, `application_agent_setup_host`):
  Settings > Agents & context, Follow-up behaviour: Queue (the default) or
  Steer, with "Press ⌘Return to do the other for one message"; kept with the
  profile.

Not yet: the lines a typed follow-up was asked about (its file and
conversation are kept, its line selection is not), Steer itself (BPR-2153),
tour proposals, the multi-window store sharing (the queue host is still one a
window, keyed by session), and AC02 to AC04's full evidence.

## Look prompts-v001 handed back (owner, 2026-11-14)

Accepted, no note: `BPR-2151-edit-hold`, `BPR-2151-follow-up-setting`.

Accepted with a note, `BPR-2151-stop-all`, verbatim: "Yes, thinking for would
be a good time to track while it's active." Built: beside Stop all, "Thinking
for 12s" (then "1m 05s") while a turn runs (`qt_agent_footer`).

Rejected, `BPR-2151-cmd-return`, verbatim: "I think we can make it look a
little better, how about drag to reorder instead of up/down erros, how about a
trash can to delete and a pencil to edit. Also I'm not able to test this with
the current agent fixture... we need a message that the fake agent takes
enough time on for me to queue stuff."

Rejected, `BPR-2151-pending-list`, verbatim: "I think it can look a little
better, like some defined borders maybe. I dunno send this back to a frontend
desgin worker."

Built (frontend-design loaded; the leaf implementer did the restyle, as it may
not start a worker): the pending list is a bordered panel (head: Waiting, the
count, the queue's state, Pause or Resume) of bordered cards; a card is
dragged by its grip to reorder (a line shows where it lands), Option-Up and
Option-Down move it, Delete removes it; a pencil edits a typed message and a
trash can removes it; a sending message is dimmed and says Sending. The
fixture agent takes about twenty seconds, a read a second, on any question
holding "Take your time".
