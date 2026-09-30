---
schema: "implement-rfc.card.v1"
id: "BPR-1245"
title: "Fifteen agent composition courses predate RFC-BPR-0023's right-side drafts and private runs"
status: "Backlog"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-0587","BPR-1211","BPR-1139","BPR-1238"]}
---

# BPR-1245: Fifteen agent composition courses predate RFC-BPR-0023's right-side drafts and private runs

## Observation

At `main` 83edd361b on macOS arm64 (dev preset), these rows fail; the
BPR-1211 implementer recorded the same set at its baseline d25f17b69, and the
Windows station records two of them on [[BPR-1139]]:

| Rows | Failure |
|---|---|
| `application_agent_chat_composition_contract` | `agent-chat-composition-failure:chat-reply-promotion-target` |
| `application_agent_chat_stale_contract` | `panel-status:Not started: side chat refused: session_stale (head_moved)` |
| `application_agent_chat_{continued,anchored_continuation}_contract` | exit 1, no failure marker |
| `application_agent_finding_{composition,full_agent,finding_host_construction_cut,workshop_press,workshop_construction_cut,mcp_construction_cut,chat_panel_construction_cut,queue_construction_cut,dif_context}_contract` | `local-draft failed=draft-shown draft-shown=false` |
| `application_agent_invocation_{composition,workspace_diagnostic}_contract` | `Choose immutable revisions or findings in Private history, then Ask agent using selected sources.` |

## Diagnosis so far

Two product rules from f55e39e72 ("Finish BPR-0587", RFC-BPR-0023, closed and
owner-accepted) are enough to explain most rows, but each course still needs
its own reading:

- A new draft is offered only on a right-side line. The finding course reaches
  its line with `navigate_change(next)`, and the reversed or first change can
  be a left-side line, so no draft appears. [[BPR-1238]] met the same rule in
  the defect probe.
- `launch_agent_invocation` refuses a turn whose context includes notes or a
  draft; private runs now choose immutable revisions in Private history. The
  invocation courses include them.

The chat courses' promotion target and stale-session refusals were not
diagnosed. Bisecting as BPR-1238 did (a worktree build of the app between
beddf1f55, where the composed rows passed, and d25f17b69) will tell whether
they share the cause.

## Outcome

Each course exercises the accepted RFC-BPR-0023 flow: a right-side line for a
new draft, and a private run from selected immutable sources where it asks an
agent. Any product defect found on the way is fixed where it lives, not
relaxed in the course.

## Direct acceptance

- The fifteen rows pass on macOS.
- The card names each course's cause.
