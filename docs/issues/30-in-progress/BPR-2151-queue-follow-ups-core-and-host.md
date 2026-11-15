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

Look `prompts-v002` filed (2026-11-14): session `a91caa31b1f548e1c54a22f0dc1350ec`, state `/Users/jaeger/.attended-review-state/prompts-v002-2026-11-14/state` (URL in `serve.log` beside it), cursor 15; root `/Users/jaeger/prompts-v002.bkog9vts` from `d92a6426e` (tests-on, Fixture agent, `--attended-prompt-examples`; its check answered all 17 asks). Five questions: `BPR-2179-nested-conditions`, `BPR-2151-queue-list-look`, `BPR-2151-drag-trash-pencil`, `BPR-2151-slow-fixture-ask` (type "Take your time"), `BPR-2151-thinking-for`.

## Look prompts-v002 handed back (owner, 2026-11-14)

Accepted: `BPR-2151-drag-trash-pencil`, `BPR-2151-slow-fixture-ask`.

Accepted with a note, `BPR-2151-queue-list-look`, verbatim: "It looks good and
works good except for the drawer is back to having a horizontal
panning/scrolling. That needs to get fixed again." Fixed: the pending list's
words wrap (a message, its state), so no card is wider than the drawer;
`qt_drawer_surfaces_fit` now shows the Agent tab with a pending list of long
messages, one being edited and a long state, at every width it checks, and
fails if the drawer clips or the Agent tab's scroll content is wider than its
view.

Rejected, `BPR-2151-thinking-for`, verbatim: "I don't think we need 2
different stops and 2 different times. Just make the left one into stop all
since it looks better. Stopping an individual task is already easy to do from
the composer." Built: one stop and one time. While one run shows, its stop
button (the one with the red square, beside its time) is Stop all; the
separate Stop all and "Thinking for" are gone. While several run, or only
messages wait, the one Stop all shows in its place.

Look `prompts-v003` filed (2026-11-14): session `2b1c044e69cb4b2740b13a50ad6c5915`, state `/Users/jaeger/.attended-review-state/prompts-v003-2026-11-14/state` (URL in `serve.log` beside it), cursor 9; root `/Users/jaeger/prompts-v003.dpoz1ap3` from `37f9eabf1` (tests-on, Fixture agent, `--attended-prompt-examples`; its check answered all 17 asks). Three questions: `BPR-2151-one-stop-one-time`, `BPR-2179-nested-conditions`, `BPR-2151-drawer-no-sideways`.

## Look prompts-v003 handed back (owner, 2026-11-14)

`BPR-2151-drawer-no-sideways`, feedback with no verdict, verbatim: "Drawer
doesn't scroll horizontally anymore which is good. However: Long messages
should not grow to multi-line they should just end with ellipsis. Queued items
need the ability to say steer instead;" (his pictures: the pending list, and
Codex's queued message with Steer, a trash can and a menu: Edit message, Open
in side chat, Turn on queueing).

Built: a queued message is one line, ending with an ellipsis; its tooltip and
the editor hold it whole. Each waiting card has Steer. No agent steers a
running turn yet ([[BPR-2153]], Backlog: native correction on Claude's stream
and Codex's App Server, with installed-agent qualification), so Steer moves the
message to go next, right after the current answer, and says so (as a typed
steer does, Q17); when BPR-2153 lands, the same action steers natively.

Rejected, `BPR-2151-one-stop-one-time`, verbatim: "Why would you have
different buttons for one running and 2 running. Just have the same button
with the stop icon for everything." Built: one control, the stop-icon chip
named Stop all, the same with one turn running, several, or only messages
waiting.

Look `prompts-v004` filed (2026-11-14): session `8a2a32701c0cb2bdeed967bb7585124e`, state `/Users/jaeger/.attended-review-state/prompts-v004-2026-11-14/state` (URL in `serve.log` beside it), cursor 15; root `/Users/jaeger/prompts-v004.mooeae56` from `7d17b1122` (tests-on, Fixture agent, `--attended-prompt-examples`; its check answered all 17 asks). Five questions: `BPR-2151-queue-one-line`, `BPR-2151-queue-steer`, `BPR-2151-one-stop-all`, `BPR-2179-choice-styling`, `BPR-2179-hide-or-disable`.

## Look prompts-v004 handed back (owner, 2026-11-14)

Accepted: `BPR-2151-queue-steer`, `BPR-2151-one-stop-all`.

Accepted with a note, `BPR-2151-queue-one-line`, verbatim: "I feel like
there's a bit of wasted space around these icons we could claw back for more
message width. Also I like your steer icon I don't think we need the word.
Just add tooltips for each of the icons. Otherwise looks good." Applied with
no new look (the item is accepted): the pencil, Steer and trash are icons
alone, 22 points, with no gap between them, each with a short tooltip (Edit;
Steer: send it into the running turn instead of waiting; Remove from the
queue) and its message in its accessible name; Sending keeps a small gap.
`qt_drawer_surfaces_fit_scale_100/125/200` now also checks that each of the
three is an icon with a tooltip no wider than 24 points;
`application_ask_queue_public_path` passes.

Accepted so far on this card's user-facing part: the pending list (look,
borders, one line, drag, pencil, trash, Steer that goes next), the edit hold,
Follow-up behaviour, one Stop all, and the fixture agent's slow ask.

Not closed: AC03's tour proposal ("with the preceding tour proposal
represented correctly") and AC04's "every window showing a session shows and
edits its one queue" across windows (the queue host is still one a window,
keyed by session) have no evidence yet, and AC02 to AC04 have not been
evidenced as a set. Native steering stays BPR-2153's (not in this card's
acceptance). The card stays In Progress for those.

## AC03 and AC04 built (2026-11-14)

The coordinator, relaying the owner's override of Q10 ("Queue must be tied
to session not active UI windows"): build AC03 and AC04 rather than split
them out.

- **AC04, one queue a session across windows** (`d8f7fbf4b`). The queue host is the
  profile's (`github_profile_runtime::AgentQueue`), made by the first window
  and outliving every window. Each window joins it as a viewer of the
  sessions it shows (`application::agent_queue_viewer`: shows, what its agent
  can take, how it runs a released entry, what it shows). Every window
  showing a session is shown each change; an edit is made from the revision
  the window last showed, so an edit that crossed another window's change is
  refused ("Another window changed this queue first, so nothing changed
  here."); a message one window edits is held in both, shown "(being
  edited)" with no controls in the other, and cannot be edited or removed
  twice (`core::hold` and `core::withdraw` refuse a held entry). The host
  drains a session through the first free window showing it, and starts
  nothing while a turn of that conversation runs in any window. The window
  that ran an entry settles it once its run retired. Closing a window
  neither pauses nor hands over a queue: a turn it ran ended with it after
  its message went, so that entry settles as sent, and another window
  showing the session runs the rest; a queue no window shows waits,
  unpaused, until one does. `application_agent_queue_host` (two windows,
  stale and held edits, the busy window, closing the dispatcher, a queue no
  window shows, a refused start, a restored queue, a refused write, the
  limit); `application_agent_queue_windows_public_path` (two desktop windows
  on returning-review-v1: queued in one, shown in the other in order; edited
  in the second, held in the first with no controls; a removal crossing the
  other window's press refused and said; typed in the second, shown in the
  first; Resume, the first window runs the next and is closed while it runs;
  the second runs the rest in order, the queue never paused).
- **AC03, the tour proposal (Q1).** Whole-tour feedback typed while Draft the
  tour runs is the whole-tour conversation's first queued message (session
  `tour-feedback:` and the pull request); more feedback before the draft is
  ready joins it. It is released on the draft's real completion: the answer
  handled, the proposal made and the tour host retired (the host's idle
  observation releases it; `send_waiting_tour_feedback`'s 50-try timer poll
  is gone). It goes as one turn with the draft's answer as the Ask more seed
  and the proposal in the request, so it revises that proposal; the tour chat
  shows it waiting where it showed the buffer. A draft that did not propose
  a tour returns it to the chat as not sent, as before; one whose draft ended
  with its window or the app returns the same way when the tour shows again.
  The disposable first draft itself is never queued or promoted.
  `application_tour_feedback_queue_public_path` (two feedbacks while the
  slow stub drafts: nothing sent while it drafts, one waiting message; after
  the draft, one turn with both as written, revising the proposal and
  started from the draft's answer; no turn refused for a run still
  finishing).
- **AC02.** `application_agent_queue_host` (a restored queue shown paused
  and starting nothing; a store refusing a write keeps every message as
  typed; past 16 nothing is added) with `core_agent_queue` and
  `application_ask_queue_public_path` (nothing runs while paused).

AC04 changes what the owner sees (a second window shows and edits the same
queue), so it waits on a look; AC03 does not (the tour chat's waiting
feedback, its notice and its turn look as before). Look `queue-windows-v001`
filed (2026-11-14): session `74fcc4b08cdfbc6fc8d13b39a1040715`, state
`/Users/jaeger/.attended-review-state/queue-windows-v001-2026-11-14/state`
(URL in `serve.log` beside it), cursor 3; root
`/Users/jaeger/queue-windows-v001.uqq1jqr3` from `d8f7fbf4b` (tests-on,
Fixture agent, at the next push, `--attended-queue-windows`; its check
answered all 17 asks). One question: `BPR-2151-queue-across-windows`.

Found while building the course, not in this card: at the next push
(returning-review-v1's reuse phase), a run in the first window that ends
while a second window is still loading the same pull request never retires:
its workspace cleanup is refused (`cleanup_failed`) and the job stays
active, so the queue there waits. With the second window loaded first, the
course passes at the next push as well. The look asks for the second window
first.
