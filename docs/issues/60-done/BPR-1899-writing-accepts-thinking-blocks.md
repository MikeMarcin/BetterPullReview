---
schema: "implement-rfc.card.v1"
id: "BPR-1899"
title: "Tool-free writing runs when Claude also writes a thinking block"
status: "Done"
rfcs: []
depends_on: ["BPR-1896"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"BPR-1896 implementer, 2026-11-06; owner asked for the card","related_cards":["BPR-1375","BPR-1891","BPR-1896"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "4b3dc2e6bbd2a9d2e10659718de87ad00437f0c7", "evidence": {"kind": "command", "detail": "application_agent_launch (test_claude_writing_reads_past_thinking_blocks): a thinking frame, or a redacted_thinking frame, before a text frame, and thinking and text in one frame, each finish with the text only; a run whose assistant frames hold only thinking still fails malformed_frame. application_review_tour_writing runs THINKING_CANARY, whose fixture writes a thinking and a redacted_thinking frame before the text, and the tour message finishes with the agent's text."}}, {"criterion": "AC2", "outcome": "pass", "revision": "4b3dc2e6bbd2a9d2e10659718de87ad00437f0c7", "evidence": {"kind": "command", "detail": "application_agent_launch: after a thinking frame, a tool_use block, a server_tool_use block, an image block, a user frame and a finding payload are each refused malformed_frame with no answer item."}}, {"criterion": "AC3", "outcome": "pass", "revision": "4b3dc2e6bbd2a9d2e10659718de87ad00437f0c7", "evidence": {"kind": "command", "detail": "application_agent_launch: no event text and no answer item carries the fixture's thinking, signature or redacted data. describe writes ' thinking=on' after memory=on and parse_agent_run_diagnosis reads it back exactly, refusing thinking=off, another value or a reordered token. application_review_tour_writing finds 'stderr_bytes=0 thinking=on' on the agent.run line, no fixture thinking token in any log line and none in the saved draft."}}, {"criterion": "AC4", "outcome": "pass", "revision": "4b3dc2e6bbd2a9d2e10659718de87ad00437f0c7", "evidence": {"kind": "command", "detail": "application_agent_launch: the same stream read as agent chat finishes with the text, reports no thinking and carries no thinking content; the decoder change applies only to writing runs."}}, {"criterion": "AC5", "outcome": "pass", "revision": "4b3dc2e6bbd2a9d2e10659718de87ad00437f0c7", "evidence": {"kind": "observation", "detail": "review_tour_composition_public_path --installed-writing /opt/homebrew/bin - \"\" once (no wrapper, the installed Claude's default model), offscreen on this Mac's Claude Code 2.1.310, three runs: each wrote the Problem message (Finished: success, text changed, undo restored bytes and selection, redo ran no inference). Two of the three agent.run lines read thinking=on; BPR-1896 saw both default-model runs refused on a thinking block."}}, {"criterion": "AC6", "outcome": "pass", "revision": "4b3dc2e6bbd2a9d2e10659718de87ad00437f0c7", "evidence": {"kind": "command", "detail": "ctest --preset dev: architecture_boundaries, application_agent_launch, application_review_tour_writing and application_review_tour_writing_wrapper pass (4/4). coding-format-check passes. python3 scripts/verify_documentation.py --root . passes. Owner direction 'dont wait for tests': no other suite was run."}}]
---

# BPR-1899: Tool-free writing runs when Claude also writes a thinking block

## Observed

While gathering BPR-1896's evidence on Claude Code 2.1.310, writing runs
ended `protocol_failure` because an assistant frame held a `thinking` block.
It happened in 1 of 3 runs with Sonnet, and in both runs with the default
model. The writing decoder's text-only check refuses every block that is not
`text`. A wrapper, a model default or managed settings can turn thinking on
over this app's settings (BPR-1896 showed that Claude keeps only the last
`--settings`), so a review tour message can still fail.

## Decision (owner, 2026-11-06)

Accept `thinking` and `redacted_thinking` blocks in a writing run, and drop
their content. Only `text` blocks make the answer. Every other block type
(`tool_use`, `server_tool_use`, images, ...) is still refused, and so is a
`user` frame or a finding payload.

## Change

- In the writing decoder (`src/application/src/agent_launch.cpp`), skip
  `thinking` and `redacted_thinking` blocks in assistant frames (and their
  stream events, if the writing path reads partial messages). Never store or
  log their content. An assistant frame that holds only thinking contributes
  no text. A run whose answer has no text at all still fails as it does today.
- Report `thinking=on` on the `agent.run` line, next to `plugins=` and
  `memory=on`, with no content.
- If a supported setting or environment variable turns thinking off for
  `WritingOnly` launches, add it as best-effort hardening, next to
  `CLAUDE_CODE_DISABLE_AUTO_MEMORY`. Verify it on the installed Claude first.
- Tests: a decoder case with a thinking block before the text, one with
  `redacted_thinking`, and a refusal for `tool_use`. An end-to-end writing
  canary that emits a thinking block finishes with text and logs
  `thinking=on` and none of the thinking content.
- Docs: `ai-agent-boundary.md` and `security-threat-model.md`, where the
  text-only answer check is described. Thinking is accepted and discarded,
  and it can't act because no tools are present.

## Acceptance

- AC1. A writing stream with a thinking or redacted_thinking block followed by
  text finishes with that text only.
- AC2. A `tool_use` block, or any other non-text, non-thinking block, is
  still refused. So are a `user` frame and a finding payload.
- AC3. Thinking content never reaches the answer, the log or any stored
  state. The log reads `thinking=on`.
- AC4. Agent chat is unchanged.
- AC5. The review tour writes through the installed Claude with its default
  model offscreen (`--installed-writing`), where BPR-1896 saw it fail.
- AC6. `application_agent_launch`, `application_review_tour_writing`,
  `application_review_tour_writing_wrapper`, `architecture_boundaries` and
  the documentation gate pass.

## Result

- The writing decoder reads past `thinking` and `redacted_thinking` blocks in
  assistant frames and drops their content unread; only `text` blocks make
  the answer. A writing frame that holds only thinking does not count as an
  answer, so a run with no text still fails at its result. A thinking block
  emits `thinking_reported`, which carries no text, becomes
  `agent_run_result::ReportedThinking` and `thinking=on` on the `agent.run`
  line. Chat reads its blocks as before. The writing path does not read
  partial messages: `stream_event` frames are read past as before.
- `ai-agent-boundary.md` and `security-threat-model.md` describe thinking as
  accepted and discarded.
- The `--installed-writing` course prints each run's `agent.run` line (enums
  and counts only), so a live run shows `thinking=on` without content.
- Hardening not added. Claude Code 2.1.310's binary names
  `CLAUDE_CODE_DISABLE_THINKING`, but direct probes with the writing
  arguments on the default model wrote no thinking block with or without it
  (also with `alwaysThinkingEnabled: true` and `MAX_THINKING_TOKENS=4000`), so
  its effect could not be verified on the installed Claude, as the card
  requires first. Only block types were recorded.
- Pending: the owner confirms on the next prerelease. No attended review, on
  the owner's instruction.
- Landed on the task branch as 4b3dc2e6b, over main 2d714bdc6. Not pushed or
  merged.
