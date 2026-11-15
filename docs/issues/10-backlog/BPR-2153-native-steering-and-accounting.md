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
- 2026-11-14: returned to Backlog. The documentation gate refuses In
  Progress while `depends_on` BPR-2152 is not Done ("unmet dependencies
  belong in Backlog"); the work above stays on `claude/native-steering`.

## Design as built

- **Capability (Q17).** `core::agent_steer_support_of` decides from the
  protocol, whether the run is on a kept native connection, and the version
  the runtime itself reported (Claude's `claude_code_version`, 2.1.319 or
  newer). It never probes. Codex exec and external terminals answer
  `protocol`; a one-shot or replayed run `no_connection`; an older or silent
  Claude `version`. `agent_invocation_host::steer_support` reports it for the
  running turn; the version is checked again at hand-off.
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
  complete while a `turn/steer` is unanswered.
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
- Tests: `core_agent_steering`, `application_agent_protocol_loop` (Claude
  same-turn, successor turn, unknown and retire, unqualified, other run;
  App Server accepted, refused, other turn, unanswered),
  `application_agent_invocation_host` (steer support and hand-off through
  the host), `qt_agent_steering_fixture` (real launcher, native connection
  and loop against the fixture agent's steerable mode), and
  `fixture_agent_answers`.

## Pending

- The Steer button's wiring in `window_quick_ask.cpp` (its Steer today moves
  the message to the front and says Steer is unavailable): blocked on
  BPR-2151's moving files; a minimal separate commit once the coordinator
  rebases this branch or releases those files.
- AC05 installed-agent acceptance: a paid, capped probe of a mid-turn
  correction on the installed Claude Code and Codex App Server. A scratch
  home holds no sign-in, and the owner's real `~/.claude` and `~/.codex` are
  off limits, so it needs the owner to sign in a scratch home (or allow a
  named credential) before it can run.
