---
schema: "implement-rfc.card.v1"
id: "BPR-2078"
title: "Suggest the change from a draft comment"
status: "Parked"
rfcs: ["RFC-BPR-0118"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0006-suggest-the-change-from-a-draft-comment.md"}
depends_on: []
parked_on: "human-review"
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"core_suggest_change; application_suggest_change_public_path (offered on the draft field menu where the lines take a suggestion; one call through workshop_comment with suggest_change and exactly the lines)"}},{"criterion":"AC02","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"application_suggest_change_public_path: the applying reply opens in the head pane; the draft body is unchanged until Keep change"}},{"criterion":"AC03","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"core_suggest_change (every check, head moved first); course: one byte off and a sentence refused with their reasons, never shown in the code, reply readable as text"}},{"criterion":"AC04","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"course: Keep change adds the block with the words as written; Undo restores the body exactly; no submission or target observation"}},{"criterion":"AC05","outcome":"pass","revision":"c74171d0d","evidence":{"kind":"command","detail":"application_agent_launch_policy; sqlite_workbench_settings_storage (31 to 32); qt_settings_window; stub reads system=1"}},{"criterion":"AC06","outcome":"pending","revision":"c74171d0d","evidence":{"kind":"observation","detail":"No look filed: the coordinator withheld it until the owner Ask model settles the entry (2026-11-12)"}}]
---

# Suggest the change from a draft comment

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0118](../../rfcs/RFC-BPR-0118-turn-a-draft-comment-into-an-exact-suggested-change.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

The control on an eligible draft comment; the seventh comment-workshop operation; the structured reply and core's acceptance checks; the hand-over into RFC-BPR-0090's in-pane suggestion edit; the refusal and its reason; Undo; and its own built-in prompt, `review.suggest-change`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests over every acceptance check: a differing range, one differing byte in the stated current text, an identical replacement, an out-of-bounds one and a change GitHub cannot take. A fixture agent producing an applying reply, a refused one and a reply that arrives after the head moved. The owner's look (AC06) is an attended review on a frozen build.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Hygiene audit 2026-11-15

Verdict: **STILL-VALID**. Built (right-click entry; RFC-BPR-0118 ledger). AC06's look waited only for the Ask model, and RFC-BPR-0128 (Auto-Verified) keeps the entry as it is, so the look can be filed now.
This is a read-only tracker audit: it changes no status, folder or metadata.
