---
schema: "implement-rfc.card.v1"
id: "BPR-2153"
title: "Claude and Codex App Server accept a native correction on the running turn, counted once"
status: "Backlog"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2152"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC05","AC07"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2153: Claude and Codex App Server accept a native correction on the running turn, counted once

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Codex App Server `turn/steer` with `threadId` and `expectedTurnId`; Claude bidirectional stream JSON with a UUID-bearing user input, priority `next`, correlated by input acknowledgment and `user_message_uuid(s)` separately from the result `uuid` fence. Completion waits for every accepted input's terminal disposition and identified continuations or helpers. Capability from local protocol and version observations, never a paid probe; an unsupported driver keeps Queue and names why Steer is unavailable; no kill, restart or replay fallback. Accounting: every native part of a steered run counts once against the run's turn and session totals and limits; unknown stays unknown. Installed-agent qualification with exact versions, smallest bounded conversations, on the owner's installed Claude and Codex.

## Acceptance

- **AC05:** Qualified installed Claude and Codex App Server each accept a native correction on their existing conversation without transcript replay or a deliberate process restart. Unsupported configurations keep Queue and report Steer as unavailable without a paid discovery probe.
- **AC07:** All native parts of a steered run contribute once to the existing turn/session accounting and limits. Unreported costs remain unknown, and pending-input management adds no model request, summarizer or keep-alive call.

## Notes

Owner questions: Q3, Q14, Q15, Q17, Q18.

## Progress

- 2026-11-14: started by coordinator assignment on branch
  `claude/native-steering`, ahead of BPR-2152 (durable handoff, Backlog).
  This card builds the native mechanism, the typed capability and the
  accounting; the durable Sending/Delivery-unknown records stay BPR-2152's.
- 2026-11-14: mechanism and accounting built (`956e8504f`), proved against
  the fixture agent (`e4f1d521d`). The Steer button's wiring is not done: the
  lane's BPR-2151 commits `d8f7fbf4b` and `3e2512da9` changed
  `window_quick_ask.cpp`, `agent_queue_host` and `agent_tab_view.cpp` after
  this branch's base, so by the coordinator's rule the wiring waits.
- 2026-11-14 (critical review of `3502b1010`): steering is bound to the run
  identity; support reads the version Claude reported, `version_unknown`
  until then; an unanswered `turn/steer` expires after 30 s as unknown;
  empty and over-long text are refused by name; an unknown correction sets
  `SteerDeliveryUnknown` on the run's result. Host-level test added. The
  Steer button's wiring is parked on `claude/native-steering-wiring-wip`
  while the queue host is redesigned (coordinator).
- 2026-11-14: returned to Backlog. The documentation gate refuses In
  Progress while `depends_on` BPR-2152 is not Done ("unmet dependencies
  belong in Backlog"); the work above stays on `claude/native-steering`.

## Design as built

- **Capability (Q17).** `core::agent_steer_support_of` decides from the
  protocol, whether the run is on a kept native connection, and the version
  the runtime itself reported (Claude's `claude_code_version`, 2.1.319 or
  newer). It never probes. Codex exec and external terminals answer
  `protocol`; a one-shot or replayed run `no_connection`; an older Claude
  `version`; a Claude whose init has not yet named its version
  `version_unknown`, never assumed qualified. The protocol loop reports the
  version to the kept connection, and `agent_process_port::steer_support`
  answers from it. `agent_invocation_host::steer_support(run)` and
  `steer(run, text, answer)` take the run identity the projection was given
  and refuse another run as `other_run`; empty text is `empty_text`, text
  over 16 KiB `text_too_long`. Every refusal has words
  (`core::agent_steer_unavailable_words`).
- **Channel (Q14).** Claude: one user message with a UUID-shaped identity
  (`core::agent_steer_identity`, from the native turn and ordinal) and
  `priority: "next"`; never `now`. Codex App Server: `turn/steer` with
  `threadId`, `expectedTurnId` and `clientUserMessageId`, numbered above any
  `turn/start`. The protocol loop writes each correction once, in order,
  only while the agent itself works (not while it waits only on helpers),
  and only for the exact run and native turn it names. Nothing restarts,
  replays or resends.
- **Settlement.** Claude: `user_message_uuids` (or `user_message_uuid`) on a
  result consumes it; the result's own `uuid` stays a separate fence. A
  result that leaves it open with `queued_turn_count` above zero keeps the
  run going into the successor native turn, under the same run, lease and
  limits; otherwise it is unknown and the connection is retired. App Server:
  an error reply is a definitive refusal (not consumed); a reply naming the
  running turn followed by `turn/completed` is consumed; a reply naming
  another turn is unknown and retires the connection. The turn is not
  complete while a `turn/steer` is unanswered, for at most
  `agent_steer_reply_deadline_milliseconds` (30 s); then the correction is
  unknown, the turn can complete, and a late answer is read past. A run
  with an unknown correction keeps its outcome and sets
  `agent_run_result::SteerDeliveryUnknown`, for the queue to pause on.
- **Accounting (AC07).** `agent_run_result::Steers` counts each correction
  once by disposition. A steered run's cost is Claude's last cumulative
  `total_cost_usd` (`core::read_run_cost_report`), so the existing
  `account_agent_turn_cost` counts every native part once; a figure that goes
  down mid-run leaves the run's cost unknown. Deadlines are not reset by a
  correction. Codex reports no dollars and none are invented. Pending-input
  management adds no request: only an explicit Steer writes.

## Evidence

- Zero-cost local observations, 2026-11-14 (no inference: a scratch home
  holds no credentials):
  - Claude Code 2.1.319 (`/opt/homebrew/bin/claude`), signed out, stream
    JSON in and out: two UUID-bearing user messages, the second with
    `priority: "next"`, were both accepted; `--replay-user-messages` echoed
    each with `isReplay: true` and its UUID; each result carried
    `user_message_uuid`, `user_message_uuids`, `queued_turn_count` and
    `result_index`; the second input ran as a successor native turn (second
    init, `result_index: 1`). Cost $0.00.
  - Codex CLI 0.167.0 `app-server`, scratch `CODEX_HOME`, signed out (401
    before any inference): `generate-json-schema` lists `turn/steer`
    (`threadId`, `expectedTurnId`, `input`, optional `clientUserMessageId`;
    reply `{turnId}`). Live: a mismatched `expectedTurnId` was refused with
    `-32600 expected active turn id ... but found ...`; the matching one was
    answered `{"turnId": ...}` and the input appeared as a `userMessage` item
    with `clientId` in the same turn. Cost $0.00.
- AC05 installed-agent probes, 2026-11-14 (owner approved in chat, "Yes,
  cap $0.50"; the installed CLIs on the owner's own sign-ins, nothing in
  `~/.claude` or `~/.codex` read or written by the probe, a scratch working
  directory holding `a.txt`, `b.txt`, `c.txt`; the task read them with one
  tool call each):
  - Claude Code 2.1.319, `-p`, `--model haiku` (`claude-haiku-5-5`),
    `--setting-sources user`, `--no-session-persistence`,
    `--strict-mcp-config` with an empty MCP config, stream JSON in and out.
    The correction (UUID `b5ee7e00-…`, `priority:"next"`) was written as the
    first `Read` tool call arrived; Claude read a.txt, then b.txt and c.txt,
    and answered "The three words are apple, banana, and cherry. STEERED".
    One result: `user_message_uuids` named the first input and the
    correction, `queued_turn_count: 0`, `num_turns: 4`. Cost from the
    result: **$0.0033** (`total_cost_usd` 0.00333895).
  - Codex CLI 0.167.0 `app-server`, `gpt-5.6-luna` (the model list's "older
    fast and efficient" model), effort low, `thread/start` with
    `ephemeral: true`, read-only sandbox, no MCP servers. `turn/steer` was
    sent as the first `cat` started; the reply `{"turnId": …}` named the
    running turn; the correction appeared as a `userMessage` item with its
    `clientId` after the second `cat`, in the same turn; the answer ended
    "STEERED"; one `turn/completed`. Codex reports tokens, not dollars
    (ChatGPT Pro sign-in): 88,921 input (74,368 cached) and 462 output
    tokens; no dollar figure is invented.
  - Total reported spend: $0.0033 of the $0.50 cap. The settle logic needed
    no change; `application_agent_protocol_loop` replays both sequences.
- Tests: `core_agent_steering`, `application_agent_protocol_loop` (Claude
  same-turn, successor turn, unknown and retire, unqualified, other run;
  App Server accepted, refused, other turn, unanswered),
  `application_agent_invocation_host` (steer support and hand-off through
  the host), `qt_agent_steering_fixture` (real launcher, native connection
  and loop against the fixture agent's steerable mode), and
  `fixture_agent_answers`.

## Pending

- The owner's look at Steer (steer-v001): Accept or Reject. Filed
  2026-11-15, not waited on: session `1e6b0468f60fc61d6480addad7ea6224`,
  state `/Users/jaeger/.attended-review-state/steer-v001-2026-11-15/state`
  (URL in `serve.log` beside it), cursor 3, served on the tailnet (host PID
  20453, Tailscale child 20460). Root `/Users/jaeger/steer-v001.4mnd4k0p`
  from `c7a0c44d4`, frozen with `returning-review-v1` and `--next-push`
  (the launcher's own check presses What changed since my review, which
  only the pushed head offers). One question.
- steer-v001 handed back 2026-11-15 (cursor 19): `BPR-2153-steer`
  **Rejected**. The owner: "Steering doesn't show up in the transcript at
  all. I would have expected to see Two show up as something I had sent."
  (Screenshots: "Two" steered from its queued card left the queue and was
  taken, as the fixture's answer says, but no message "Two" appears in the
  transcript.) Required now (coordinator, 2026-11-15): the steered text
  shows as the reviewer's message inline in the running turn where it went
  in, marked steered, with its state when the agent never took it; it is
  kept in the stored transcript and native history across reopen and
  restart; it flows through the BPR-2181 runtime's turn events, not window
  code. Then steer-v002 with a question on the transcript.

## Progress (steered messages in the transcript, 2026-11-15)

- Rebased onto BPR-2181's profile runtimes (`121c32f58`). The Agent tab's
  Steer now runs on the session's `review_agent_runtime`: the registry's
  `agent_session_runtime::Steer` asks the runtime, which names the run it
  admitted and goes through its turn port (`review_agent_stack` forwards to
  its invocation host). The window only forwards the press and shows `Why`.
- The steered message is a turn event: the protocol loop records each
  correction it writes (`agent_run_result::SteeredInputs`: the words, how
  many assistant items and how many of the agent's own steps had happened,
  and at the end its disposition) and emits `agent_stream_kind::steered`.
  The runtime keeps it in the turn it holds; the agent panel keeps it for
  the running turn; the Agent tab shows it as the reviewer's message,
  marked "Steered after step N", between the question and the reply, and
  says when the agent refused it or may not have taken it. The activity
  record counts steps rather than listing them, so the step number is how
  the transcript says where it went in.
- It is kept: the stored turn (`agent_conversation_turn::Steered`: words,
  where in the answer, step, state) goes into native history through a new
  `native_agent_turn_steers` table, read back with the turn and pruned with
  it; the transcript shown again carries it.
- The fixture agent acknowledges a correction without repeating its words
  ("Took your correction after step N"), so the words show once, as the
  reviewer's.
- Found on the way, not this card's: at the lane tip `36cc0c881` (built and
  run in this worktree, detached), `application_agent_queue_restart_public_path`
  fails at "the running ask finishes", the same as on this branch. A
  question typed right after a review opens is now refused until the window
  has lent the session's runtime its authority (`launch_refused`), and a
  file opened with `open_file_in_review` stays on BPR-2180 v004's code
  skeleton in the public-path fixture, so a question about its lines finds
  no selection; the steer and fixture guard courses open the file at a line
  and send again until the turn runs. The contract's app source list lacked
  the lane's `review_agent_stack` files; they were added with the reseal.

## Progress (fixture session, 2026-11-15)

- The blocker: a tests-on fixture session's Agent tab ran on the built-in
  Claude profile (profile 3) with no agent entry, so the window asked for no
  native session and no turn could be steered. Fixed test-only (under
  `BPR_BUILD_TESTING`, never in a shipped build): a fixture session adds the
  fixture agent as a person's setup adds Claude Code, a saved entry named
  "Fixture agent", preferred, whenever the setup's list shows none (the
  stored list is read off the lane and replaces what is shown when it
  lands); the picker offers that entry first.
- A second gap the sent course found: the four application process-port
  wrappers between the host and the launcher (`wrapped_claude_process`,
  `diagnosed_agent_process`, `located_agent_process`, `agent_entry_process`)
  and the tests-on probe did not forward `steer_support` and `steer`, so
  every run answered `no_connection`. Each now forwards them; the entry
  process asks its direct port, then the others.
- `application_agent_steer_sent_public_path` is enabled and passes: the
  follow-up steered into the steerable fixture's running Agent tab turn is
  taken after a tool call, leaves the queue, and no second turn runs.

## Progress (wiring)

- 2026-11-15: rebased onto BPR-2181 slice 1; Steer wired through the
  session queue host (window forwards only); the Fixture agent steerable by
  default as `claude`, `FIXTURE_UNVERSIONED` for an unversioned turn;
  `application_ask_queue_public_path` checks the real Q17 words; the app
  source digest resealed. Old WIP wiring branch deleted.
