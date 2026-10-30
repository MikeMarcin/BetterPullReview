---
schema: "implement-rfc.card.v1"
id: "BPR-1827"
title: "A Claude Code agent answers on a real pull request, and a stopped run says why"
status: "Done"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1827--BPR-1827--23a2b35b8218.json","kind":"defect","found_by":"owner report on 0.4.1 build 6, 2026-10-29: a custom Claude Code stream JSON agent ended 'answered in a form this app can't read' on every Ask and Review this pull request, and the log said 'The agent wrote nothing.'","related_cards":["BPR-1375","BPR-1738","BPR-1753","BPR-1817","BPR-1823"],"frontend_design":"frontend-design:frontend-design loaded before the log's words; no layout, colour or control change"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "ba4019018", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_agent_launch_canary$'", "detail": "Through the real process launcher, the canary's --current-claude-transcript (a banner line, hook frames before init, 2.1.310's init fields, thinking, task, stream and progress frames, a string-content user frame, twelve tool-result lines past 64 KiB each and past 1 MiB in all, frames after the result) answers success with exactly its one text and its cost. With --transcript-stops-early it is nonzero_exit 3 with its stderr. The generic 64 KiB bound still refuses an oversize Codex JSONL line, keeping only its first 512 bytes."}}, {"criterion": "AC2", "outcome": "pass", "revision": "ba4019018", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_agent_launch$|^application_agent_protocol_loop$'", "detail": "The decoder reads the current transcript pushed in 16 KiB reads past 1 MiB, records only init and assistant runtime facts, still refuses broken JSON, a foreign session, a payload outside the result and a foreign init, and holds Claude's own 16 MiB line bound. The loop counts stdout bytes, keeps the refused line, and reports an error exit mid-line by its exit. Every BPR-1375 strictness case is unchanged."}}, {"criterion": "AC3", "outcome": "pass", "revision": "ba4019018", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_agent_launch_panel$'", "detail": "A stopped run's log says what went wrong in words with the failure's name, the exit code, the bytes written to each stream, the refused line bounded to 400 characters and the last twenty stderr lines, and never 'wrote nothing'."}}, {"criterion": "AC4", "outcome": "pass", "revision": "ba4019018", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^quill_logging$|^application_agent_launch$|^application_release_maintenance$'", "detail": "An agent.run ended line reaches the real log as category agent with detail 'protocol=claude_stream_json outcome=protocol_failure failure=frame_too_large exit=0 stdout_bytes=223211 stderr_bytes=12'; an agent.run line whose detail is prose is dropped; the reader takes exactly what describe writes."}}, {"criterion": "AC5", "outcome": "pass", "revision": "ba4019018", "evidence": {"kind": "command", "command": "ctest --preset dev -R 'application_agent_chat_|application_agent_finding_|application_review_round_trip_composition_contract'", "detail": "The composed application runs every agent through diagnosed_agent_process; all agent chat and finding composition contracts pass, and the resealed full-application and production-manifest digests hold."}}, {"criterion": "AC6", "outcome": "pending", "revision": "ba4019018", "evidence": {"kind": "freeform", "detail": "Owner check on the next release build with their own custom agent on a real pull request. Not reproducible here: the owner's wrapper and repository are private. The owner checks that Ask and Review this pull request answer, and, if any run still stops, that Show the log names the failure and the line, and that the diagnostics export has agent.run lines."}}]
---

# BPR-1827: A Claude Code agent answers on a real pull request, and a stopped run says why

## Report

On 0.4.1 build 6 the owner asked a custom agent, set to Claude Code's
stream JSON output, about a real pull request. After the workspace was made,
about 13 to 15 seconds in, the run ended with "stopped before answering" and
"answered in a form this app can't read". Show the log said "The agent wrote
nothing." Review this pull request ended the same way. The setup's program
check had passed. The diagnostics export held no agent event at all.

## Root cause

Claude Code writes every tool result into its stream JSON transcript twice:
as the `tool_result` content of a `user` frame and again as that frame's
`tool_use_result`. Measured with Claude Code 2.1.310 in an empty folder, one
Read of a 92 KB file produced a 111 KB line, and a second read a 93 KB line;
the whole two-read run wrote 223 KB. The decoder applied the generic 64 KiB
per-line bound to Claude, so the first tool result on any real-size file
refused the run as `frame_too_large`. A review that reads a dozen files also
passes the generic 1 MiB stream bound (`output_too_large`). Both are
`protocol_failure`, which the Agent tab words as "answered in a form this app
can't read". A run that uses no tool, or reads only small files, passed, which
is why the fixture courses and the setup check did not see it.

The log was empty because a failed run clears its events and the panel showed
only stderr; the agent's stdout and the decoder's cause were never shown.
Agent runs recorded no diagnostic line.

The setup's program check runs the custom agent's own check arguments, or
only checks the program launches; it never decodes a turn. A turn probe would
not have caught this either, since the failure needs a tool result larger
than 64 KiB, so no probe was added.

The same measurement showed frame types the decoder did not name
(`hook_started`, `hook_response`, `task_summary`, `thinking_tokens`,
`post_turn_summary`) and the source of hypothesis 1. Those were already read
past as `system` frames, but `stream_event`, `tool_progress`, a string-content
`user` frame, a repeated init, a `rate_limit_event` after the result, and a
wrapper's banner line would each have refused the run.

## Built

- **Decoder** (`agent_launch.cpp`): Claude stream JSON has its own bounds,
  `maximum_claude_frame_bytes` (16 MiB) and `maximum_claude_stdout_bytes`
  (256 MiB); frames are parsed and dropped, assistant text keeps its 1 MiB
  bound, and a long line is scanned only where new bytes arrived. Only init,
  `assistant`, `user` and `result` frames carry the turn; other types are read
  past under the session, as are a string-content user frame, a same-session
  init and a line that does not begin as a JSON object. Only init reports
  runtime facts among system frames. The decoder keeps the first 512 bytes of
  the line it refused (`rejected_line`) and counts every pushed byte.
- **Loop** (`agent_protocol_loop.cpp`): the result carries
  `StandardOutputBytes` and `RejectedOutputLine`; an error exit that left an
  unfinished line is `nonzero_exit`.
- **Log** (`agent_launch_panel::failure_log`): what went wrong in words and by
  name, the exit code, the bytes written to each stream, the refused line
  (400 characters at most) and the last twenty stderr lines (4,000 characters
  at most). "The agent wrote nothing." is now said only when nothing was.
- **Diagnostics**: `application::diagnosed_agent_process` wraps the entry
  process the agent host runs through and records `agent.run.started` and
  `agent.run.succeeded|failed|cancelled` with detail
  `protocol=… outcome=… failure=… exit=… stdout_bytes=… stderr_bytes=…`.
  `quill_log` accepts an `agent.run` line only when its detail parses as
  exactly that, so no prompt, answer, path or program name can ride it; the
  maintenance preview classifies the operation.
- **Fixture**: the canary's `--current-claude-transcript` and
  `--transcript-stops-early`, with neutral names only.
- Resealed the full-application and production-manifest digests;
  `docs/architecture/ai-agent-boundary.md` and the fixture catalog describe
  the bounds, the frames read past and the fixture.

## Design

Loaded `frontend-design:frontend-design` before writing the log's words. No
layout, colour or control changed; the log says what happened and what the
agent wrote, plainly.

## Not reproduced

The owner's wrapper, account and repository are private and were not used.
The diagnosis rests on the measured Claude Code 2.1.310 transcript and the
fixture built from it. Whether the wrapper adds anything else to stdout is
for the owner's next run to show: if it still stops, the log now names the
failure and the line.

## Owner check (next build)

1. Ask the custom agent about a real pull request, and press Review this pull
   request. Both should answer.
2. If either still stops, press Show the log: it names what went wrong, how
   many bytes the agent wrote, the first line the app could not read and the
   last lines of its errors. Send that cause and line (redacted as needed).
3. Export diagnostics: it holds `agent.run.started` and
   `agent.run.succeeded` or `agent.run.failed` lines, with the protocol,
   outcome, failure, exit code and byte counts and no text.

## Verification

At `ba4019018`, dev preset, macOS: `application_agent_launch`,
`application_agent_protocol_loop`, `application_agent_finding_protocol`,
`application_agent_invocation_host`, `application_agent_chat_replies`,
`application_claude_process_wrapper`, `qt_agent_launch_panel`,
`qt_agent_launch_canary`, `qt_agent_launch_canary_durable_course`,
`quill_logging`, `quill_logging_content_policy`, `quill_logging_backpressure`,
`application_release_maintenance`, every `application_agent_chat_*` and
`application_agent_finding_*` composition contract,
`application_review_round_trip_composition_contract`,
`architecture_boundaries` and `architecture_boundary_contract` pass;
`coding-format-check` passes with clang-format 22.1.8.
