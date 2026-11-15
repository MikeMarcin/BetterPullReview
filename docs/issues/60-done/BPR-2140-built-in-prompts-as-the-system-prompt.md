---
schema: "implement-rfc.card.v1"
id: "BPR-2140"
title: "Built-in prompts reach the agent as its system prompt, verified, with a paste fallback"
status: "Done"
rfcs: ["RFC-BPR-0107"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["AC09"]}
acceptance: [{"criterion":"AC09","outcome":"pass","revision":"76100684fe14b299e3936876b62f7de098b66ecf","evidence":{"kind":"command","detail":"Built-in prompts (Enhance, field drafts, outline, Test send, Draft Tour, the tour conversation, the local draft's description, discussion and commit message, the review skill) reach Claude as one inline --append-system-prompt and Codex exec as developer_instructions; pasted on the switch, the App Server and past 24 KiB; the installed Claude canary applied a rule given only in the system prompt (CANARY_PASS)","command":"ctest --preset dev -R '^(application_agent_launch_policy|core_local_draft_tour|local_changes|application_local_draft_tour_desktop_public_path|application_local_draft_author_desktop_public_path|application_agent_invocation_composition_contract)$' && python3 tests/scripts/claude_system_prompt_canary.py --launch-printer build/dev/tests/application/better_pull_review_agent_launch_policy_tests"}}]
---

# Built-in prompts reach the agent as its system prompt, verified, with a paste fallback

## Scope and acceptance

P02 settled by the owner (2026-11-12). Built-in prompt instructions go in
Claude's inline `--append-system-prompt "<text>"` (never the `-file` form,
which the corporate wrapper drops) and the run's material in the message;
Codex `exec` gets `-c developer_instructions=…` (verified on 0.167 with a
canary); the App Server pastes. Instructions past the Windows command-line
bound are pasted. A per-agent switch (System prompt / In the message) is
the fallback for a wrapper that drops it. A canary course with the installed
Claude shows a rule given only in the system prompt applied. The wrapper's
own behavioural confirmation of the inline route is pending the owner's
rerun.

Acceptance: AC09 as amended on 2026-11-12.

## Progress (2026-11-12)

Complete. Every built-in prompt now travels as the system prompt: Enhance,
field rewrites and drafts, the outline, Test send, Draft Tour and the tour
conversation (`split_tour_prompt`), the local draft's description,
discussion and commit message, and the review skill (out of the brief). The
switch, the App Server and the 24 KiB bound paste. Canary: the installed
Claude applied a rule given only in the system prompt. The owner's rerun
through the corporate wrapper is still theirs to make; the per-agent switch
is the fallback. Record: the [RFC-BPR-0107 ledger](../../verification/RFC-BPR-0107.md).

## Regression found later (2026-11-15, under [[BPR-2181]])

`application_native_agent_sessions` failed at `tour-native-first-answer`
from `b8c371613` on: its parent `b8e2adeac` passes and `b8c371613` fails,
each built in a scratch worktree. The tour conversation's instructions,
with the schema the answer must use, moved into Claude's
`--append-system-prompt`, but the agent canary looked for the schema only
in the message it read, so it answered in plain text and the tour said the
answer could not be read. The same commit made Draft Tour one tool-free
call ([[BPR-2141]]), and the canary still listed tools in its `init` frame
and reported a tool result there, which a tool-free run refuses. The
product was right both times; the fixture was not. The canary now reads
the appended system prompt too, and a tool-free run (`--disallowedTools
*`) lists no tools and reports no tool result
(`tests/fixtures/agent/agent_canary_main.cpp`). The course passes again.
Fixed in BPR-2181's round.
