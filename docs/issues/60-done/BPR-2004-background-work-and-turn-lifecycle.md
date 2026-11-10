---
schema: "implement-rfc.card.v1"
id: "BPR-2004"
title: "Establish how agents report background work and end a turn when the work ends"
status: "Done"
rfcs: ["RFC-BPR-0103"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0016-background-work-and-turn-lifecycle.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0103 AC01","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"observation","detail":"Claude Code 2.1.319 stream JSON reports helpers (task_* frames, background_tasks_changed, parent_tool_use_id) and answers each report in a further part of the turn: helpers visible. Codex 0.167.0 App Server reports subAgentActivity and helper threads but never answers them; exec --json reports nothing; no flag removes the tools: asked to keep helpers foreground. Recorded traces in tests/fixtures/agent/traces; docs/verification/RFC-BPR-0103.md"}},{"criterion":"RFC-BPR-0103 AC02","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"Recorded Claude traces (input open and closed) through the shared loop: one success, three helpers started and ended, closing message in the same turn, one cost; composed Agent tab on a scripted runtime replaying the trace","command":"ctest --preset dev -R 'application_agent_helper_turn|application_agent_chat_helpers_public_path'"}},{"criterion":"RFC-BPR-0103 AC03","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"Trace cut after the first result: helpers_unfinished naming Count lines in b.txt and c.txt, text kept; the Agent tab keeps it and says how it ended","command":"ctest --preset dev -R 'application_agent_helper_turn|application_agent_chat_helpers_unfinished_public_path'"}},{"criterion":"RFC-BPR-0103 AC04","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"After the turn a late answer and a helper start are refused; a result with helpers out does not end the turn; a stale App Server terminal is refused","command":"ctest --preset dev -R 'application_agent_helper_turn|application_agent_launch$'"}},{"criterion":"RFC-BPR-0103 AC07","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"observation","detail":"live_second_account_course --course agent-helpers on MikeMarcin/BPR-Test #40 with installed Claude Code 2.1.319 (Haiku) and Codex 0.167.0 (App Server): each one turn, three helper rows while running, answered after the helpers; output recorded in docs/verification/RFC-BPR-0103.md"}}]
---

# Establish how agents report background work and end a turn when the work ends

## Scope and acceptance

Implements
[RFC-BPR-0103](../../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).

First record, for each installed agent and transport, how sub-agents and
background tasks are reported and whether the agent can be run with helpers
visible or must run foreground only. Then decode the runtime's activity and
helper events, keep a turn open while reported helpers are outstanding, accept
that turn's later frames, end every turn as an answer, Stopped or a stated
error, and extend the run diagnosis as the RFC states.

Covers AC01 to AC04 and AC07. This amends RFC-BPR-0096's late-frame rule only
for frames that continue an open turn; a frame for no open turn is still
refused.

This is part of `agent-work-visible` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
