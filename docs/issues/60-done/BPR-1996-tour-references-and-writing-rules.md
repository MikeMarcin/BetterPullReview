---
schema: "implement-rfc.card.v1"
id: "BPR-1996"
title: "Link and publish code references in tour text, and add the citation and moved-code rules"
status: "Done"
rfcs: ["RFC-BPR-0100"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0008-tour-references-and-writing-rules.md"}
depends_on: ["BPR-1998"]
acceptance: [{"criterion":"AC06","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"Links and 12-line snippets in the stop reader; published as commit-pinned permalinks in the form established on BPR-Test #43 (both comment kinds embed); known limit: an edited published stop republished by an update keeps references as written","command":"ctest --preset dev -R 'qt_tour_side_chat|application_review_tour_publication_native'"}},{"criterion":"AC07","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"Both rules in the drafting prompt, the conversation and the stop-comment hook; guide v2, tour-stop-comment-v2","command":"ctest --preset dev -R 'application_tour_agent_conversation|application_writing_delivery'"}},{"criterion":"AC08","outcome":"pass","revision":"8143e373ec464f7512c1678f265f0a0acb1f004e","evidence":{"kind":"observation","detail":"Owner accepted the flag-move-v1 drafted tour (1180b2e4) in attended session c46887c9608e3cab116a025a744f2379"}}]
---

# Link and publish code references in tour text, and add the citation and moved-code rules

## Scope and acceptance

Implements
[RFC-BPR-0100](../../rfcs/RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md).

Apply the shared reference rules to stop text, the Problem and the Solution;
show a reference alone on its line as a snippet; publish references as commit-
pinned permalinks in the form established on MikeMarcin/BPR-Test; report
unresolvable references before publication; add the citation rule and the
moved-code rule to the drafting prompt, the conversation contract and the
stop-comment hook and advance the guide and hook versions.

Covers AC06 to AC08. AC08 is the owner's judgement of a drafted tour on a
fixture pull request that moves an unchanged path behind a flag.

This is part of `tour-conversation` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Record (2026-11-11)

Product candidate `e6e84653e`; ledger [RFC-BPR-0100](../../verification/RFC-BPR-0100.md).
Frontend-design was loaded before the Qt work.

- In the application (AC06): `core::rewrite_tour_references`, on the
  RFC-BPR-0101 recogniser, makes a stop's reference a link to its lines in
  Review and one alone on its line the permalink of at most 12 of its lines at
  the tour's pinned head, drawn by the BPR-1802 snippet box, with Show in
  Review. The Overview's Problem and Solution link their references.
- On GitHub (AC06): established on BPR-Test #43 that review comments and
  conversation comments both draw a bare permalink as a snippet box. A new
  publication writes an inline reference as a Markdown link to the
  commit-pinned permalink and a lone one as the bare permalink; references
  naming no file of the pull request stay as written and the preview says so.
  The plan keeps each stop's authored text (recovery record v7) and a
  published stop's baseline the text it came from, so a published tour is not
  seen as changed. Known limit: an edited published stop republished by a tour
  update keeps its references as written.
- Rules (AC07): citation and moved-code rules in the guide, now
  `bpr.tour-guide.v2` (drafting and conversation), and in the stop-comment
  hook, now `tour-stop-comment-v2`.
- AC08: fixture `flag-move-v1` and the opt-in `live-tour-draft-flag-move`
  course; its Sonnet draft keeps the moved loader to "the old loader body
  moved into its own file". The owner's judgement is pending.

Filed, not waited on (URL in `../serve.log` beside the state): session
`c46887c9608e3cab116a025a744f2379`, state
`/Users/jaeger/.attended-review-state/tour-conversation-v001-2026-11-11/state`,
cursor 9, host PID 19967 (Tailscale child 19990), candidates ready and
protected: `RFC-BPR-0100-chat-lower-edge`, `RFC-BPR-0100-revision-card`
(AC09) and `RFC-BPR-0100-flag-move-draft` (AC08). Frozen root
`/Users/jaeger/tour-conversation-v001.l3mto90j` from `8143e373ec46` (app
SHA-256 `ca7a6f81fe78…`, launcher check reached `ATTENDED_TOUR_REVISION_SHOWN`).

## Owner look (2026-11-11)

Session `c46887c9608e3cab116a025a744f2379` handed back with all three
candidates accepted (`session.json`): `RFC-BPR-0100-flag-move-draft`
(`1180b2e4`) for AC08, no note. Host 19967 (Tailscale child 19990) stopped with `attended_review.py stop`;
root `tour-conversation-v001.l3mto90j` removed with
`freeze_attended_build.py remove`.
