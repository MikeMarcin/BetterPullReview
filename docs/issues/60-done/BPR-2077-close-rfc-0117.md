---
schema: "implement-rfc.card.v1"
id: "BPR-2077"
title: "Verify and close RFC-BPR-0117"
status: "Done"
rfcs: ["RFC-BPR-0117"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0005-close-explain-this-file.md","draft_title":"Close Explain this file","owner_closure":{"date":"2026-11-16","source":"owner decisions on the 2026-11-15 hygiene audit, relayed by the coordinator","decision":"close; delivered through RFC-BPR-0128 or superseded, implementer's choice","chosen":"delivered through RFC-BPR-0128, AC02 and AC03 superseded"}}
depends_on: ["BPR-2076"]
closeout_for: ["RFC-BPR-0117"]
---

# BPR-2077: Verify and close RFC-BPR-0117

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0117](../../rfcs/RFC-BPR-0117-explain-this-file-where-i-am-reading.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

Acceptance assessment for Explain this file.

## Intended verification

Judge every criterion on the composed application.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Explain this file on a file's header and context menu sends `analyze_current_file` with that file as the subject and no selection, and the turn is in the pull request's Agent tab history. | `application_explain_file_public_path`; `application_ask_menu_public_path` | Pass, delivered through RFC-BPR-0128. [[BPR-2076]] AC01 at `153e28d0f`; Explain this file is now a row of the file's Ask and the file tab's Ask menu (Q3) and sends `analyze_current_file` with the file as subject and no selection, a turn in the Agent tab | `153e28d0f`; `7b3fe6eaa` |
| **AC02:** The answer shows in a card at the file's header in Review with its references as links, and a running turn shows its activity there. | RFC-BPR-0117's Ask amendment (2026-11-13); RFC-BPR-0128 AC06 (`application_ask_report_public_path`) | **Superseded, not passed.** The amendment replaced the card at the file's header with a report card in the Agent tab and a report in a file tab, links and activity included (RFC-BPR-0128 AC06, accepted ask-v007 to report-templates-v004). The header card was removed | `7b3fe6eaa` (successor); none for the card as written |
| **AC03:** Keep as note makes a private file-level note marked with its agent origin; Dismiss removes the card and no note; Continue in Agent tab opens that turn. | Owner Reject of ask-v001 ([RFC-BPR-0128 ledger](../../verification/RFC-BPR-0128.md)) | **Superseded, not passed.** Keep as note, Dismiss and Continue in Agent tab went with Explain this file's answer bar after the owner's ask-v001 Reject ("I think just opening the agent drawer in the session and scrolling to the question and response is probably good enough"); every Ask opens the Agent tab at its turn | None (removed by owner direction) |
| **AC04:** The operation is briefed by its own built-in prompt, whose edit reaches the next Explain this file and no other prompt. | `application_agent_launch_policy`; `core_prompt_macro` | Pass. [[BPR-2076]] AC04 at `153e28d0f` (`review.explain-file`, own version and Reset, edit reaches only this operation); under RFC-BPR-0128 the `agent-tab.explain-file` action row keeps that prompt in Built-in prompts | `153e28d0f`; `7b3fe6eaa` |
| **AC05:** Opening, scrolling to or accepting a file launches no agent, and the operation writes nothing to GitHub. | `application_explain_file_public_path` | Pass. [[BPR-2076]] AC05 at `153e28d0f`: opening, scrolling and accepting launch no agent; one launch per press; nothing written to GitHub | `153e28d0f` |
| **AC06:** On a frozen build with a fixture agent, the owner accepts the control and the answer card. | Owner looks under RFC-BPR-0128 on the Fixture agent | Pass for the control, through RFC-BPR-0128's looks: the file's Ask (`BPR-2164-file-ask`, ask-v002; on the tab, ask-v006) and the report it answers in (ask-v007 to report-templates-v004). The answer card this criterion names is superseded with AC02; the card's own v001 look was rejected ("I think this gets folded into Ask") | `47dc394c8`; `bac5aab96` (frozen) |

## Hygiene audit 2026-11-15

Verdict: **UNSURE**. Explain this file was folded into RFC-BPR-0128's Ask (the owner: 'I think this gets folded into Ask'; shipped row prompt_macro_shipped.cpp:101). Question for the owner: close RFC-BPR-0117 as delivered through RFC-BPR-0128, or as superseded?
This is a read-only tracker audit: it changes no status, folder or metadata.

## Closeout 2026-11-16: delivered through RFC-BPR-0128, its answer card superseded

Owner, 2026-11-16, on the hygiene audit: close RFC-BPR-0117; whether it is
delivered through RFC-BPR-0128 or superseded is the implementer's call on the
evidence.

**Decided: delivered through RFC-BPR-0128, with AC02 and AC03 superseded.**
The entry point, its subject, its own prompt and its no-launch guarantees
(AC01, AC04, AC05) are built and kept green, and the owner accepted the file's
Ask that now carries it (AC06's control). What was not delivered is the answer
card under the file's header and its Keep as note, Dismiss and Continue
buttons (AC02, AC03): the Ask amendment replaced the card with a report card
and a report in a file tab, and the owner's ask-v001 Reject removed the answer
bar. Those two rows are recorded as superseded, never as passed, as
RFC-BPR-0025's closeout recorded its superseded criteria. Calling the whole
RFC superseded would discard the four criteria that shipped.

Nothing was re-run for this closeout; the evidence is reused unchanged from
the cited revisions. RFC-BPR-0117 is Auto-Verified for the scope reconciled
under RFC-BPR-0128.
