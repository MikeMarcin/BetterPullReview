---
schema: "implement-rfc.card.v1"
id: "BPR-2076"
title: "Explain this file, answered in Review"
status: "Done"
rfcs: ["RFC-BPR-0117"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0004-explain-this-file-in-review.md","superseded_by":["RFC-BPR-0128","BPR-2163","BPR-2164"],"owner_closure":{"date":"2026-11-16","source":"owner decisions on the 2026-11-15 hygiene audit, relayed by the coordinator","decision":"close as superseded"}}
depends_on: []
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"153e28d0f","evidence":{"kind":"command","detail":"core_explain_file, core_agent_chat_scope; application_explain_file_public_path: the header control and the tab menu's Explain This File each send one analyze_current_file call about src/engine/renderer.cpp (subject=1) with nothing selected, each a turn in the Agent tab history"}},{"criterion":"AC02","outcome":"pass","revision":"153e28d0f","evidence":{"kind":"command","detail":"application_explain_file_public_path: answer in the card at the file's header with links, running turn shown (saw-running=1); qt_explain_file_card_scale_100/150/200"}},{"criterion":"AC03","outcome":"pass","revision":"153e28d0f","evidence":{"kind":"command","detail":"application_explain_file_public_path: Keep as note makes a private file-level note beginning 'Agent: Claude explained this file.' (Go to note); Continue in Agent tab opens the tab; Dismiss hides the card, no note, turn kept; qt_explain_file_card_scale_*"}},{"criterion":"AC04","outcome":"pass","revision":"153e28d0f","evidence":{"kind":"command","detail":"application_agent_launch_policy (review.explain-file own prompt, version, fresh tool-free system prompt, edit isolation, Reset); stub echoes system=1; sqlite_workbench_settings_storage schema 31; qt_settings_window"}},{"criterion":"AC05","outcome":"pass","revision":"153e28d0f","evidence":{"kind":"command","detail":"application_explain_file_public_path: opening, opening a file and scrolling launch no agent; one launch per press; review mutation evidence unchanged"}},{"criterion":"AC06","outcome":"pending","revision":"153e28d0f","evidence":{"kind":"artifact","detail":"Attended session 0002b9fad401fad26fe5fa36206d539b (state /Users/jaeger/.attended-review-state/explain-file-v001-2026-11-12/state), candidate BPR-2076-explain-this-file on frozen root /Users/jaeger/explain-file-v001.4gr0t3jw"}}]
---

# Explain this file, answered in Review

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0117](../../rfcs/RFC-BPR-0117-explain-this-file-where-i-am-reading.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

The control on the file header and its context menu; `analyze_current_file` taking the open file as its subject, with core's rule and `docs/ux/ai-experience.md` changed to say so; the answer card at the file header with Keep as note, Continue in Agent tab and Dismiss; and its own built-in prompt, `review.explain-file`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for the changed subject rule. Qt tests at the supported scales for the card and its three actions; the composed path shows the turn in the Agent tab's history. The owner's look (AC06) is an attended review on a frozen build with a fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Owner verdict (2026-11-13)

Session `0002b9fa…`: **Reject**, "I think this gets folded into Ask." Parked pending the owner's Ask redesign ([RFC-BPR-0117 ledger](../../verification/RFC-BPR-0117.md)).

## Hygiene audit 2026-11-15

Verdict: **SUPERSEDED**. By RFC-BPR-0128's Ask (Auto-Verified): the owner rejected this card's look with 'I think this gets folded into Ask', and Explain this file is now a shipped quick ask (prompt_macro_shipped.cpp:101).
This is a read-only tracker audit: it changes no status, folder or metadata.

## Owner decision 2026-11-16: closed as superseded

Owner, 2026-11-16 (on the hygiene audit, relayed by the coordinator): close
this card as superseded. The board has one terminal lane, so a superseded card
sits in Done with `metadata.superseded_by` (as [[BPR-1353]] does); it was not
delivered as written.

By [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md)'s
Ask (Auto-Verified 2026-11-14): the owner rejected this card's look with "I
think this gets folded into Ask", and Explain this file is now a quick ask in
the file's Ask and the file tab's menu (`prompt_macro_shipped.cpp`), built
under [[BPR-2163]] and [[BPR-2164]]. Its answer is a report card and a report
in a file tab; the answer card under the header, with Keep as note and
Dismiss, was removed after the ask-v001 look. AC01 to AC05 stay recorded
against `153e28d0f`; AC06 was never asked again on this card. RFC-BPR-0117
closes on [[BPR-2077]].
