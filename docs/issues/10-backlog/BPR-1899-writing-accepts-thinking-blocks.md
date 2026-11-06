---
schema: "implement-rfc.card.v1"
id: "BPR-1899"
title: "Tool-free writing runs when Claude also writes a thinking block"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1896"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"BPR-1896 implementer, 2026-11-06; owner asked for the card","related_cards":["BPR-1375","BPR-1891","BPR-1896"]}
acceptance: []
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
