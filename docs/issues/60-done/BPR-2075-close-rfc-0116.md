---
schema: "implement-rfc.card.v1"
id: "BPR-2075"
title: "Verify and close RFC-BPR-0116"
status: "Done"
rfcs: ["RFC-BPR-0116"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0003-close-returning-pull-request.md","draft_title":"Close the returning pull request questions","owner_closure":{"date":"2026-11-16","source":"owner decisions on the 2026-11-15 hygiene audit, relayed by the coordinator","decision":"close; delivered through RFC-BPR-0128 or superseded, implementer's choice","chosen":"delivered through RFC-BPR-0128"}}
depends_on: ["BPR-2073", "BPR-2074"]
closeout_for: ["RFC-BPR-0116"]
---

# BPR-2075: Verify and close RFC-BPR-0116

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0116](../../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for What changed since my review and Was this addressed?.

## Intended verification

Judge every criterion on the composed application with an installed agent as well as the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** What changed since my review is offered only when the pull request has commits newer than the reviewer's last review or Seen; a press sends `analyze_latest_upload` with both revisions named in the brief; and the reply's code references open as links. | `application_returning_review_latest_public_path`; `application_ask_menu_public_path`; `application_fixture_agent_every_ask` | Pass, delivered through RFC-BPR-0128. [[BPR-2073]] AC01 at `98dfeb91a`; What changed since my review is the review bar's Ask row and answers as a report card and report with its references as links ([RFC-BPR-0128 ledger](../../verification/RFC-BPR-0128.md), ask-menu and reports milestones); owner Accept `BPR-2164-review-bar-ask` (ask-v002) | `98dfeb91a`; `7b3fe6eaa` |
| **AC02:** When the earlier revision's material cannot be read, the control is unavailable with that reason, and no whole-pull-request answer is labelled as what changed. | `application_returning_review_gone_public_path`; `core_quick_ask` | Pass. [[BPR-2073]] AC02 at `98dfeb91a`; under the Ask model the row is greyed with its typed `quick_ask_reason` as tooltip and status tip | `98dfeb91a`; `7b3fe6eaa` |
| **AC03:** Was this addressed? on a thread sends `evaluate_proposed_fix` with the conversation as its subject, with no selection required, and its brief carries the attached lines as they were and as they are. | `application_returning_review_addressed_public_path`; `application_ask_addressed_public_path` | Pass, as widened by the Ask amendment (any thread, a file, the pull request). [[BPR-2074]] AC03 at `98dfeb91a`; [[BPR-2165]]: `evaluate_proposed_fix` with the thread as subject, no selection, the lines as they started and as they are | `98dfeb91a`; `7b3fe6eaa` |
| **AC04:** The answer leads with addressed, partly, not addressed or cannot tell and is shown as the agent's claim; the thread is not resolved, no defect's disposition changes and nothing is posted. | `core_returning_review`; `application_ask_addressed_public_path`; live installed-agent run (`ask-menu-addressed:pass`) | Pass. The claim is one chip, Addressed, Partly, Not addressed or Unclear, in the thread's header; nothing resolved, posted or published; owner Accept `BPR-2165-claim-chips` (ask-v002) | `98dfeb91a`; `7b3fe6eaa` |
| **AC05:** Each operation is briefed by its own built-in prompt with its own editor, default, version and Reset; an edit reaches that operation's next brief and no other prompt. | `application_agent_launch_policy` | Pass. [[BPR-2073]] and [[BPR-2074]] AC05 at `98dfeb91a` (each operation's own prompt, version and Reset); under RFC-BPR-0128 the action rows keep their prompts in Built-in prompts (Q13) and an own macro cannot claim them | `98dfeb91a`; `7b3fe6eaa` |
| **AC06:** Opening, refreshing and returning to a pull request with new commits launches no agent, and neither operation writes to GitHub or changes attention state. | `application_returning_review_latest_public_path`; `application_ask_menu_public_path` | Pass. [[BPR-2073]] AC06 at `98dfeb91a`: opening, refreshing and returning launch no agent, no GitHub write, no attention change; a quick ask starts only on a press | `98dfeb91a`; `7b3fe6eaa` |
| **AC07:** On a frozen build serving a pull request with commits newer than a review and a commented thread, the owner accepts both controls and their answers. | Owner looks under RFC-BPR-0128 on the returning-review-v1 fixture (newer commits than a review, commented threads) | Pass through RFC-BPR-0128's looks. The v001 looks of both controls were rejected (session `ff82755e`); the owner then accepted their Ask-model successors: review-bar, file and thread Ask and the claim chips (ask-v002), Was this addressed? at the pull request's many threads (Q5, ask-v008), and the reports What changed answers in (report-templates-v003, v004) | `47dc394c8`; `bac5aab96` (frozen) |

## Hygiene audit 2026-11-15

Verdict: **UNSURE**. RFC-BPR-0116's entry points were folded into RFC-BPR-0128's Ask model (Auto-Verified 2026-11-14; BPR-2165 delivers Was this addressed? on any thread). Question for the owner: close RFC-BPR-0116 as delivered through RFC-BPR-0128, or as superseded?
This is a read-only tracker audit: it changes no status, folder or metadata.

## Closeout 2026-11-16: delivered through RFC-BPR-0128

Owner, 2026-11-16, on the hygiene audit: close RFC-BPR-0116; whether it is
delivered through RFC-BPR-0128 or superseded is the implementer's call on the
evidence.

**Decided: delivered through RFC-BPR-0128.** The Ask amendment of 2026-11-13
rewrote no criterion of this RFC: it moved where the two controls stand and
widened Was this addressed? to any thread, and said the change "is built and
verified under RFC-BPR-0128". Every criterion has its evidence, in the table
above: [[BPR-2073]] and [[BPR-2074]]'s courses at `98dfeb91a`, the same
courses kept green through RFC-BPR-0128 (its integration pass at `7b3fe6eaa`
lists `application_returning_review_{latest,addressed,gone}_public_path` as
"unchanged in what they send"), [[BPR-2165]] for the widened Was this
addressed?, and the owner's accepts of the controls and answers in their Ask
form. Nothing the RFC asked for was dropped, so superseded would misdescribe
it. [[BPR-2074]] closes as superseded because its own look (AC07 on that
card) was answered by the Ask-model controls, not by the card's control.

Nothing was re-run for this closeout; the evidence is reused unchanged from
the cited revisions. RFC-BPR-0116 is Auto-Verified.
