---
schema: "implement-rfc.card.v1"
id: "BPR-2138"
title: "Each agent action runs fresh or resumes, as the owner classified them"
status: "Done"
rfcs: ["RFC-BPR-0101"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["AC10", "AC11", "AC12"]}
acceptance: [{"criterion":"AC10","outcome":"pass","revision":"76100684fe14b299e3936876b62f7de098b66ecf","evidence":{"kind":"command","detail":"Every launch path named with its session mode; fresh paths save no session and refuse a native turn, resumed paths resume, the tool-free local discussion included","command":"ctest --preset dev -R '^application_agent_launch_policy$'"}},{"criterion":"AC11","outcome":"pass","revision":"76100684fe14b299e3936876b62f7de098b66ecf","evidence":{"kind":"command","detail":"A tour part's Enhance is a fresh run; nothing is sent to the side chat; its result is a proposal on the part","command":"ctest --preset dev -R '^(qt_tour_side_chat|review_tour_agent_help)$'"}},{"criterion":"AC12","outcome":"pass","revision":"76100684fe14b299e3936876b62f7de098b66ecf","evidence":{"kind":"command","detail":"Ask more (Draft Tour's proposal, a fresh Enhance's suggestion) seeds the thread's next turn with the answer, which then resumes; the local draft's first description seeds the discussion's session, whose next turn resumes it by handle with no replay; live installed Claude resumed a tool-free discussion session (CANARY_PASS KUMQUAT same_session=True)","command":"ctest --preset dev -R '^(application_agent_launch_policy|core_local_draft_tour|qt_tour_agent_session_view|qt_tour_side_chat|application_local_draft_tour_desktop_public_path)$' && python3 tests/scripts/claude_resume_canary.py --launch-printer build/dev/tests/application/better_pull_review_agent_launch_policy_tests"}}]
---

# Each agent action runs fresh or resumes, as the owner classified them

## Scope and acceptance

Implements the owner's classification (RFC-BPR-0101, amendment of
2026-11-12): fresh disposable runs for one-offs (Enhance everywhere, a stop's
comment and empty-field drafts, the review-feedback hook, Draft Tour's first
draft, a local draft's first description and commit message, agent checks and
probes, the agent-assistance actions' first answers), resumed native sessions
for conversations (Agent tab, tour side chats and the whole-tour
conversation, the local draft's discussion, Review Cycle rounds, self-review
and fix loops, address-feedback runs).

- The core names every launch path and its session mode; the launch builder
  enforces it.
- A tour stop's Enhance is a fresh run whose result is a proposal on the
  stop; nothing is sent into the stop's side chat.
- Ask more promotes a fresh answer into a resumed session seeded with it.
- A test asserts each launch path's session mode (AC10).

Acceptance: AC10, AC11, AC12 as amended on 2026-11-12.

## Progress (2026-11-12)

Complete. The core names sixteen launch paths and their session modes and the
launch enforces them (AC10, `application_agent_launch_policy`); a tour part's
Enhance is a fresh run whose result is a proposal on the part (AC11,
`qt_tour_side_chat`); the local draft's discussion resumes its native
session by handle instead of replaying its transcript, its first
description is its own fresh path, and an explicit Ask more on Draft Tour's
proposal and on a fresh Enhance's suggestion starts the thread's next turn
from that answer (AC12; `application_local_draft_tour_desktop_public_path`,
`qt_tour_agent_session_view`, `qt_tour_side_chat`, and the live
`claude_resume_canary.py`). The agent-assistance actions (RFC-BPR-0116 to
0121) take `assistance_first_answer` when they are built. Record: the
[RFC-BPR-0101 ledger](../../verification/RFC-BPR-0101.md#second-pass-2026-11-12-resumed-local-discussion-ask-more-one-call-per-fresh-click).
