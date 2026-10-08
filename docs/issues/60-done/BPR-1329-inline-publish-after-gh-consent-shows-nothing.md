---
schema: "implement-rfc.card.v1"
id: "BPR-1329"
title: "Publishing an inline draft after the GitHub CLI confirmation shows nothing"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
acceptance: [{"criterion": "Over a GitHub CLI session, publishing a new comment from the drawer asks the confirmation once; while the request is in flight the drawer says the comment is publishing and does not offer Publish again; on success the posted thread replaces the draft and is the one open in the drawer.", "outcome": "pass", "revision": "512c2836b", "evidence": {"kind": "command", "detail": "application_inline_publish_cli_consent, course published: one publication confirmation; with the fixture's review-mutation terminal held, the draft in the drawer read \"Publishing your comment to GitHub…\" with Publish disabled; after release the fixture stored one comment with the draft's body, no draft remained and the posted thread was the open card."}}, {"criterion": "A lost result leaves the draft saying why in words, with Retry, also after it is closed and opened again; Retry asks the confirmation again and the drawer then shows exactly one posted thread.", "outcome": "pass", "revision": "512c2836b", "evidence": {"kind": "command", "detail": "application_inline_publish_cli_consent, course lost-then-retried: after the fixture lost the first answer the draft read \"Publication result is unavailable; retry is safe.\"; closed and opened again it still did, with Publish reading Retry and enabled; Retry asked the confirmation a second time; one stored comment and one posted thread, open in the drawer."}}, {"criterion": "Cancel on the confirmation sends nothing; the draft says so and still offers Publish.", "outcome": "pass", "revision": "512c2836b", "evidence": {"kind": "command", "detail": "application_inline_publish_cli_consent, course cancelled: the draft read \"Not published: you cancelled the GitHub CLI confirmation. Your draft is kept.\"; no submission; Publish enabled."}}, {"criterion": "Each of the three courses fails on the source before the fix.", "outcome": "pass", "revision": "512c2836b", "evidence": {"kind": "command", "detail": "With src/adapters/qt and github_application.cpp at 33677d37d plus only the fixture gate accessors, offscreen: published and lost-then-retried failed at \"while it is in flight, the draft says the comment is publishing\", the drawer reading \"No editable draft is selected. Return to an open pull request and select a current-head line.\" with the terminal held; cancelled failed at \"the draft says the publication was not sent\", reading the same sentence. On the pre-BPR-1212 base the same courses failed with the card's status empty while the hidden editor line held the reason."}}, {"criterion": "Focused regression selectors, the composition contracts and the documentation gate pass.", "outcome": "pass", "revision": "512c2836b", "evidence": {"kind": "command", "detail": "ctest in the worktree's dev build at the fix commit: architecture_boundaries, test_network_isolation_manifest, qt_composed_github_scale_100, qt_conversation_cards_view, qt_workbench_conversation_reveal, qt_review_draft_editor, application_review_round_trip_composition_contract, application_threaded_reply_contract, application_label_authoring_contract, application_published_file_switch_contract and application_inline_publish_cli_consent passed; qt_workbench_window_scale_100, qt_composed_inline_drawer_scale_100 and application_device_flow_cli_choice passed one rebase earlier with the same product change. composition_contract_tests.py passed (8 tests, 774 s) before the BPR-1212 rebase, run outside ctest because its 600 s row timeout was reached while another worktree used the shared CPM cache. verify_documentation.py: DOCUMENTATION_OK."}}]
metadata: {"found_by":"owner report on the private macOS 0.1.5 prerelease, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1329--BPR-1329--82d67ecdafa9.json","related_cards":["BPR-1326","BPR-1212","BPR-1290","BPR-0797"]}
---

# Publishing an inline draft after the GitHub CLI confirmation shows nothing

Owner, 2026-10-08, in chat:

> Or perhaps it did try to publish the comment and just failed to do so. It
> popped up a warning about publishing with my gh account and I allowed it but
> nothing seemed to happen

Context: on the private macOS 0.1.5 prerelease the owner published an inline
draft comment from the Comments drawer over a GitHub CLI session. Before that,
the draft had not shown in the drawer until they clicked its line; that is
[[BPR-1326]], fixed separately and not touched here. After the per-publication
"Confirm GitHub CLI publication" question, answered Yes, the owner could not
tell whether the comment was published.

## What the owner's case shows

- **Local diagnostics.** No 0.1.5 installation and no diagnostic log from
  2026-10-08 exist on the machine this card was worked on. The default data
  root's newest session log is from 2026-10-04 and the profiles' newest are
  from 2026-09-27, so the run happened on another machine and its log was not
  read. A publication attempt writes one `create_review_comment` diagnostic
  event (operation and outcome, no body or token); a Settings > Diagnostics
  export from that machine would show whether the POST was made and how it
  ended.
- **BPR-Test.** Read-only `gh api` on `MikeMarcin/BPR-Test` shows one review
  comment on 2026-10-08, on #41 at 18:20:01Z, and that is the BPR-1212 agent
  probe. No pull request there received a comment from the owner that day. If
  the owner published to BPR-Test, it did not post; no other repository was
  read.

So whether the owner's comment posted is not known from here. What is known is
that, from the drawer, nothing the publication came to could have been seen.

## Cause

The drawer's Publish runs the draft editor's own Publish. Everything that
Publish came to was written only to the draft editor's status line, which sits
on the inline composition page the drawer does not show:

- while the request was in flight the drawer said nothing of it; since
  [[BPR-1212]] (compose in the drawer) it said "No editable draft is
  selected. Return to an open pull request and select a current-head line.",
  the disabled Publish's tooltip, because a publishing draft is not editable;
- a failure or an uncertain result ("Publication result is unavailable; retry
  is safe.", a server refusal, a moved head, an incompatible body) stayed on
  the hidden page, and the editor's Retry was not offered in the drawer;
- some refusals said nothing anywhere: Cancel on the confirmation, the
  session or target changing while it was open, a draft still being saved,
  and a draft that changed before it was sent.

On success the drawer already replaces the draft with the posted thread, open
at the new post (`select_published_conversation`). Over gh that takes the
provider's observations, the POST, its reconciliation and a full pull request
refresh, each a separate `gh` process; with no word in the drawer meanwhile,
an in-flight success also looks like nothing.

## Scope

- `review_draft_editor` keeps what the last Publish of its draft came to
  apart from its other status messages (`publication_status`,
  `publication_retry_offered`, `present_publication_started`,
  `present_publication_refusal`) and tells its presentation observer.
- The workbench window gives that to the draft written in the drawer as its
  notice (`threadDraftStatus`, under Publish), ahead of the disabled
  Publish's reason, and tells it when the editor offers a retry: Publish then
  reads Retry and stays enabled although the draft is no longer editable.
- The composition says Publishing once the mutation host has taken the
  request, and names a reason for every Publish it stops before sending,
  including the confirmation's Cancel and a session that changed while it was
  asked. The confirmation's gate and question are unchanged.
- Fixture-only accessors hold and release the fixture's review-mutation
  decision gate, so a course can see the drawer while a publication is in
  flight.
- The round-trip composition pins are resealed for `github_application.cpp`
  and the fixture-access header.

Out of scope: [[BPR-1326]] (saved drafts in the drawer); replies written in
place, whose publication status has the same gap; the live gh mutation
provider, which no fixture course reaches.

Note for the owner of [[BPR-1326]]'s saved-draft notes: a note for another
line's draft (`draft:unpublished:<id>`) opens through `edit_draft`, and its
Publish, where one is shown, runs the editor's Publish, which is the selected
line's draft. This card does not change that.

## Acceptance

- Over a GitHub CLI session, publishing a new comment from the drawer asks
  the confirmation once; while the request is in flight the drawer says the
  comment is publishing and does not offer Publish again; on success the
  posted thread replaces the draft and is the one open in the drawer.
- A lost result leaves the draft saying why in words, with Retry, also after
  it is closed and opened again; Retry asks the confirmation again and the
  drawer then shows exactly one posted thread.
- Cancel on the confirmation sends nothing; the draft says so and still
  offers Publish.
- Each of the three courses fails on the source before the fix.
- Focused regression selectors, the composition contracts and the
  documentation gate pass.

## Implementation and verification — 2026-10-08

Fix commit 512c2836b. The draft in the drawer shows the editor's publication
status as its notice under Publish, in the place and type the drawer already
uses for Publish's disabled reason; Publish reads Retry when the editor
offers one. The words come from the editor, so Discuss's full composer and
the drawer never disagree. Worked on the pre-BPR-1212 drawer first (a status
line on the draft note's open card); that part was dropped on the rebase
because a draft note now opens as the composer.

Pending for the owner's own case: whether the 0.1.5 publication posted. A
Settings > Diagnostics export from the machine that ran it would show the
`create_review_comment` event and its outcome.
