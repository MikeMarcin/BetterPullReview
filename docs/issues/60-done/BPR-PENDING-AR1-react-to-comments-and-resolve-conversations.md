---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AR1"
title: "React to comments and resolve conversations as GitHub's web interface does"
status: "Done"
rfcs: ["RFC-BPR-0091"]
depends_on: []
closeout_for: ["RFC-BPR-0091"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"kind":"feature","origin":"Owner, 2026-10-22, relayed by the coordinator","owner_note":"we're missing the ability to react to a comment with an emoji from GitHub's web interface and also missing the resolve conversation","draft_rfc":"docs/rfcs/assets/reactions-and-resolve-v1/RFC-DRAFT-0027-comment-reactions-and-resolve-conversation.md","draft_rfc_revision":"r1","frontend_design":"frontend-design:frontend-design loaded before the Qt surfaces","related_cards":["BPR-0657","BPR-0879","BPR-1368","BPR-1290","BPR-1471","BPR-1589"],"related_rfcs":["RFC-BPR-0008","RFC-BPR-0015","RFC-BPR-0028"],"live_rfc":"docs/rfcs/RFC-BPR-0091-comment-reactions-and-resolve-conversation.md","owner_session":"3b59571c0c371e211a07e6a1c88e5711 (closed; host stopped, frozen root removed)"}
acceptance: [{"criterion": "RA01", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "ctest -R '^(github_paged_read|github_provider|github_read_contract|github_anonymous_read_contract|native_https_anonymous_read_contract)$'", "detail": "test_reaction_read: canonical ListReactions body (ids, pull request, five reactors), groups in GitHub's order with zero ones dropped, counts from totalCount, my flag, a reactor without a login counted not named; refused: another pull request or repository, fewer nodes, another type, another identity, an unknown content, a content twice, my reaction in an empty group, six named reactors, no nodes; a schema without the fields is unsupported_capability; GHES at /api/graphql; signed out and oversized pages send nothing. test_reactions_are_read_after_the_snapshot: one page for the comment and the review with words, GitHub's answer kept per subject, a refused (rate-limited) read leaves the snapshot whole with reactions failed."}}, {"criterion": "RA02", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "ctest -R '^(github_mutation_contract|gh_cli_transport)$'", "detail": "test_reactions_go_by_graphql_exactly: AddReaction, RemoveReaction and ObserveReactionSubject compile to exact bodies, revalidate canonically, refuse a read passed off as a write, a swapped write and a body changed in flight, refuse unbound, foreign, nameless, unsafe and unknown requests, and use /api/graphql on GHES; test_recorded_reaction_and_resolution_answers parses GitHub's answers recorded on BPR-Test #132. gh_cli_transport: resolve, unresolve, the thread observation, the reaction observation and both reaction writes launch gh api graphql with the exact body on stdin; a changed body and a reply never launch."}}, {"criterion": "RA03", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "ctest -R '^(core_conversation_reactions|core_review_round_trip|core_anonymous_public_read|application_anonymous_read_host|application_review_mutation_application_host)$'", "detail": "core_conversation_reactions: the four refusals in order, and the overlay (shown at once, GitHub's answer newer than the read, a stale read does not undo it, an agreeing read settles it, a second disagreeing read is believed, a failed change restores the read). core_review_round_trip: not_permitted after the capability and before the state. core_anonymous_public_read and application_anonymous_read_host: react_to_comment is refused signed out. application_review_mutation_application_host: a reaction needs the provider's capability and a grant for that exact change."}}, {"criterion": "RA04", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "ctest -R '^live_review_mutation_provider$'", "detail": "test_an_ambiguous_change_is_confirmed_by_reading_it_back: a lost AddReaction or ResolveThread answer is followed by one read; GitHub showing the state asked confirms it (ConfirmedByReadBack), not showing it is not_confirmed; exactly one write either way; an unreadable answer is settled by the read; a cancelled write is not read back. test_a_reaction_is_observed_then_written_once: a state already as asked sends no write; a foreign comment, viewerCanReact false and a gone comment send none; an answer naming another pull request is undesignated_target with no second write. Live, a second add, a second remove and a second resolve sent nothing."}}, {"criterion": "RA05", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^(qt_conversation_cards_view|qt_workbench_window_scale_.*|qt_composed_github_scale_.*)$'", "detail": "qt_conversation_cards_view: test_reactions_show_and_toggle_by_mouse_and_keyboard (chips in order under the body; tooltip and accessible name say who and what pressing does; click and Space ask for the reaction; Add reaction focusable and named; the picker shows eight with mine checked and focused, arrows move, Return chooses and closes, Esc closes without asking; a post I may not react to offers no Add reaction and disabled chips; new counts and the pending state shown in place without rebuilding the thread) and test_reactions_render_in_light_and_dark (my chip drawn in the accent and another's not, light and dark). qt_workbench_window_scale_{100,125,200} and qt_composed_github_scale_{100,125,200} pass; renders at 100% and 200% were shown to the owner."}}, {"criterion": "RA06", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "ctest -R '^(qt_conversation_cards_view|gh_cli_transport|application_threaded_reply_contract)$'", "detail": "test_resolve_conversation_beside_reply: Resolve conversation beside Reply once offered, by keyboard too; shown resolved and pending at once and disabled until answered; closed, one line with the check, reopened without it; a denied permission offers neither the button nor the header tool. The composition offers resolution on the GitHub CLI connection and gh_cli_transport carries it; the composed course resolves and reopens."}}, {"criterion": "RA07", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "command", "command": "python3 tests/scripts/reaction_live_course.py --tool BUILD/tests/github_contract/reaction_live_course --work DIR", "detail": "application_threaded_reply_contract runs --threaded-reply-probe --reaction-probe on github-com and ghes-3.18: THREADED_REPLY_EVIDENCE:11111 and REACTION_EVIDENCE:11111111. Live: tests/scripts/reaction_live_course.py on MikeMarcin/BPR-Test #132 (closed, branch deleted): MikeMarcinTest added a thumbs up through GH_TOKEN in the child only; the owner read it, added and removed reactions on a review comment, a pull request comment and a review, resolved, resolved again (not sent) and unresolved; the second identity removed its thumbs up; 28 owner requests, REST X-RateLimit-Remaining 4980 to 4955."}}, {"criterion": "RA08", "outcome": "pass", "revision": "cdea2e55194d9bac0f231ae39b7baee8e38f5b0a", "evidence": {"kind": "observation", "detail": "Owner Accept, no note, on all three looks in attended-review session 3b59571c0c371e211a07e6a1c88e5711 (handed back at sequence 16): AR1-reaction-chips (fingerprint 9814755f), AR1-reaction-picker (eeaad6bc) and AR1-resolve-conversation (d9084309), revision cdea2e551-frozen-v001, on the frozen tests-on build of cdea2e551 (removed after the look)."}}]
---

# BPR-PENDING-AR1: React to comments and resolve conversations

## Owner request

The owner, 2026-10-22, verbatim:

> we're missing the ability to react to a comment with an emoji from
> GitHub's web interface and also missing the resolve conversation

## Phase 1 finding

Reactions are not read, shown or written anywhere. Resolve and Unresolve
were built by [[BPR-0657]] but the composition hides them on the GitHub CLI
connection and the CLI transport refuses their GraphQL documents; on a
native connection they are an unlabelled header glyph and a menu item.
GitHub's per-thread `viewerCanResolve` / `viewerCanUnresolve` are read and
unused. The design follows GitHub's web interface, so it is built without an
owner question; the choices are recorded as D1 to D6 on
[RFC-DRAFT-0027](../../rfcs/assets/reactions-and-resolve-v1/RFC-DRAFT-0027-comment-reactions-and-resolve-conversation.md)
for the owner to amend; the owner's look accepted them.

## Plan

1. Core: reaction contents, summaries, the decisions (RA03), the pending
   overlay and its projection; the resolution decision takes GitHub's
   per-thread permission.
2. GitHub adapter, read: the `ListReactions` read after the snapshot
   (RA01), admitted on native HTTPS and the CLI connection, refused signed
   out; the snapshot carries the reactions.
3. GitHub adapter, write: reaction subject observation and add/remove
   documents; resolve and reactions admitted on `gh api graphql` (RA02); the
   session observes first, skips a write already in state and confirms an
   ambiguous outcome by one read-back (RA04).
4. Application: the port, coordinator and host carry a reaction under a
   one-shot gesture grant, as a resolution does.
5. Qt and composition: chips, picker, Resolve conversation, permission
   gating, immediate counts (RA05, RA06); the fixture answers reactions and
   records changes; a composed course (RA07).
6. Live course on `MikeMarcin/BPR-Test` (RA07) and the owner's look on a
   frozen build (RA08, attended).

## Acceptance

The criteria of [RFC-BPR-0091](../../rfcs/RFC-BPR-0091-comment-reactions-and-resolve-conversation.md), each recorded in the front matter; this card also closes the RFC.

## Progress (2026-10-22)

Built on branch `worktree-agent-aab81f7a773163494` over main `3ddfbc5f8`.
Steps 1 to 6 are done, the owner's look included.

- **RA01.** `github_paged_read` (`test_reaction_read`: canonical body, binding,
  unknown type and content, foreign identity, five-reactor bound, schema
  without the fields, GHES root, signed out sends nothing) and
  `github_provider` (`test_reactions_are_read_after_the_snapshot`, a refused
  read leaves the snapshot whole).
- **RA02.** `github_mutation_contract` (exact documents, revalidation,
  tampers, GHES root, the recorded BPR-Test #132 answers) and
  `gh_cli_transport` (the six fixed documents go by `gh api graphql` with the
  body on stdin; a changed body and a reply never launch).
- **RA03.** `core_conversation_reactions`, `core_review_round_trip`,
  `core_anonymous_public_read`, `application_review_mutation_application_host`.
- **RA04.** `live_review_mutation_provider`
  (`test_a_reaction_is_observed_then_written_once`,
  `test_an_ambiguous_change_is_confirmed_by_reading_it_back`): one read back,
  never a second write; a state already as asked is not written.
- **RA05, RA06.** `qt_conversation_cards_view` (chips, picker by mouse and
  keyboard, Resolve conversation beside Reply, a denied permission, the accent
  in light and dark); `qt_workbench_window_scale_{100,125,200}` and
  `qt_composed_github_scale_{100,125,200}` pass.
- **RA07.** `application_threaded_reply_contract` now runs `--reaction-probe`
  on github-com and ghes-3.18 (`REACTION_EVIDENCE:11111111`). Live:
  `tests/scripts/reaction_live_course.py` on BPR-Test #132 (closed, branch
  deleted), owner and `MikeMarcinTest`, 28 owner requests, REST
  `X-RateLimit-Remaining` 4980 to 4955.
- **Gates.** clang-format 22 check, clang-tidy on every changed translation
  unit (no finding on a changed line), the documentation gate, the
  round-trip composition and command-registry reseals.

## Owner look

Fresh short attended-review session `3b59571c0c371e211a07e6a1c88e5711`,
state `/Users/jaeger/.mask/attended-review/owner-review-20261022-reactions-v001`,
served over Tailscale (the capability URL stays in the session state). Three
Accept/Reject questions, one per criterion: `AR1-reaction-chips`,
`AR1-reaction-picker`, `AR1-resolve-conversation`, revision
`cdea2e551-frozen-v001`, on the frozen tests-on build
`/Users/jaeger/bpr-ar1-reactions-v001.m_jymzo2` (source `cdea2e551`, launcher
check `ATTENDED_FILE_SHOWN:review.cpp`). The owner accepted all three with
no note and handed back at sequence 16. The host was stopped and the frozen
root removed with `scripts/freeze_attended_build.py remove`.
