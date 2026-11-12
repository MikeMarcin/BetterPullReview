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
