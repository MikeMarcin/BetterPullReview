---
schema: "implement-rfc.card.v1"
id: "BPR-1747"
title: "Set up an agent the first time the Agent tab opens"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1747--BPR-1756--f06ad4b55890.json","kind":"design","origin":"Owner, 2026-10-22, relayed by the coordinator","owner_note":"I think we need some ux for setting up your agent the first time you go to the agent tab. Maybe a little wizard I'm not sure.","phase":"1 (investigation and design); no product code changed","draft_rfc":"docs/rfcs/assets/agent-first-run-setup-v1/RFC-DRAFT-0026-agent-first-run-setup.md","adopted_rfc":"docs/rfcs/RFC-BPR-0092-agent-first-run-setup.md","import":"docs/rfcs/assets/agent-first-run-setup-v1.LIVE_IMPORT.md","design_frames":"docs/design/BPR-1747-agent-setup-v1/index.html","frontend_design":"frontend-design:frontend-design loaded before the design frames","requested_skill":"frontend-design","owner_decisions":["D1","D2","Q1 A","Q2 accepted","Q3 A","Q4 accepted","Q5 accepted","Q6 accepted","Q7 accepted","Q8 A","Q9 A"],"owner_sessions":[{"id":"29049098deb24f47c839771a46b05c4c","state":"/Users/jaeger/.mask/attended-review/owner-review-20261022-agent-setup-v001","status":"superseded by r2 with no answers; released and host stopped 2026-10-22"},{"id":"349862f0cb991e14b6cb57cf80653c5f","state":"/Users/jaeger/.mask/attended-review/owner-review-20261022-agent-setup-v002","status":"Q1 and Q3 answered on the page, the rest in chat; host stopped through the script and protections released 2026-10-22"}],"milestone_cards":["BPR-1748","BPR-1749","BPR-1750","BPR-1751","BPR-1752","BPR-1753","BPR-1754","BPR-1755","BPR-1756"],"related_cards":["BPR-1594","BPR-1684","BPR-1301","BPR-1375","BPR-1179","BPR-0348","BPR-0532","BPR-1738"],"related_rfcs":["RFC-BPR-0011","RFC-BPR-0012","RFC-BPR-0060","RFC-BPR-0085","RFC-BPR-0003"]}
acceptance: [{"criterion":"The owner's answers to Q1 to Q9 are recorded here and in the RFC, with D1 and D2, and the RFC is adopted, revised or set aside to match","outcome":"pass","revision":"92162efed","evidence":{"kind":"observation","detail":"attended_review.py status --after 27 on session 349862f0cb991e14b6cb57cf80653c5f read Q1 select A (sequence 29) and Q3 select A (sequence 31). The page's Accept/Reject and Next failed on the rest, and the owner said in chat 'I want to accept all your recommendations but it's not letting me hit accept or next question on most of the questions'; the coordinator relayed Q2, Q4, Q5, Q6 and Q7 accepted, Q8 A and Q9 A. All eleven decisions are recorded in RFC-BPR-0092 r3, which adopts the revised draft. The host was stopped through the script and resume-development --all released the nine candidates."}}]
---

# BPR-1747: Set up an agent the first time the Agent tab opens

## Owner request

The owner, 2026-10-22, verbatim:

> I think we need some ux for setting up your agent the first time you go to
> the agent tab. Maybe a little wizard I'm not sure.

## Phase 1 finding

The request is right, and there are defects under it. Observed on
`b69b82c9b`, rendered offscreen with a fresh profile, light and dark:

- The Agent tab opens on a conversation. Its "Set up an agent" empty state
  can only show when no profile is offered, which never happens on
  github.com or GHES.
- The agent menu is the four built-in profiles by protocol name, with
  "Codex App Server" selected first. Nothing is looked for until Ask.
- The first Ask ended "Codex App Server stopped before answering. Finished:
  failed_to_start." The Codex and external terminal profiles name a bare
  program, and only Claude's launch is resolved to an absolute path, so a
  shipped build can never start Codex (the Unix supervisor refuses a
  relative program; Windows passes it to `CreateProcessW` unsearched).
- The owner's own first run (2026-10-22) chose **External terminal
  harness** for a Claude wrapper and ended "Finished: workspace_unavailable"
  with a raw Git log: the review workspace could not be made from the cache,
  and no agent ran. [[BPR-1738]] fixed that cause.
- "Use my working copy" always refuses, because no shipped path sets the
  checkout it needs. Nothing links a repository to the reviewer's clone, and
  the agent is told identities, not the description, threads or a review
  method.
- The MCP helper needs no user setup, and `bpr` ships in no release package.

## Outcome

Adopted as [RFC-BPR-0092](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md)
r3 ([import record](../../rfcs/assets/agent-first-run-setup-v1.LIVE_IMPORT.md);
[design frames](../../design/BPR-1747-agent-setup-v1/index.html)).

## Owner decisions (2026-10-22)

- **D1**, verbatim: "I think we ship it as a skill and make it editable by
  the user."
- **D2**, verbatim: "We probably also give the user a setting to point to
  their folders where it can find existing repo clones."
- **Q1 A** and **Q3 A**, saved in session `349862f0cb991e14b6cb57cf80653c5f`.
- **Q2, Q4, Q5, Q6 and Q7 accepted, Q8 A and Q9 A**, given in chat because
  the page's Accept/Reject and Next controls failed on those questions; the
  owner, verbatim: "I want to accept all your recommendations but it's not
  letting me hit accept or next question on most of the questions." A
  separate agent fixes the page in AgentSkills.

## Milestones

BPR-1748 to AQ8, and AQ9 to close the RFC; see the RFC's
[Milestones](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones).

## Acceptance

- The owner's answers to Q1 to Q9 are recorded here and in the RFC, with D1
  and D2, and the RFC is adopted, revised or set aside to match.
