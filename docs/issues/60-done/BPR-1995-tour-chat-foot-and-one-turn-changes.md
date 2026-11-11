---
schema: "implement-rfc.card.v1"
id: "BPR-1995"
title: "Clear the side chat foot, answer change requests with the change and run Enhance in the conversation"
status: "Done"
rfcs: ["RFC-BPR-0100"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0007-tour-chat-foot-and-one-turn-changes.md"}
depends_on: ["BPR-1999", "BPR-2000"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"Only the composer and the session line under the transcript; a replay or a moved head adds its one notice","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"AC02","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"The link in every thread names the part and sends the update; no Update button","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"AC03","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"Contracts state it; live installed Claude Code (Sonnet) carded 5/5 fixed requests in the first answer","command":"build/dev/tests/end_to_end/better_pull_review_agent_chat_composition_public_path live-tour-requests /opt/homebrew/bin/claude"}},{"criterion":"AC04","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"Accept all, Review each, Discard; settling in the tour settles the card","command":"ctest --preset dev -R qt_tour_agent_session_view"}},{"criterion":"AC05","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"Enhance sends the part's update turn in its session; a selection is replaced alone; nothing changes before Accept","command":"ctest --preset dev -R 'qt_tour_side_chat|application_review_tour_writing$|core_tour_references'"}},{"criterion":"AC09","outcome":"pass","revision":"8143e373ec464f7512c1678f265f0a0acb1f004e","evidence":{"kind":"observation","detail":"Owner accepted the lower edge (8736f969) and the revision card (e1ad5848) in attended session c46887c9608e3cab116a025a744f2379 (frozen 8143e373ec46)"}}]
---

# Clear the side chat foot, answer change requests with the change and run Enhance in the conversation

## Scope and acceptance

Implements
[RFC-BPR-0100](../../rfcs/RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md).

Reduce what sits under the transcript to the composer and the session line;
show the update link under any answer that proposed nothing, in every thread,
as the only place the update is offered; make a change request in the whole-
tour thread yield its revision in the same turn, shown in the chat as a card
with Accept all, Review each in the tour and Discard; route the application's
Enhance buttons through the part's side chat session.

Covers AC01 to AC05 and the owner's look in AC09. The session line's cost and
the Accept buttons' ordinary role come from RFC-BPR-0101. Sessions, history
and replay under RFC-BPR-0096 do not change.

This is part of `tour-conversation` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Record (2026-11-11)

Product candidate `e6e84653e`; ledger [RFC-BPR-0100](../../verification/RFC-BPR-0100.md).
Frontend-design was loaded before the Qt work.

- Lower edge (AC01): the Update button, the History and New session row and
  the standing continuation label are gone. Under the transcript: a notice
  only for a transcript replay, native continuation unavailable or a moved
  head (with Continue old revision), above the composer; the composer, whose
  attached passage is a chip inside its field; one line with the Session menu
  (its ordinal; New session, replay and earlier sessions are its items) and
  the session's cost.
- The link (AC02): "Nothing in <part> changed. Update <part> from this chat"
  under the newest answer that changed nothing, in every thread; the link is
  the only way the update is offered, plain words while it cannot be sent.
- One turn (AC03): every thread's contract answers a request with the change
  in that turn and a question in words; the whole tour's says it beside its
  schema. Live, installed Claude Code (Sonnet): 5/5 fixed requests carded in
  the first answer.
- Revision card (AC04): the whole tour's revision is a card listing its parts
  in plain words with Accept all, Review each (to the first waiting part in
  the tour) and Discard, equal buttons with their explanations in tooltips,
  drawn from the one proposal so deciding in the tour settles it.
- Enhance (AC05): the Problem's, the Solution's and a stop's Enhance open the
  part's chat and send its update turn there; a selection rides as the
  passage and `core::confine_to_passage` replaces only it. The one-shot hooks
  stay for the CLI and an empty field's draft
  (`review_tour_editor::request_field_writing`).

AC01 to AC05 pass on their fixture and composed courses; AC09 (the owner's
look at the lower edge and the card) is pending.

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
candidates accepted (`session.json`): `RFC-BPR-0100-chat-lower-edge`
(`8736f969`) and `RFC-BPR-0100-revision-card` (`e1ad5848`) for AC09, no
decision notes. A later feedback note on the lower edge, a suggestion, is
recorded verbatim in the
[ledger](../../verification/RFC-BPR-0100.md#owner-look-c46887c9-2026-11-11)
(naming a session instead of its number). Host 19967 (Tailscale child 19990) stopped with `attended_review.py stop`;
root `tour-conversation-v001.l3mto90j` removed with
`freeze_attended_build.py remove`.
