---
schema: "implement-rfc.card.v1"
id: "BPR-0657"
title: "Threaded replies and resolve through the mutation port"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"A reply published from a card appears under its thread in the recorded fixture's projection; Resolve and Unresolve change the thread's state through the port and refuse exactly the cases above","outcome":"pass","revision":"8651e1e1","evidence":{"kind":"observation","detail":"application_threaded_reply_contract (new, github-com and ghes-3.18): Reply on a thread's card starts a draft answering PRRT_fixture_1 (the editor names it), publication reaches the fixture provider as a reply, the reply is under its thread on the next read, Resolve on the card resolves the thread through the port and the thread comes back resolved (THREADED_REPLY_EVIDENCE:11111); with the fixture mutated to drop replies from its overlay the course fails. core_review_round_trip pins the three refusals (anonymous read, capability absent, already in the state asked for) and the proposal carrying its thread; application_review_mutation_application_host pins a resolution refused without the capability, for a nameless thread and for a grant that does not permit it, and admitted otherwise with the exact thread bound to the target and actor; github_mutation_contract pins the reply and resolution GraphQL documents, their tampers, exact answers and mismatches."}},{"criterion":"application_review_mutation, application_review_submission_contract, the recorded GitHub contracts and the Qt selectors for the cards and the editor pass","outcome":"pass","revision":"8651e1e1","evidence":{"kind":"observation","detail":"application_review_mutation, application_review_submission_contract, application_review_mutation_application_host, github_mutation_contract, github_read_contract, live_review_mutation_provider, live_review_mutation_restart_recovery, core_review_round_trip, qt_conversation_cards_view, qt_review_draft_editor, qt_github_conversation_panel, qt_workbench_window_scale_{100,125,200}, qt_composed_github_scale_100, application_github_public_path_{github_com,ghes_3_18,uiforetw_74}, application_public_path, application_review_ledger_composition_contract, application_defect_composition_contract, application_review_round_trip_composition_contract (repinned in two passes; its literal inventory kept by minting gesture grants in one helper) and application_composition_contract pass; test_network_isolation_manifest passes with the new pin; check_boundaries, clang-format --Werror and verify_documentation.py pass; strict tidy passes on every changed translation unit except pre-existing findings on untouched lines, named on the card."}}]
---

# Threaded replies and resolve through the mutation port

RFC-BPR-0028's cards and feed offer Reply and Resolve on a thread. The
review mutation port publishes a comment at a line
(`review_mutation_request` carries a `review_publication_proposal` and its
anchor) and knows nothing of the thread it answers, so a "reply" published
today lands as a new comment on the line rather than in the thread; resolve
and unresolve exist only as capabilities (`github_capability::resolve_thread`)
with no intent, port method or host path. [BPR-0648](BPR-0648-aligned-comment-cards-in-read.md)
therefore offers "Comment here" on a card, which starts a draft on the
thread's line, and no Resolve.

## Scope

- Core: a reply proposal that names the thread (and the comment) it answers,
  and resolve/unresolve intents with their refusals (capability absent,
  anonymous read, thread already in the state asked for).
- Application and GitHub adapter: the mutation port carries the reply
  linkage (`in_reply_to`) and the resolve mutations; the recorded contracts
  cover both against the fixture; no live mutation outside the designated
  test repository.
- Qt: the card's action becomes Reply, Resolve appears beside it and in the
  Discuss feed (BPR-0649), and the draft editor names the thread it replies
  to.
- Owned paths: `src/core` review round trip, `src/application` mutation
  port and hosts, `src/adapters/github`, `src/adapters/qt` cards, editor and
  feed, their focused tests and the mutation contracts.

## Acceptance

- A reply published from a card appears under its thread in the recorded
  fixture's projection; Resolve and Unresolve change the thread's state
  through the port and refuse exactly the cases above.
- `application_review_mutation`, `application_review_submission_contract`,
  the recorded GitHub contracts and the Qt selectors for the cards and the
  editor pass.

## Notes (2026-09-07)

- **Reply linkage.** `core::review_comment_draft` and
  `review_publication_proposal` carry `ReplyToThread`, the provider's
  opaque thread identity; `propose_publication` copies it and
  `idempotency_key_for` hashes it, so a reply and a root at the same line
  with the same body are different publications. GitHub addresses a thread
  only by the identity the read side reports, so a reply goes by GraphQL:
  `compile_review_comment_request` emits the reserved `ReplyToThread`
  document (`addPullRequestReviewThreadReply`) at the GraphQL root when the
  proposal names a thread and keeps the REST form otherwise;
  `parse_review_comment_response` accepts the 200 envelope with the
  identity checks a REST comment gets (path, commit, line, body, author,
  `viewerDidAuthor`). The recovery record and reconciliation are the ones a
  root comment has. A reply is drafted from whichever line was selected
  (its thread anchors it), so the host admits a left-side selection for a
  reply and still holds a root comment to the right side.
- **Resolve.** `core::decide_thread_resolution` refuses, in order, an
  anonymous read, an absent capability (never claimed or denied) and a
  thread already in the state asked for. The mutation port gained
  `resolve_thread`; the GitHub adapter compiles `ResolveThread` /
  `UnresolveThread` GraphQL documents and accepts only the exact thread in
  the state asked for; the live provider resolves through the leased
  transport and now says `ResolvesThreads`. The mutation host's `resolve`
  runs under the same grants a publication gets, over a thread scope (the
  session, generation, target and actor, and the thread): a testing grant
  permits any thread of its target, a user one-shot issued for a resolution
  permits only that one, and one issued for a publication permits none; a
  refused request (no capability, nameless thread, unpermitted grant)
  terminates before any effect. A resolution neither supersedes nor is
  superseded by a publication.
- **Qt and composition.** A card's Reply names its thread to the
  composition before selecting it and selects the thread's line as a click
  would ("Comment here" now does the same); the draft that follows answers
  the thread and the editor says so while it does. Resolve, or Unresolve on
  a resolved thread, appears on every thread card once the provider and
  the server both resolve threads; the gesture carries the projected state
  and the server's capability, the composition adds the session's
  anonymity and decides through the core, names a refusal on the panel's
  notice, and on the provider's answer re-reads the pull request so the
  thread's state comes back the way every fact does.
- **Fixture.** The public-path fixture's mutation provider records replies
  and resolutions and overlays them on the recorded review-thread answer;
  with `--threaded-reply-probe` the fixture replays its read script from
  the repository read on a refresh (`strict_recorded_transport::skip_until`),
  which is what lets a course refresh at all, and its provider resolves
  threads (`github_public_path_fixture_options::ResolvesThreads`). Off the
  course the provider still says it does not, so the round-trip contract's
  capability-fallback evidence (the editor naming the degradation) keeps
  its meaning. The grant a gesture publishes or resolves under is minted in
  one helper, keeping the round-trip contract's literal inventory (one
  fixture grant factory call, two absent-grant starts).
- **Evidence.** `github_mutation_contract` (reply and resolution documents,
  tampers, exact answers and mismatches), `application_review_mutation_application_host`
  (resolution admission, refusals and terminals; a reply from a left-side
  selection), `core_review_round_trip` (the proposal carries the thread;
  the three refusals), `qt_conversation_cards_view` (Resolve and Unresolve
  offered by state), `qt_review_draft_editor` (the thread named only while
  answered), and the new `application_threaded_reply_contract`: over
  github-com and ghes-3.18 a reviewer presses Reply on a thread's card, the
  draft answers it, publishes, the reply is under its thread on the next
  read, Resolve on the card resolves it and the thread comes back resolved
  (`THREADED_REPLY_EVIDENCE:11111`). With the fixture mutated to drop
  replies from its overlay the course fails at `reply-not-under-thread`.
- **Not done here.** The fixture's threads sit past the compared file's
  lines, so a reply in the course is drafted from the comparison's own
  selection rather than the thread's line; a fixture with a thread on a
  current line would exercise `select_thread_line`. The public preview's
  refusal of Resolve is decided by the core and named on the panel but is
  not driven by a course.
- **Tidy.** Strict tidy passes on every translation unit this card
  changed except three whose findings predate it and are not on its lines:
  the glaze option members in `github_mutation_contract.cpp`,
  `github_read_contract.cpp` and `github_public_path_fixture.cpp`
  (`readability-identifier-naming`), a use-after-move in the fixture's
  replay constructor and `modernize-use-emplace` in
  `large_review_fixture.hpp`.
