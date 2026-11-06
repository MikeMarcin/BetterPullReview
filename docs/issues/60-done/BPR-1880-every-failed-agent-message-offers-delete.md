---
schema: "implement-rfc.card.v1"
id: "BPR-1880"
title: "Every failed agent message offers Delete"
status: "Done"
rfcs: []
depends_on: ["BPR-1876"]
priority: "P2"
platforms: ["mac"]
metadata: {"kind": "feature", "found_by": "owner, 2026-11-06: 'Put Delete on every failed message, not just unsent ones'", "frontend_design": "loaded (frontend-design:frontend-design); no new visual language: the accepted BPR-1876 v002 flat footer Delete, now on every error turn", "agent_session_decision": "A message the agent received is forgotten once deleted: an Agent tab run starts a new agent process and resumes no harness session (only Review Cycles pass --resume), and what it knows of earlier turns is the kept thread its review brief carries (BPR-1753), which no longer holds the deleted turn. Tooltip only: 'The agent won't be sent it again, so it won't remember it.'", "attended_review": "not needed: the visible change is the existing Delete on more turns and a tooltip"}
acceptance: [{"criterion": "AC1 Every failed turn kind shows Delete; successful turns do not. Core and Qt tests.", "outcome": "pass", "revision": "daa3434b1", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_agent_turn|qt_workbench_agent_send_refusal)$'", "detail": "core test_delete_never_sent: Delete on every non-answer outcome, latest or earlier, exactly while nothing runs; an answer never, with or without text; stopped beside Resend, unreadable beside Agent settings…. Qt test_a_failed_answer_asks_again_and_says_when_fixed: a thread of an answer and every failed outcome shows Delete on each failed turn and not the answer, with the never-reached tooltip for failed_to_start, workspace_unavailable and history_unavailable and the won't-remember tooltip for the rest."}}, {"criterion": "AC2 Deleting a received-and-failed turn behaves as recorded for the agent session, covered by a test.", "outcome": "pass", "revision": "daa3434b1", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_workbench_agent_send_refusal$'", "detail": "Deleting the latest stopped turn and then an earlier unreadable turn removes each from the thread and from the thread observer's kept thread (the conversation the next review brief carries), leaving the answer; the agent session decision is in metadata.agent_session_decision."}}, {"criterion": "AC3 python3 scripts/verify_documentation.py --root . passes.", "outcome": "pass", "revision": "daa3434b1", "evidence": {"kind": "command", "command": "python3 scripts/verify_documentation.py --root .", "detail": "DOCUMENTATION_OK"}}]
---

# BPR-1880: Every failed agent message offers Delete

BPR-1876 v002 offers Delete only on a failed turn whose message never reached
the agent (did not start, workspace not made, history not recorded). The
owner wants it on every failed turn: stopped, timed out, error exit,
unreadable, set aside, as well.

## Change

- Offer Delete beside the failed turn's action (Resend, or Agent settings…
  for an unreadable answer) on every failed turn, latest or earlier, while
  nothing is running.
- Deleting removes the turn from the thread and from what is kept for next
  time, as BPR-1876 does.
- For a message the agent did receive, decide and record what deletion means
  for the agent's own resumable session (for example: the next message starts
  a fresh agent session, or the agent may still remember it). Never leave the
  reviewer believing the agent forgot something it still holds; if it may
  still remember, the tooltip says so in one line.

## Acceptance

- AC1. Every failed turn kind shows Delete; successful turns do not. Core and
  Qt tests.
- AC2. Deleting a received-and-failed turn behaves as recorded for the agent
  session, covered by a test.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.

## Fix

- Core `bpr/core/agent_turn.hpp`: `decide_agent_turn_actions` sets
  `Delete` on every error while nothing runs. `agent_turn_never_sent` now
  only chooses Delete's tooltip.
- Qt `agent_tab_view`: `delete_turn` accepts any ended non-answer turn; the
  Delete tooltip is set with the turn's actions: "Remove this message from
  the conversation. It never reached the agent." for an unsent one, "Remove
  this message from the conversation. The agent won't be sent it again, so
  it won't remember it." for one the agent received.
- `docs/ux/workbench-specification.md` describes both.

## Agent session decision

A message the agent received is forgotten once deleted. An Agent tab run is
a new agent process and resumes no harness session (`--resume` is passed only
by Review Cycles' `build_round_launch`); what the agent knows of earlier
turns is the kept thread its review brief carries ([[BPR-1753]]), and
deleting the turn removes it from that thread. No visible wording changed
beyond the tooltip, so no attended review.

## Open

- `tests/workbench_case_inventory.json`'s `source_sha256` against
  `workbench_window_tests.cpp` was already stale on the base (BPR-1876's
  card); this change does not reseal it.
- A harness may still keep its own transcript of a received run on disk;
  this application never resumes it.
