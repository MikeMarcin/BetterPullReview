---
schema: "implement-rfc.card.v1"
id: "BPR-2154"
title: "The composer queues and steers, and shows every pending, sent and uncertain input"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2151", "BPR-2153"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC01","AC08","AC09"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2154: The composer queues and steers, and shows every pending, sent and uncertain input

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first. Follow-up behaviour as a Settings choice, Queue or Steer, default Queue, and Cmd+Return (Ctrl+Return off macOS) for the opposite once; Send labelled by what it will do while the conversation runs or its queue is not empty; Stop always reachable; the compact pending list with in-place Edit (held while edited), Remove and reorder (also by keyboard), Pause queue and Resume queue; Stop all in the status bar's agent footer; states in words with one polite announcement per action; projections keep the draft, focus, scroll and selection; existing palette, accessibility and agent-accent rules. Rendered offscreen and driven through the composed application.

## Acceptance

- **AC01:** Every in-scope chat accepts and displays multiple busy follow-ups with the defined Queue/Steer distinction, keyboard behavior and unsent editing controls; projections preserve the composer and captured subject.
- **AC08:** The composed Qt flow exposes pending, paused, sent and uncertain states and recovery with reachable Stop and keyboard/accessibility actions, while retaining the existing proposal-acceptance and publication boundaries.
- **AC09:** One Stop all control in the status bar's agent footer, beside the agent picker, in a fixed slot shown while any agent turn runs or any message is queued, stops every active turn across sessions and pauses every queue, so nothing starts next; it is reachable by keyboard and named to assistive technology.

## Notes

Owner answers 2026-11-13 (on the RFC): Q4/Q5 follow-up behaviour is a Queue or Steer setting, default Queue, with Cmd+Return doing the opposite for one message (Codex's Follow-up behavior setting as inspiration); Q9 queued messages are edited in place and held while edited; Stop all in the agent footer (AC09). An owner look on a frozen build is not an acceptance criterion of the RFC as drafted; a look can be filed and not waited on.

## Audit (2026-11-15, coordinator assignment after BPR-2152)

frontend-design was loaded for this card's Qt work.

Built and accepted already, under [[BPR-2151]] and [[BPR-2181]], for the
Agent tab (the pull request's chat): Follow-up behaviour (Queue or Steer,
default Queue, Cmd+Return the other once); Send labelled Queue while a turn
runs or messages wait; Stop reachable; the bordered pending list with one
line a message, in-place Edit held while edited, drag and Option-Up/Down to
reorder, Delete and the trash to remove, Steer on each waiting card, Pause
queue and Resume queue; Delivery unknown with Discard; Stop all in the
agent footer's fixed slot, named and focusable (looks prompts-v001 to v004,
queue-windows-v001, steer-v002; courses `application_ask_queue_public_path`,
`application_agent_queue_windows_public_path`, `qt_agent_footer`,
`application_review_agent_runtime` `test_stop_all_is_the_hosts`).

Added this round with [[BPR-2152]]: a steered message the agent's runtime
took shows **Sent to agent** in the pending list, with no controls, until its
run settles it (`application_agent_steer_sent_public_path`). Look
`queue-sent-v001` (one question) filed for it.

Not built, so AC01 ("every in-scope chat") is not met:

- **Tour side chat and the whole-tour conversation.** Send is disabled while
  a tour turn runs (`tour_side_chat::prompts_usable`, `Send->setEnabled`),
  except whole-tour feedback during the first draft, which is the tour
  conversation's first queued message (BPR-2181 AC11). No pending list, Queue,
  Steer or Stop all wiring for a later busy turn of a stop, problem,
  solution or whole-tour thread. Each tour part is its own queue ("tour parts
  never share inputs"), so this needs a `queued_session_kind` per tour part,
  a turn runner for the window's (or a profile runtime's) tour agent, Steer
  through the tour run's invocation host, and the pending list in the tour
  chat.
- **The local draft's discussion (Q2).** No queue at all: its runs are the
  local draft workbench's (`local_draft_workbench`, path
  `local_draft_discussion`), outside the profile's queue host.

AC08's states, AC09 and the Agent tab's part of AC01 have evidence; the card
stays In Progress for the tour chats and the local draft discussion.

## Looks

- `queue-sent-v001`, filed 2026-11-15, not waited on: session
  `b9d07626e46d2797ec280c09d469bec4`, state
  `/Users/jaeger/.attended-review-state/queue-sent-v001-2026-11-15/state`
  (URL in `serve.log` beside it), cursor 3, served on the tailnet (host PID
  79132, Tailscale child 79214). Root `/Users/jaeger/queue-sent-v001.o2z5din9`
  from `cf2c6d800`, `returning-review-v1` with `--next-push`, Fixture agent.
  One question, `BPR-2154-sent-to-agent`.
- `queue-sent-v001` handed back 2026-11-16: `BPR-2154-sent-to-agent`
  **Accepted** by the owner, no note (decision
  `e795a561-da44-4b43-81b8-7de3434ac29e`, session sequence 6). Host
  (79132/79214) stopped; root `/Users/jaeger/queue-sent-v001.o2z5din9`
  removed with `freeze_attended_build.py remove`. Two roots from failed
  freezes, `/Users/jaeger/queue-sent-v001.3k92km1p` and
  `/Users/jaeger/queue-sent-v001.yi4i4_2v`, hold only an export (and a
  failed build) and are refused by `remove`; left for the coordinator.

## Owner decision (2026-11-16, through the coordinator)

The tour side chat, the whole-tour conversation and the local draft
discussion queues stay on this card; no new card. Next: a queue per tour part
and one for the local draft discussion, their turn runner, Steer wiring and
pending list, reusing [[BPR-2181]]'s runtimes; frontend-design for the Qt
surfaces; then one look (Fixture agent, fresh session, one Accept/Reject per
surface), then BPR-2155.

### Plan for that slice (built 2026-11-16, below)

1. Core: `queued_session_kind::tour_part` keyed by pull request plus tour
   thread (kind and stop), and `local_draft_discussion` keyed by the local
   draft's identity; `session_key`/`parse_session_key` round-trip them;
   `core::decide_follow_up` already serves every composer.
2. Host: the window offers an `agent_session_runtime` for its tour sessions to
   `agent_session_runner` (Runs, State, Start through `request_tour_ask`,
   Steer through the tour run's invocation host, Bound); the local draft
   workbench offers one for its discussion. The queue host itself is
   unchanged (claims, receipts and settlement from BPR-2152 apply as is).
3. Qt: `tour_side_chat` keeps Send enabled while its thread's turn runs,
   labelled Queue, with Cmd+Return for the other; the pending list widget of
   `agent_tab_view` is factored out and reused by the tour chat and the local
   draft discussion.
4. Courses: one public path per surface (tour stop chat, whole-tour
   conversation after the first draft, local draft discussion) for AC01;
   AC08 states reuse the Agent tab's; then the look.

## Tour chats and the local draft discussion (2026-11-16)

frontend-design was loaded for this round's Qt work; the pending list keeps
the accepted Agent-tab look (looks prompts-v001 to v004) unchanged.

- **Core.** `queued_session_kind::tour_part` (`tour-part:host/owner/name#N/PART`)
  and `local_draft_discussion` (`local-draft:DRAFT/PART`), the part being
  `tour`, `problem`, `solution` or `stop-ID` (`tour_part_name`,
  `tour_part_thread`, `tour_part_session`, `local_draft_session`); every part
  is its own queue. `core_agent_queue` covers the round trip and the refusals.
- **Qt.** The Agent tab's pending list is `qt::agent_pending_list`
  (`agent_pending_list.{hpp,cpp}`), now also shown above the tour side chat's
  composer for the current part. While the tour agent runs, or the part's
  queue holds messages, Send reads Queue (or Steer, by the Follow-up setting),
  Cmd+Return (Ctrl+Return off macOS) the other once, and Stop stands beside
  it; the whole tour's feedback during its first draft keeps its own path
  (BPR-2181 AC11).
- **Host.** The window offers the profile's runner a second runtime
  (`window_chat_queues.cpp`): a pull request's tour parts run on the window's
  tour host (Start through `request_tour_ask`, Steer through the tour run's
  invocation host by `agent_runtime_steer`, the run's corrections settled by
  receipts at its end, the release settled on the host's retirement, deferred
  out of it); a local draft's parts on the draft's own lane (its question's
  end settles it). The queue host is unchanged.
- **Found and fixed.** In the composed app a local draft's discussion
  question was stopped by every tour-editor refresh, hide or focus change
  (`writing_stop` let go any question); it now lets go only a one-shot
  writing request's question.
- **Fixture agent.** Answers tour-ask and tour-revise turns (its reply names
  the message it read), drafts two stops on the renderer change, and is slow
  only for this turn's own "Take your time".
- **Courses** (fixture agent, returning-review-v1, pass):
  `application_tour_chat_queue_stop_public_path` (Queue label and Stop beside
  it, two follow-ups queued, one edited in place, none in the whole tour's
  chat, each goes after the one before, in order, once),
  `application_tour_chat_queue_whole_tour_public_path` (a follow-up queued,
  one steered with Cmd+Return into the running turn, Pause queue holding past
  the turn's end, Resume sending it once),
  `application_tour_chat_queue_local_draft_public_path` (a real worktree:
  a follow-up typed while the question runs waits in the pending list and
  goes as the draft's next question).

### Look queue-tour-v001 (filed 2026-11-16, not waited on)

Session `1b7406ad1e4e00a25aee2d23cd0f6dab`, state
`/Users/jaeger/.attended-review-state/queue-tour-v001-2026-11-16/state`
(URL in `serve.log` beside it), cursor 6, served on the tailnet (host PID
28483, Tailscale child 28491). Root `/Users/jaeger/queue-tour-v001.ity27747`
from `0ed8dbaf081f`, returning-review-v1 with `--next-push`, Fixture agent
only (launcher check: 17 asks answered, no real agent started; executable
SHA-256 `cf93d29ad698…`). The local draft's worktree is
`/Users/jaeger/queue-tour-v001-draft/engine` (made by
`tests/fixtures/local-draft/make_worktree.sh`; outside the root, remove it
with the root). Three Accept/Reject questions: `queue-stop-chat`,
`queue-whole-tour`, `queue-local-draft`. A failed first freeze on
conventional-comments-v1 (the launcher's fixture-agent check needs a
returning review) was removed with `freeze_attended_build.py remove`.
