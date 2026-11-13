---
schema: "implement-rfc.card.v1"
id: "BPR-2073"
title: "What changed since my review"
status: "Done"
rfcs: ["RFC-BPR-0116"]
metadata: {"criteria":["AC01","AC02","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0001-what-changed-since-my-review.md"}
depends_on: []
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"core_returning_review; application_returning_review_latest_public_path (offered in the Pull Request menu and Agent tab; one call naming both revisions with the change; references are links); application_agent_launch_policy"}},{"criterion":"AC02","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"core_returning_review; application_returning_review_gone_public_path (unavailable with its reason in the menu and the Agent tab, no suggestion in its place, a press launches nothing)"}},{"criterion":"AC05","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"application_agent_launch_policy (own prompt, purpose review.latest-upload, version, edit reaches only it, Reset); sqlite_workbench_settings_storage (schema 30 migration); qt_settings_window (listed under Agent tab); stub echoes system=1"}},{"criterion":"AC06","outcome":"pass","revision":"98dfeb91a","evidence":{"kind":"command","detail":"application_returning_review_latest_public_path: zero launches from opening and reading, one per press, no GitHub mutation; no Seen or attention write in the path"}}]
---

# What changed since my review

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0116](../../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC01`, `AC02`, `AC05`, `AC06`.

The control beside Seen and Approve again and among the Agent tab's suggestions; the `analyze_latest_upload` send with both revisions named; the unavailable state when the earlier revision cannot be read; and its own built-in prompt, `review.latest-upload`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for when the control is offered and for the two revisions it names. The composed path on a fixture pull request with commits newer than a review, and on one whose earlier commits are gone. The launch inventory shows no agent started by opening or refreshing.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Owner look (after Done)

Rejected in attended session `ff82755e…` (decision `40b79eb4`, 2026-11-13), verbatim:

> I don't like that the drawer is panning, seems layout isn't quite right.
>
> I don't like that the top menu is the only way to access this feature, that stuff is going to be mostly ignored by users. Oh I guess it's also a suggested prompt? Okay I guess it's fine to stay if it's like that but it needs an icon in the menu bar then that make it clearly an AI feature. I dunno if we can apply our ai color up there.

Recorded only; another round reworks the surface under a new card.
